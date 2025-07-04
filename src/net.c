/* vi:set ts=4: */
/*
 * Copyright (C) 2024 Tetsuya Isaki
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

//
// ネットワーク
//

#include "common.h"
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#if defined(HAVE_OPENSSL)
#include <openssl/err.h>
#include <openssl/ssl.h>
#endif

struct net;
struct net {
	int (*f_connect)(struct net *, const char *, const char *,
		const struct net_opt *);
	int (*f_read)(struct net *, void *, int);
	int (*f_write)(struct net *, const void *, int);
	void (*f_shutdown)(struct net *);
	void (*f_close)(struct net *);

	// f_connect() が確保したリソースを解放する。
	// net 自体はここではなく呼び出し元の net_destroy() が解放する。
	void (*f_cleanup)(struct net *);

	int sock;
#if defined(HAVE_OPENSSL)
	SSL_CTX *ctx;
	SSL *ssl;
#endif

	const struct diag *diag;

	// 行単位受信用の受信バッファ。
	// バッファにあればこちらから優先して読み出す。
	// バッファが空で行単位受信でない場合はここを経由しなくてよい。
	uint buflen;			// バッファの有効長
	uint bufpos;			// 現在位置
	char buf[1024];
};

static void sock_cleanup(struct net *);
static int  sock_connect(struct net *, const char *, const char *,
	const struct net_opt *);
static int  sock_read(struct net *, void *, int);
static int  sock_write(struct net *, const void *, int);
static void sock_shutdown(struct net *);
static void sock_close(struct net *);
#if defined(HAVE_OPENSSL)
static void tls_cleanup(struct net *);
static int  tls_connect(struct net *, const char *, const char *,
	const struct net_opt *);
static int  tls_read(struct net *, void *, int);
static int  tls_write(struct net *, const void *, int);
static void tls_shutdown(struct net *);
static void tls_close(struct net *);
#endif
static int  socket_connect(const char *, const char *, const struct net_opt *);
static int  socket_setblock(int, bool);

//
// URL パーサ
//

// url をパースして urlinfo を作成して返す。
struct urlinfo *
urlinfo_parse(const char *urlstr)
{
	char url[strlen(urlstr) + 1];
	char *sep;
	const char *scheme;
	const char *authority;
	const char *userpass;
	const char *user;
	const char *pass;
	const char *hostport;
	const char *host;
	const char *port;
	const char *pqf;

	strlcpy(url, urlstr, sizeof(url));

	// スキームとそれ以降(オーソリティ+PQF)を分離。
	sep = strstr(url, "://");
	if (sep) {
		*sep = '\0';
		scheme = url;
		authority = sep + 3;
	} else {
		scheme = "";
		authority = url;
	}

	// オーソリティとそれ以降(PathQueryFragment)を分離。
	sep = strchr(authority, '/');
	if (sep) {
		*sep++ = '\0';
		pqf = sep;
	} else {
		pqf = "";
	}

	// オーソリティからユーザ情報とホストポートを分離。
	sep = strchr(authority, '@');
	if (sep) {
		*sep++ = '\0';
		userpass = authority;
		hostport = sep;
	} else {
		userpass = "";
		hostport = authority;
	}

	// ユーザ情報をユーザ名とパスワードに分離。
	sep = strchr(userpass, ':');
	if (sep) {
		*sep++ = '\0';
		user = userpass;
		pass = sep;
	} else {
		user = userpass;
		pass = "";
	}

	// ホストポートをホストとポートに分離。
	port = "";
	if (hostport[0] == '[') {
		// IPv6 アドレスは中に ':' があるので先に見ないといけない。
		char *e = strchr(hostport, ']');
		if (e) {
			*e++ = '\0';
			host = hostport + 1;
			// ':' が続いてるはずだが。
			sep = strchr(e, ':');
			if (sep) {
				port = sep + 1;
			}
		} else {
			// 閉じ括弧がない?
			host = hostport + 1;
		}
	} else {
		sep = strchr(hostport, ':');
		if (sep) {
			*sep++ = '\0';
			host = hostport;
			port = sep;
		} else {
			host = hostport;
		}
	}

	// PathQueryFragment の分離は不要。

	struct urlinfo *info = calloc(1, sizeof(*info));
	if (info == NULL) {
		return NULL;
	}
	info->scheme	= string_from_cstr(scheme);
	info->host		= string_from_cstr(host);
	info->port		= string_from_cstr(port);
	info->user		= string_from_cstr(user);
	info->password	= string_from_cstr(pass);
	info->pqf		= string_alloc(strlen(pqf) + 2);
	string_append_char(info->pqf, '/');
	string_append_cstr(info->pqf, pqf);
	return info;
}

// info を解放する。info が NULL なら何もしない。
void
urlinfo_free(struct urlinfo *info)
{
	if (info) {
		string_free(info->scheme);
		string_free(info->host);
		string_free(info->port);
		string_free(info->user);
		string_free(info->password);
		string_free(info->pqf);
		free(info);
	}
}

// baseurl の path, query, fragment を newurl のもので更新する。
// newurl のほうは影響を受けない。
void
urlinfo_update_path(struct urlinfo *baseurl, const struct urlinfo *newurl)
{
	assert(baseurl);
	assert(newurl);

	string_free(baseurl->pqf);
	baseurl->pqf = string_dup(newurl->pqf);
}

// url を文字列にして返す。
string *
urlinfo_to_string(const struct urlinfo *url)
{
	string *s = string_init();

	if (string_len(url->scheme)) {
		string_append_cstr(s, string_get(url->scheme));
		string_append_cstr(s, "://");
	}
	if (string_len(url->user)) {
		string_append_cstr(s, string_get(url->user));
		if (string_len(url->password)) {
			string_append_char(s, ':');
			string_append_cstr(s, string_get(url->password));
		}
		string_append_char(s, '@');
	}
	if (strchr(string_get(url->host), ':')) {
		string_append_char(s, '[');
		string_append_cstr(s, string_get(url->host));
		string_append_char(s, ']');
	} else {
		string_append_cstr(s, string_get(url->host));
	}
	if (string_len(url->port)) {
		string_append_char(s, ':');
		string_append_cstr(s, string_get(url->port));
	}
	string_append_cstr(s, string_get(url->pqf));

	return s;
}


//
// コネクション
//

// opt を初期化する。
void
net_opt_init(struct net_opt *opt)
{
	memset(opt, 0, sizeof(*opt));
	opt->address_family = 0;
	opt->use_rsa_only = false;
	opt->timeout_msec = 0;
}

// net コンテキストを作成する。
struct net *
net_create(const struct diag *diag)
{
	struct net *net = calloc(1, sizeof(*net));

	net->sock = -1;
	net->diag = diag;

	return net;
}

// net コンテキストを解放する。
void
net_destroy(struct net *net)
{
	if (net) {
		if (net->f_close) {
			net->f_close(net);
		}
		if (net->f_cleanup) {
			net->f_cleanup(net);
		}
		free(net);
	}
}

// scheme://host:serv/ に接続する。
// 成功すれば 0 を返す。
// 失敗すれば (おそらく) errno をセットして -1 を返す
// (OpenSSL は返さないかも知れないが)。
// SSL 接続なのに SSL ライブラリが有効でない時は -2 を返す。
int
net_connect(struct net *net,
	const char *scheme, const char *host, const char *serv,
	const struct net_opt *opt)
{
	assert(net);

	if (diag_get_level(net->diag) >= 1) {
		char portbuf[16];
		if (strcmp(scheme, serv) == 0) {
			portbuf[0] = '\0';
		} else {
			snprintf(portbuf, sizeof(portbuf), ":%s", serv);
		}
		diag_print(net->diag, "Trying %s %s%s ...", scheme, host, portbuf);
	}

	// ここでプロトコル選択。
	if (strcmp(scheme, "https") == 0 ||
		strcmp(scheme, "wss") == 0)
	{
#if defined(HAVE_OPENSSL)
		net->f_connect = tls_connect;
		net->f_read    = tls_read;
		net->f_write   = tls_write;
		net->f_shutdown= tls_shutdown;
		net->f_close   = tls_close;
		net->f_cleanup = tls_cleanup;
#else
		Debug(net->diag, "%s: SSL library not compiled", __func__);
		return -2;
#endif
	} else {
		net->f_connect = sock_connect;
		net->f_read    = sock_read;
		net->f_write   = sock_write;
		net->f_shutdown= sock_shutdown;
		net->f_close   = sock_close;
		net->f_cleanup = sock_cleanup;
	}

	// f_connect() は接続出来たら Debug レベルで "Connected"
	// (と SSL 等の追加情報) を表示する。
	return net->f_connect(net, host, serv, opt);
}

// 1行受信して返す。
string *
net_gets(struct net *net)
{
	assert(net);
	const struct diag *diag = net->diag;

	Verbose(diag, "%s: begin", __func__);
	string *s = string_init();

	for (;;) {
		// バッファが空なら受信。
		if (net->bufpos == net->buflen) {
			int n = net_read(net, net->buf, sizeof(net->buf));
			Verbose(diag, "%s: net_read=%d", __func__, n);
			if (n < 0) {
				Debug(diag, "%s: net_read failed: %s", __func__, strerrno());
				return s;
			}
			if (n == 0) {
				// EOF
				string_free(s);
				return NULL;
			}
			net->bufpos = 0;
			net->buflen = n;
		}

		// バッファから改行を探す。
		uint pos;
		bool lf_found = false;
		for (pos = net->bufpos; pos < net->buflen; pos++) {
			if (net->buf[pos] == '\n') {
				pos++;
				lf_found = true;
				break;
			}
		}
		uint copylen = pos - net->bufpos;
		string_append_mem(s, net->buf + net->bufpos, copylen);
		net->bufpos += copylen;
		Verbose(diag, "%s: copied=%u, pos=%u/len=%u%s", __func__,
			copylen, net->bufpos, net->buflen,
			(lf_found ? " lf_found" : ""));
		if (lf_found) {
			return s;
		}
	}
}

// dst に最大 dstsize バイトを受信する。
int
net_read(struct net *net, void *dst, uint dstsize)
{
	assert(net);

	// バッファにあれば先に使い切る。
	if (net->bufpos != net->buflen) {
		uint copylen = MIN(net->buflen - net->bufpos, dstsize);
		memcpy(dst, net->buf + net->bufpos, copylen);
		net->bufpos += copylen;
		return copylen;
	}

	int n = net->f_read(net, dst, dstsize);
	return n;
}

int
net_write(struct net *net, const void *src, uint srcsize)
{
	assert(net);
	int n = net->f_write(net, src, srcsize);
	return n;
}

// 送信方向を shutdown する。
void
net_shutdown(struct net *net)
{
	assert(net);
	net->f_shutdown(net);
}

void
net_close(struct net *net)
{
	assert(net);
	net->f_close(net);
}

// 生ソケットを取得する。
int
net_get_fd(const struct net *net)
{
	assert(net);
	return net->sock;
}


//
// 生ソケット
//

static int
sock_connect(struct net *net, const char *host, const char *serv,
	const struct net_opt *opt)
{
	struct timespec start, end;

	clock_gettime(CLOCK_MONOTONIC, &start);
	net->sock = socket_connect(host, serv, opt);
	if (net->sock < 0) {
		return -1;
	}
	clock_gettime(CLOCK_MONOTONIC, &end);
	uint32 msec = timespec_to_msec(&end) - timespec_to_msec(&start);
	Debug(net->diag, "Connected (%u msec)", msec);
	return 0;
}

static int
sock_read(struct net *net, void *dst, int dstsize)
{
	int n = read(net->sock, dst, dstsize);
	return n;
}

static int
sock_write(struct net *net, const void *src, int srcsize)
{
	int n = write(net->sock, src, srcsize);
	return n;
}

static void
sock_shutdown(struct net *net)
{
	shutdown(net->sock, SHUT_WR);
}

static void
sock_close(struct net *net)
{
	if (net->sock >= 3) {
		close(net->sock);
	}
	net->sock = -1;
}

static void
sock_cleanup(struct net *net)
{
}


#if defined(HAVE_OPENSSL)

//
// TLS
//

// 失敗すると errno をセットして -1 を返す仕様だが、
// OpenSSL のライブラリが何を返すかいまいち分からない。orz
static int
tls_connect(struct net *net, const char *host, const char *serv,
	const struct net_opt *opt)
{
	struct timespec start, end;
	const struct diag *diag = net->diag;
	int r;

	static bool initialized = false;
	if (initialized == false) {
		SSL_load_error_strings();
		SSL_library_init();
		initialized = true;
	}

	net->ctx = SSL_CTX_new(TLS_client_method());
	if (net->ctx == NULL) {
		Debug(diag, "%s: SSL_CTX_new failed", __func__);
		return -1;
	}

	if (opt->use_rsa_only) {
		// RSA 指定なら TLSv1.2 以下を強制する。
		// (TLSv1.3 ではそもそも RSA128-SHA とかの指定自体が存在しない)
		SSL_CTX_set_max_proto_version(net->ctx, TLS1_2_VERSION);

		r = SSL_CTX_set_cipher_list(net->ctx, "AES128-SHA");
		if (r != 1) {
			ERR_print_errors_fp(stderr);
			return -1;
		}
	}

	net->ssl = SSL_new(net->ctx);
	if (net->ssl == NULL) {
		Debug(diag, "%s: SSL_new failed", __func__);
		return -1;
	}

	clock_gettime(CLOCK_MONOTONIC, &start);

	net->sock = socket_connect(host, serv, opt);
	if (net->sock == -1) {
		Debug(diag, "%s: %s:%s failed: %s", __func__, host, serv, strerrno());
		return -1;
	}

	r = SSL_set_fd(net->ssl, net->sock);
	if (r == 0) {
		ERR_print_errors_fp(stderr);
		return -1;
	}

	r = SSL_set_tlsext_host_name(net->ssl, UNCONST(host));
	if (r != 1) {
		ERR_print_errors_fp(stderr);
		return -1;
	}

	if (SSL_connect(net->ssl) < 1) {
		Debug(diag, "%s: SSL_connect failed", __func__);
		return -1;
	}

	// 接続できたらログ。
	if (__predict_false(diag_get_level(diag) >= 1)) {
		clock_gettime(CLOCK_MONOTONIC, &end);

		SSL_SESSION *sess = SSL_get_session(net->ssl);
		int ssl_version = SSL_SESSION_get_protocol_version(sess);
		char verbuf[16];
		const char *ver;
		switch (ssl_version) {
		 case SSL3_VERSION:		ver = "SSLv3";		break;
		 case TLS1_VERSION:		ver = "TLSv1.0";	break;
		 case TLS1_1_VERSION:	ver = "TLSv1.1";	break;
		 case TLS1_2_VERSION:	ver = "TLSv1.2";	break;
		 case TLS1_3_VERSION:	ver = "TLSv1.3";	break;
		 default:
			snprintf(verbuf, sizeof(verbuf), "0x%04x", ssl_version);
			ver = verbuf;
			break;
		}

		const SSL_CIPHER *ssl_cipher = SSL_SESSION_get0_cipher(sess);
		const char *cipher_name = SSL_CIPHER_get_name(ssl_cipher);

		uint32 msec = timespec_to_msec(&end) - timespec_to_msec(&start);
		diag_print(diag, "Connected %s %s (%u msec)", ver, cipher_name, msec);
	}

	return 0;
}

static int
tls_read(struct net *net, void *dst, int dstsize)
{
	const struct diag *diag = net->diag;
	ssize_t r;

	Verbose(diag, "%s (dstsize=%u)", __func__, dstsize);
	r = SSL_read(net->ssl, dst, dstsize);
	if (r < 0) {
		if (SSL_get_error(net->ssl, r) != SSL_ERROR_SYSCALL) {
			// とりあえず何かにしておく。
			errno = EIO;
		}
		Verbose(diag, "%s r=%zd, errno=%d", __func__, r, errno);
	} else {
		Verbose(diag, "%s r=%zd", __func__, r);
	}
	return r;
}

static int
tls_write(struct net *net, const void *src, int srcsize)
{
	const struct diag *diag = net->diag;
	ssize_t r;

	Verbose(diag, "%s (srcsize=%u)", __func__, srcsize);
	r = SSL_write(net->ssl, src, srcsize);
	if (r < 0) {
		if (SSL_get_error(net->ssl, r) != SSL_ERROR_SYSCALL) {
			// とりあえず何かにしておく。
			errno = EIO;
		}
		Verbose(diag, "%s r=%zd, errno=%d", __func__, r, errno);
	} else {
		Verbose(diag, "%s r=%zd", __func__, r);
	}
	return r;
}

static void
tls_shutdown(struct net *net)
{
	SSL_shutdown(net->ssl);
}

static void
tls_close(struct net *net)
{
	if (net->ssl) {
		SSL_shutdown(net->ssl);
	}
	// 元ソケットも閉じる。
	sock_close(net);
}

static void
tls_cleanup(struct net *net)
{
	if (net->ssl) {
		SSL_free(net->ssl);
		net->ssl = NULL;
	}
	if (net->ctx) {
		SSL_CTX_free(net->ctx);
		net->ctx = NULL;
	}
}

#endif // HAVE_OPENSSL


// 下請け。
// hostname:servname に TCP で接続しそのソケットを返す。
// 失敗すれば errno をセットして -1 を返す。
static int
socket_connect(const char *hostname, const char *servname,
	const struct net_opt *opt)
{
	struct timeval tv;
	struct timeval *tvp;
	struct timespec now;
	struct addrinfo hints;
	struct addrinfo *ai;
	struct addrinfo *ailist;
	uint64 end_usec = 0;
	fd_set wfds;
	int fd;
	int r;

	if (__predict_false(opt->timeout_msec == 0)) {
		tvp = NULL;
	} else {
		tvp = &tv;
		clock_gettime(CLOCK_MONOTONIC, &now);
		end_usec = timespec_to_usec(&now) + (opt->timeout_msec * 1000);
	}

	memset(&hints, 0, sizeof(hints));
	switch (opt->address_family) {
	 case 4:
		hints.ai_family = PF_INET;
		break;
	 case 6:
		hints.ai_family = PF_INET6;
		break;
	 default:
		hints.ai_family = PF_UNSPEC;
		break;
	}
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	if (getaddrinfo(hostname, servname, &hints, &ailist) != 0) {
		return -1;
	}

	fd = -1;
	for (ai = ailist; ai != NULL; ai = ai->ai_next) {
		int val;
		socklen_t vallen = sizeof(val);

		fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
		if (fd < 0) {
			continue;
		}

		// ここでノンブロックに設定。
		if (socket_setblock(fd, false) < 0) {
			goto abort_continue;
		}

		// ノンブロッキングなので connect() は EINPROGRESS を返す。
		if (connect(fd, ai->ai_addr, ai->ai_addrlen) == 0) {
			break;
		}
		if (errno != EINPROGRESS) {
			goto abort_continue;
		}

		if (tvp != NULL) {
			clock_gettime(CLOCK_MONOTONIC, &now);
			uint64 now_usec = timespec_to_usec(&now);
			if (now_usec >= end_usec) {
				errno = ETIMEDOUT;
				return -1;
			}
			uint64 timeout_usec = end_usec - now_usec;
			tv.tv_sec  = timeout_usec / 1000000;
			tv.tv_usec = timeout_usec % 1000000;
		}
		FD_ZERO(&wfds);
		FD_SET(fd, &wfds);
		r = select(fd + 1, NULL, &wfds, NULL, tvp);
		if (r <= 0) {
			goto abort_continue;
		}

		val = -1;
		getsockopt(fd, SOL_SOCKET, SO_ERROR, &val, &vallen);
		if (val != 0) {
			goto abort_continue;
		}

		// ここまで来れば接続成功。
		break;

 abort_continue:
		close(fd);
		fd = -1;
	}
	freeaddrinfo(ailist);

	// 一応。
	if (fd < 0) {
		return -1;
	}

	// ブロッキングに戻す。
	if (socket_setblock(fd, true) < 0) {
		close(fd);
		return -1;
	}

	return fd;
}

// 下請け。
// ソケット fd のブロッキングモードを変更する。
// blocking = true ならブロッキングモード、
// blocking = false ならノンブロッキングモード。
// 成功すれば 0、失敗すれば errno をセットして -1 を返す。
static int
socket_setblock(int fd, bool blocking)
{
	int val;

	val = fcntl(fd, F_GETFL);
	if (val < 0) {
		return -1;
	}

	if (blocking) {
		val &= ~O_NONBLOCK;
	} else {
		val |= O_NONBLOCK;
	}

	if (fcntl(fd, F_SETFL, val) < 0) {
		return -1;
	}

	return 0;
}
