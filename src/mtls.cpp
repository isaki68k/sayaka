#include "mtls.h"
#include "mbedtls/debug.h"
#include "mbedtls/error.h"
#include <cstdarg>
#include <cstdlib>
#include <cstring>
#include <sys/socket.h>

//#define DEBUG 1

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
mTLSHandle::mTLSHandle()
{
	// メンバを初期化
	mbedtls_net_init(&net);
	mbedtls_ssl_init(&ssl);
	mbedtls_ssl_config_init(&conf);
}

// デストラクタ
mTLSHandle::~mTLSHandle()
{
	TRACE("called\n");

	if (initialized) {
		Close();
		mbedtls_ssl_free(&ssl);
		mbedtls_ssl_config_free(&conf);
	}
}

// mbedTLS のエラーコードを文字列にして返す
// (static バッファを使っていることに注意)
const char *
mTLSHandle::errmsg(int code)
{
	mbedtls_strerror(code, errbuf, sizeof(errbuf));
	return errbuf;
}

// mbedTLS のデバッグレベルを指定
void
mTLSHandle::SetDebugLevel(int level)
{
	mbedtls_debug_set_threshold(level);
}


// 初期化
bool
mTLSHandle::Init()
{
	int r;

	// グローバルコンテキストの初期化
	if (gctx.initialized == false) {
		mbedtls_ctr_drbg_init(&gctx.ctr_drbg);
		mbedtls_entropy_init(&gctx.entropy);
		// init RNG
		r = mbedtls_ctr_drbg_seed(&gctx.ctr_drbg, mbedtls_entropy_func,
			&gctx.entropy, (const unsigned char *)"a", 1);
		if (r != 0) {
			ERROR("mbedtls_ctr_drbg_seed failed: %s\n", errmsg(r));
			goto errexit;
		}
		gctx.initialized = true;
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
	r = mbedtls_ssl_setup(&ssl, &conf);
	if (r != 0) {
		ERROR("mbedtls_ssl_setup failed: %s\n", errmsg(r));
		goto errexit;
	}

	mbedtls_ssl_set_bio(&ssl, &net, mbedtls_net_send, mbedtls_net_recv, NULL);

	initialized = true;
	TRACE("done\n");
	return true;

 errexit:
	// cleanup
	TRACE("failed\n");
	return false;
}

void
mTLSHandle::UseRSA()
{
	mbedtls_ssl_conf_ciphersuites(&conf, ciphersuites_RSA);
}

// 接続
bool
mTLSHandle::Connect(const char *hostname, const char *servname)
{
	struct timeval start, end, result;
	int r;

	TRACE_tv(&start, "called: %s:%s\n", hostname, servname);

	r = mbedtls_net_connect_timeout(&net, hostname, servname,
		MBEDTLS_NET_PROTO_TCP, connect_timeout);
	if (r != 0) {
		ERROR("mbedtls_net_connect_timeout %s:%s - %s\n",
			hostname, servname, errmsg(r));
		return false;
	}

	if (usessl == false) {
		TRACE("connect (plain) OK\n");
		return true;
	}

	while ((r = mbedtls_ssl_handshake(&ssl)) != 0) {
		if (r != MBEDTLS_ERR_SSL_WANT_READ && r != MBEDTLS_ERR_SSL_WANT_WRITE) {
			ERROR("mbedtls_ssl_handshake failed: %s\n", errmsg(r));
			return false;
		}
	}

	TRACE_tv(&end, "connect OK\n");

	timersub(&end, &start, &result);
	TRACE("connect time = %d.%03d msec\n",
		(int)result.tv_sec * 1000 + result.tv_usec / 1000,
		(int)result.tv_usec % 1000);
	return 0;
}

// クローズ。
// 未初期化時や接続前に呼び出しても副作用はない。
void
mTLSHandle::Close()
{
	TRACE("called\n");

	if (initialized) {
		if (usessl) {
			mbedtls_ssl_close_notify(&ssl);
		}

		// free という名前だが実は close
		mbedtls_net_free(&net);
	}
}

// shutdown する
int
mTLSHandle::Shutdown(int how)
{
	int rv = 0;

	if (usessl == false) {
		rv = shutdown(net.fd, how);
	}

	return rv;
}

// 読み込み
int
mTLSHandle::Read(void *buf, int len)
{
	int rv;

	TRACE("called\n");

	if (usessl) {
		rv = mbedtls_ssl_read(&ssl, (unsigned char *)buf, len);
	} else {
		rv = mbedtls_net_recv(&net, (unsigned char *)buf, len);
	}

	if (rv == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) {
		// EOF
		TRACE("EOF\n");
		return 0;
	}
	if (rv < 0) {
		ERROR("mbedtls_*_read failed: %s\n", errmsg(rv));
		return rv;
	}

	TRACE("%d bytes\n", rv);
	return rv;
}

// 書き出し
int
mTLSHandle::Write(const void *buf, int len)
{
	int rv;

	TRACE("called\n");

	if (usessl) {
		rv = mbedtls_ssl_write(&ssl, (const unsigned char *)buf, len);
	} else {
		rv = mbedtls_net_send(&net, (const unsigned char *)buf, len);
	}

	if (rv < 0) {
		ERROR("mbedtls_*_write failed: %s\n", errmsg(rv));
		return rv;
	}

	TRACE("%d bytes\n", rv);
	return rv;
}


#if defined(TEST)

#include <err.h>
#include <getopt.h>

int
main(int ac, char *av[])
{
	mTLSHandle mtls;
	const char* hostname = "www.google.com";
	const char* servname = "443";
	int r;
	int c;
	int debuglevel;
	int use_rsa_only;

	debuglevel = 0;
	use_rsa_only = 0;
	while ((c = getopt(ac, av, "d:p:r")) != -1) {
		switch (c) {
		 case 'd':
			debuglevel = atoi(optarg);
			break;
		 case 'p':
			servname = optarg;
			break;
		 case 'r':
			use_rsa_only = 1;
			break;
		 default:
			printf("usage: [-p servname] [hostname]\n");
			break;
		}
	}
	ac -= optind;
	av += optind;
	if (ac > 0) {
		hostname = av[0];
	}
	mtls.SetDebugLevel(debuglevel);

	fprintf(stderr, "Test to %s:%s\n", hostname, servname);

	if (mtls.Init() == false) {
		errx(1, "mtls.Init failed");
	}

	if (use_rsa_only) {
		mtls.UseRSA();
	}

	if (strcmp(servname, "443") == 0) {
		mtls.UseSSL(true);
	}

	if (mtls.Connect(hostname, servname) == false) {
		errx(1, "mtls.Connect failed");
	}

	char req[128];
	sprintf(req,
		"GET / HTTP/1.1\r\n"
		"Host: %s\r\n"
		"Connection: close\r\n"
		"\r\n", hostname);
	fprintf(stderr, "write buf=|%s|\n", req);

	r = mtls.Write(req, strlen(req));
	if (r < 0) {
		errx(1, "Write failed %d", r);
	}

	for (;;) {
		char buf[1024];
		r = mtls.Read(buf, sizeof(buf) - 1);
		if (r == MBEDTLS_ERR_SSL_WANT_READ || r == MBEDTLS_ERR_SSL_WANT_WRITE) {
			continue;
		}
		if (r == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) {
			break;
		}
		if (r < 0) {
			errx(1, "mtls.Read failed %d", r);
		}
		if (r == 0) {
			break;
		}
		buf[r] = '\0';
		fprintf(stderr, "%s", buf);
	}

	mtls.Close();
	return 0;
}
#endif // TEST
