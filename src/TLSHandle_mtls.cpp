/*
 * Copyright (C) 2021 Tetsuya Isaki
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "sayaka.h"
#include "TLSHandle_mtls.h"
#include <cstdarg>
#include <cstdlib>
#include <cstring>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/debug.h>
#include <mbedtls/entropy.h>
#include <mbedtls/error.h>
#include <mbedtls/net_sockets.h>

// ローカルのデバッグモード (on / off のみ)
//#define DEBUG 1

// mbedTLS のデバッグレベル (1でも結構多いし、2 でほぼ読めないくらい)
// 0 .. No debug
// 1 .. Error
// 2 .. State Change
// 3 .. Informational
// 4 .. Verbose
#define MTLS_DEBUG_LEVEL (0)

#if defined(DEBUG)
#define TRACE(fmt, ...) do { \
	struct timeval tv; \
	TRACE_tv(&tv, fmt, ## __VA_ARGS__); \
} while (0)
#define TRACE_tv(tvp, fmt, ...) do { \
	gettimeofday(tvp, NULL); \
	trace(tvp, __func__, fmt, ## __VA_ARGS__); \
} while (0)
#define ERROR(fmt, ...)	TRACE(fmt, ## __VA_ARGS__)
#else
#define TRACE(...)		(void)0
#define TRACE_tv(...)	(void)0
#define ERROR(...)		fprintf(stderr, __VA_ARGS__)
#endif

#if defined(DEBUG)
// デバッグ表示
static void trace(struct timeval *tv, const char *func, const char *fmt, ...)
	__printflike(3, 4);
static void
trace(struct timeval *tv, const char *funcname, const char *fmt, ...)
{
	va_list ap;

	fprintf(stderr, "[%02d:%02d.%06d] %s() ",
		(int)((tv->tv_sec / 60) % 60),
		(int)((tv->tv_sec     ) % 60),
		(int)(tv->tv_usec),
		funcname);

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
}
#endif

// グローバルコンテキスト
struct mtls_global_ctx
{
	bool initialized;
	mbedtls_ctr_drbg_context ctr_drbg;
	mbedtls_entropy_context entropy;
};
static struct mtls_global_ctx gctx;

static int mbedtls_net_connect_nonblock(mbedtls_net_context *ctx,
	const char *host, const char *port, int proto, int family);

// RSA のみを使う
static const int ciphersuites_RSA[] = {
	MBEDTLS_TLS_RSA_WITH_AES_128_CBC_SHA,
	0,
};

// mbedTLS ライブラリ内からのデバッグメッセージ表示用のコールバック
static void
debug_callback(void *aux, int level, const char *file, int line,
	const char *msg)
{
	struct timeval tv;
	FILE *out = (FILE *)aux;

	gettimeofday(&tv, NULL);
	fprintf(out, " %02d:%02d.%06d %d|%s|%4d|%s",
		(int)(tv.tv_sec / 60) % 60,
		(int)(tv.tv_sec     ) % 60,
		(int)(tv.tv_usec),
		level, file, line, msg);
}

// コンストラクタ
TLSHandle_mtls::TLSHandle_mtls()
{
	// 最初の1回だけグローバルコンテキストを初期化 (後始末はしない)
	if (__predict_false(gctx.initialized == false)) {
		mbedtls_entropy_init(&gctx.entropy);
		mbedtls_ctr_drbg_init(&gctx.ctr_drbg);
		// init RNG
		int r = mbedtls_ctr_drbg_seed(&gctx.ctr_drbg, mbedtls_entropy_func,
			&gctx.entropy, (const unsigned char *)"a", 1);
		if (r != 0) {
			ERROR("mbedtls_ctr_drbg_seed failed: %s\n", errmsg(r));
			throw "initializing gctx failed";
		}
		gctx.initialized = true;

		// mbedTLS のデバッグレベルはグローバルなのでここでセットする。
		mbedtls_debug_set_threshold(MTLS_DEBUG_LEVEL);
	}

	// メンバを初期化
	mbedtls_net_init(&net);
	mbedtls_ssl_init(&ssl);
	mbedtls_ssl_config_init(&conf);
}

// デストラクタ
TLSHandle_mtls::~TLSHandle_mtls()
{
	TRACE("called\n");

	Close();
	mbedtls_ssl_free(&ssl);
	mbedtls_ssl_config_free(&conf);
}

// mbedTLS のエラーコードを文字列にして返す
// (static バッファを使っていることに注意)
const char *
TLSHandle_mtls::errmsg(int code)
{
	mbedtls_strerror(code, errbuf, sizeof(errbuf));
	return errbuf;
}


// 初期化
bool
TLSHandle_mtls::Init()
{
	int r;

	if (inherited::Init() == false) {
		return false;
	}

	// TLS config
	r = mbedtls_ssl_config_defaults(&conf, MBEDTLS_SSL_IS_CLIENT,
		MBEDTLS_SSL_TRANSPORT_STREAM,
		MBEDTLS_SSL_PRESET_DEFAULT);
	if (r != 0) {
		ERROR("mbedtls_ssl_config_defaults failed: %s\n", errmsg(r));
		goto errexit;
	}

	mbedtls_ssl_conf_authmode(&conf, MBEDTLS_SSL_VERIFY_NONE);
	mbedtls_ssl_conf_rng(&conf, mbedtls_ctr_drbg_random, &gctx.ctr_drbg);
	mbedtls_ssl_conf_dbg(&conf, debug_callback, stderr);

	mbedtls_ssl_set_bio(&ssl, &net,
		mbedtls_net_send,
		NULL, // recv (without timeout)
		mbedtls_net_recv_timeout);

	TRACE("done\n");
	return true;

 errexit:
	// cleanup
	TRACE("failed\n");
	return false;
}

void
TLSHandle_mtls::SetTimeout(int timeout_)
{
	// 親クラスの timeout は、0 ならポーリング、-1 ならタイムアウトしない。
	// mbedtls_net_poll() の timeout はこの仕様。
	inherited::SetTimeout(timeout_);

	// 一方、mbedtls_ssl_conf_read_timeout() と mbedtls_net_recv_timeout() は
	// timeout 0 が無期限となっている。どうしてこうなった…
	// 仕方ないのでここで変数を2つ用意して、使い分けることにする。
	ssl_timeout = (timeout_ > 0) ? timeout_ : 0;

	mbedtls_ssl_conf_read_timeout(&conf, ssl_timeout);
}

bool
TLSHandle_mtls::UseRSA()
{
	mbedtls_ssl_conf_ciphersuites(&conf, ciphersuites_RSA);
	return true;
}

// 接続
bool
TLSHandle_mtls::Connect(const char *hostname, const char *servname)
{
#if defined(DEBUG)
	struct timeval start, end, result;
#endif
	int r;

	TRACE_tv(&start, "called: %s:%s\n", hostname, servname);

	// ssl_setup() は複数回呼んではいけないし、呼んだ後で conf を変更するな
	// と書いてあるように読める。TLSHandle_mtls は Init() 後、必要に応じて
	// いろいろ Set してから Connect() を呼ぶということにしているので、
	// ここで呼ぶのがいいか。
	r = mbedtls_ssl_setup(&ssl, &conf);
	if (r != 0) {
		ERROR("mbedtls_ssl_setup failed: %s\n", errmsg(r));
		return false;
	}

	// 独自のノンブロッキングコネクト。(see net_socket2.cpp)
	// 戻り値 -0x004b は EINPROGRESS 相当。
	r = mbedtls_net_connect_nonblock(&net, hostname, servname,
		MBEDTLS_NET_PROTO_TCP, family);
	if (__predict_false(r != -0x004b)) {
		if (__predict_false(r == 0)) {
			// 起きることはないはずだが
			// エラーメッセージが混乱しそうなので分けておく。
			ERROR("mbedtls_net_connect_nonblock %s:%s - %s\n",
				hostname, servname, "Success with blocking mode?");
			goto abort;
		} else {
			ERROR("mbedtls_net_connect_nonblock %s:%s - %s\n",
				hostname, servname, errmsg(r));
		}
		return false;
	}

	// ブロッキングに戻す (ここからは net コンテキスト内のディスクリプタが
	// オープンなので、用意されてる API が使える)
	r = mbedtls_net_set_block(&net);
	if (__predict_false(r != 0)) {
		// mbedtls_net_set_block() は他の mbedTLS API とか違って fcntl(2) の
		// 戻り値をそのまま返してしまっており、成功なら 0、エラーなら -1 が
		// 返ってくる。そのため errmsg() ではエラーメッセージが表示できない。
		ERROR("mbedtls_net_set_block failed\n");
		goto abort;
	}

	r = mbedtls_net_poll(&net, MBEDTLS_NET_POLL_WRITE, timeout);
	if (__predict_false(r < 0)) {
		ERROR("mbedtls_net_poll failed: %s\n", errmsg(r));
		goto abort;
	}
	if (__predict_false(r == 0)) {
		ERROR("mbedtls_net_poll: timed out\n");
		goto abort;
	}

	if (usessl) {
		while ((r = mbedtls_ssl_handshake(&ssl)) != 0) {
			if (r != MBEDTLS_ERR_SSL_WANT_READ
			 && r != MBEDTLS_ERR_SSL_WANT_WRITE) {
				ERROR("mbedtls_ssl_handshake failed: %s\n", errmsg(r));
				goto abort;
			}
		}
	}

#if defined(DEBUG)
	gettimeofday(&end, NULL);
	timersub(&end, &start, &result);
	trace(&end, __func__, "connected, %d.%03d msec\n",
		(int)result.tv_sec * 1000 + result.tv_usec / 1000,
		(int)result.tv_usec % 1000);
#endif
	return true;

 abort:
	mbedtls_net_free(&net);
	return false;
}

// クローズ。
// 未初期化時や接続前に呼び出しても副作用はない。
void
TLSHandle_mtls::Close()
{
	// これを知る方法はないのか…
	if (net.fd >= 0) {
		TRACE("called\n");

		if (usessl) {
			mbedtls_ssl_close_notify(&ssl);
		}
		// free という名前だが実は close
		mbedtls_net_free(&net);
	}
}

// shutdown する
int
TLSHandle_mtls::Shutdown(int how)
{
	int rv = 0;

	if (usessl == false) {
		rv = shutdown(net.fd, how);
	}

	return rv;
}

// 読み込み
size_t
TLSHandle_mtls::Read(void *buf, size_t len)
{
	ssize_t rv;

	TRACE("called\n");

	if (usessl) {
	 ssl_again:
		rv = mbedtls_ssl_read(&ssl, (unsigned char *)buf, len);
		if (rv < 0) {
			if (rv == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) {
				// EOF
				TRACE("EOF\n");
				return 0;
			}
			if (rv == MBEDTLS_ERR_SSL_WANT_READ) {
				// EINTR ?
				goto ssl_again;
			}
			ERROR("mbedtls_ssl_read failed: %s\n", errmsg(rv));
			return rv;
		}
	} else {
	 net_again:
		// net_recv_timeout() は 0 が無期限のほう (SetTimeout() 参照)
		rv = mbedtls_net_recv_timeout(&net, (unsigned char *)buf, len,
			ssl_timeout);
		if (rv < 0) {
			// XXX ?
			if (errno == EINTR) {
				goto net_again;
			}
			ERROR("mbedtls_net_recv_timeout failed: %s\n", errmsg(rv));
			return rv;
		}
	}

	TRACE("%zd bytes\n", rv);
	return rv;
}

// 書き出し
size_t
TLSHandle_mtls::Write(const void *buf, size_t len)
{
	ssize_t rv;

	TRACE("called\n");

	if (usessl) {
		rv = mbedtls_ssl_write(&ssl, (const unsigned char *)buf, len);
		if (rv < 0) {
			ERROR("mbedtls_ssl_write failed: %s\n", errmsg(rv));
			return rv;
		}
	} else {
		rv = mbedtls_net_send(&net, (const unsigned char *)buf, len);
		if (rv < 0) {
			ERROR("mbedtls_net_send failed: %s\n", errmsg(rv));
			return rv;
		}
	}

	TRACE("%zd bytes\n", rv);
	return rv;
}


//
// mbedTLS 改良
//

// C なので mbedtls_net_context (struct) の中身が丸見えなことを悪用する。
static_assert(sizeof(mbedtls_net_context) == sizeof(int),
	"mbedtls_net_context has be changed?");

// mbedtls_net_connect() のノンブロッキング connect(2) 対応版。
// ついでにアドレスファミリ限定 connect も出来るようにする。
//
// mbedTLS には、ctx で示されるオープンされたディスクリプタをノンブロッキング
// モードに設定する mbedtls_net_set_nonblock(ctx) は用意されているものの、
// mbedtls_net_connect() は自分で socket(2) を作って即 connect(2) を呼んで
// いるため、呼び出し側がこのソケットを connect 前にノンブロッキングに指定
// できる余地がない。そのため mbedtls/library/net_socket.c を横目に見ながら
// ほぼ同じものを書き起こす。orz
//
// ノンブロッキングモードで connect できれば戻り値 -0x004b を返す。
// ノンブロッキングモードの設定に失敗すれば戻り値 -0x0043 を返す。
// これらのエラーコードは mbedtls/include/net_socket.h を見て空いてるところを
// 選んだもの。それ以外は mbedtls_net_connect() 本来のエラーコードを返す。
// 本来ブロッキング connect に成功すれば 0 が返るが、それは起きないはず。
static int
mbedtls_net_connect_nonblock(mbedtls_net_context *ctx,
	const char *host, const char *port, int proto, int family)
{
	struct addrinfo hints, *addr_list, *cur;
	int ret;
	int val;

    /* Do name resolution with both IPv6 and IPv4 */
	memset(&hints, 0, sizeof(hints));
	// アドレスファミリを指定できるようにする
	hints.ai_family = family;
	if (proto == MBEDTLS_NET_PROTO_UDP) {
		hints.ai_socktype = SOCK_DGRAM;
		hints.ai_protocol = IPPROTO_UDP;
	} else {
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_protocol = IPPROTO_TCP;
	}

	if (getaddrinfo(host, port, &hints, &addr_list) != 0)
		return MBEDTLS_ERR_NET_UNKNOWN_HOST;

    /* Try the sockaddrs until a connection succeeds */
	ret = MBEDTLS_ERR_NET_UNKNOWN_HOST;
	for (cur = addr_list; cur != NULL; cur = cur->ai_next) {
		ctx->fd = socket(cur->ai_family, cur->ai_socktype, cur->ai_protocol);
		if (ctx->fd < 0) {
			ret = MBEDTLS_ERR_NET_SOCKET_FAILED;
			continue;
		}

		// ここでノンブロックに設定
		val = fcntl(ctx->fd, F_GETFL);
		if (val < 0) {
			close(ctx->fd);
			// 勝手に空いてるエラーコードを拝借、EINVAL 的なもの。
			ret = -0x0041;
			continue;
		}
		if (fcntl(ctx->fd, F_SETFL, val | O_NONBLOCK) < 0) {
			close(ctx->fd);
			// 勝手に空いてるエラーコードを拝借、EINVAL 的なもの。
			ret = -0x0041;
			continue;
		}

		if (connect(ctx->fd, cur->ai_addr, cur->ai_addrlen) == 0) {
			ret = 0;
			break;
		}
		// ノンブロッキングなので connect() は EINPROGRESS を返す
		if (errno == EINPROGRESS) {
			// 勝手に空いてるエラーコードを拝借。
			ret = -0x004b;
			break;
		}

		close(ctx->fd);
		ret = MBEDTLS_ERR_NET_CONNECT_FAILED;
	}

	freeaddrinfo(addr_list);
	return ret;
}


#if defined(TEST)

// Connection timeout のテストは -h 10.0.0.1 -p 80 -t 1000 とかで出来る。

#include <err.h>
#include <getopt.h>

void
usage()
{
	printf("usage: <options>\n");
	printf(" -d <level>   : set debug level\n");
	printf(" -h <hostname>: set hostname (default: www.google.com)\n");
	printf(" -p <pathname>: set pathname (default: /)\n");
	printf(" -s <servname>: set servname or portname (default: 443)\n");
	printf(" -r           : use RSA\n");
	printf(" -t <timeout> : set timeout (default -1)\n");
	exit(0);
}

int
main(int ac, char *av[])
{
	TLSHandle_mtls mtls;
	const char *hostname = "www.google.com";
	const char *servname = "443";
	const char *pathname = "/";
	int r;
	int c;
	int use_rsa_only;
	int timeout;

	use_rsa_only = 0;
	timeout = -1;
	while ((c = getopt(ac, av, "h:p:rs:t:")) != -1) {
		switch (c) {
		 case 'h':
			hostname = optarg;
			break;
		 case 'p':
			pathname = optarg;
			break;
		 case 'r':
			use_rsa_only = 1;
			break;
		 case 's':
			servname = optarg;
			break;
		 case 't':
			timeout = atoi(optarg);
			break;
		 default:
			usage();
			break;
		}
	}
	ac -= optind;
	av += optind;
	if (ac > 0) {
		usage();
	}

	fprintf(stderr, "Test to %s:%s\n", hostname, servname);

	if (mtls.Init() == false) {
		errx(1, "mtls.Init failed");
	}

	if (use_rsa_only) {
		fprintf(stderr, "UseRSA\n");
		mtls.UseRSA();
	}

	if (strcmp(servname, "443") == 0 || strcmp(servname, "https") == 0) {
		fprintf(stderr, "UseSSL\n");
		mtls.UseSSL(true);
	}

	if (timeout != -1) {
		fprintf(stderr, "Timeout=%d msec\n", timeout);
		mtls.SetTimeout(timeout);
	}

	if (mtls.Connect(hostname, servname) == false) {
		errx(1, "mtls.Connect failed");
	}

	char req[512];
	sprintf(req,
		"GET %s HTTP/1.1\r\n"
		"Host: %s\r\n"
		"Connection: close\r\n"
		"\r\n", pathname, hostname);
	fprintf(stderr, "write buf=|%s|\n", req);

	r = mtls.Write(req, strlen(req));
	if (r < 0) {
		errx(1, "Write failed %d", r);
	}

	for (;;) {
		char buf[1024];
		r = mtls.Read(buf, sizeof(buf));
#if defined(DEBUG)
		fprintf(stderr, "read=%d\n", r);
#endif
		if (r < 0) {
			if (r == MBEDTLS_ERR_SSL_WANT_READ ||
			    r == MBEDTLS_ERR_SSL_WANT_WRITE) {
				continue;
			}
			if (r == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) {
				break;
			}
			fprintf(stderr, "mtls.Read failed\n");
			break;
		}
		if (r == 0) {
			break;
		}
		fwrite(buf, 1, sizeof(buf), stderr);
#if defined(DEBUG)
		fprintf(stderr, "\n");
#endif
	}

	mtls.Close();
	return 0;
}
#endif // TEST
