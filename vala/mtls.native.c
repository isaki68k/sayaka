/*
 * Copyright (C) 2016 Y.Sugahara (moveccr)
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mtls.native.h"

//#define DEBUG

#include <sys/time.h>

#if defined(DEBUG)
#define TRACE(fmt...)	do { \
	struct timeval tv;	\
	TRACE_tv(&tv, fmt);	\
} while (0)
#define TRACE_tv(tvp, fmt...)	do { \
	gettimeofday((tvp), NULL); \
	fprintf(stderr, "[%02d:%02d.%06d] %s() ",	\
		(int)((tvp)->tv_sec / 60) % 60,	\
		(int)((tvp)->tv_sec) % 60,	\
		(int)((tvp)->tv_usec),	\
		 __FUNCTION__);	\
	fprintf(stderr, fmt);	\
} while (0)
#define ERROR(fmt...)	TRACE(fmt)
#else
#define TRACE(fmt...)			(void)0
#define TRACE_tv(tvp, fmt...)	(void)0
#define ERROR(fmt...)	do { \
	fprintf(stderr, fmt);	\
} while (0)	/* XXX とりあえず */
#endif

// Mac OS X 10.8 には timersub がない
#if !defined(timersub)
#define timersub(a, b, res)	do {			\
	(res)->tv_sec = (a)->tv_sec - (b)->tv_sec;	\
	(res)->tv_usec = (a)->tv_usec - (b)->tv_usec;	\
	if ((res)->tv_usec < 0) {			\
		(res)->tv_sec--;			\
		(res)->tv_usec += 1000000;		\
	}						\
} while (0)
#endif

// global context
mtls_global_ctx_t gctx;


////////////////////////////////////

// --ciphers RSA 用の ciphersuites。
static int ciphersuites_RSA[] = {
	MBEDTLS_TLS_RSA_WITH_AES_128_CBC_SHA,
	0,
};

// mbedTLS のエラーコードを表示用の文字列にして返します。
static const char *
mtls_errmsg(int errcode)
{
	static char buf[128];

	mbedtls_strerror(errcode, buf, sizeof(buf));
	return buf;
}

// デバッグメッセージ表示用のコールバックです。
static void
debug_callback(void *aux, int level, const char *file, int line,
	const char *msg)
{
	struct timeval tv;
	FILE *out = (FILE *)aux;

	gettimeofday(&tv, NULL);
	fprintf(out, " %02d:%02d.%06d %d|%s|%4d|%s",
		(int)(tv.tv_sec / 60) % 60,
		(int)(tv.tv_sec) % 60,
		(int)(tv.tv_usec),
		level, file, line, msg);
}

// デバッグレベルを指定します。
void
mtls_set_debuglevel(int level)
{
	mbedtls_debug_set_threshold(level);
}

// mtlsctx_t のメモリを確保します。
// alloc と init を分離しないと、エラー通知が難しいので分離されています。
// mtls_alloc() で確保したメモリは mtls_free() で解放してください。
mtlsctx_t*
mtls_alloc()
{
	mtlsctx_t *ctx;

	TRACE("called\n");
	ctx = (mtlsctx_t *)malloc(sizeof(mtlsctx_t));
	if (ctx != NULL) {
		memset(ctx, 0, sizeof(*ctx));
	}
	return ctx;
}

// コンテキストを初期化します。
// mtls_alloc() に続いてコールしてください。
// 成功すれば 0、失敗すれば -1 を返します。
int
mtls_init(mtlsctx_t* ctx)
{
	int r;

	TRACE("start\n");

	// グローバルコンテキストの初期化
	if (gctx.initialized == 0) {
		mbedtls_ctr_drbg_init(&gctx.ctr_drbg);
		mbedtls_entropy_init(&gctx.entropy);
		// init RNG
		r = mbedtls_ctr_drbg_seed(&gctx.ctr_drbg, mbedtls_entropy_func,
			&gctx.entropy, "a", 1);
		if (r != 0) {
			ERROR("mbedtls_ctr_drbg_seed failed: %s\n", mtls_errmsg(r));
			goto errexit;
		}
		gctx.initialized = 1;
	}

	ctx->usessl = 0;
	mbedtls_net_init(&ctx->net);
	mbedtls_ssl_init(&ctx->ssl);
	mbedtls_ssl_config_init(&ctx->conf);

	// TLS config
	r = mbedtls_ssl_config_defaults(&ctx->conf,
			MBEDTLS_SSL_IS_CLIENT,
			MBEDTLS_SSL_TRANSPORT_STREAM,
			MBEDTLS_SSL_PRESET_DEFAULT);
	if (r != 0) {
		ERROR("mbedtls_ssl_config_defaults failed: %s\n", mtls_errmsg(r));
		goto errexit;
	}

	mbedtls_ssl_conf_authmode(&ctx->conf, MBEDTLS_SSL_VERIFY_NONE);
	mbedtls_ssl_conf_rng(&ctx->conf, mbedtls_ctr_drbg_random, &gctx.ctr_drbg);
	mbedtls_ssl_conf_dbg(&ctx->conf, debug_callback, stderr);

	r = mbedtls_ssl_setup(&ctx->ssl, &ctx->conf);
	if (r != 0) {
		ERROR("mbedtls_ssl_setup failed: %s\n", mtls_errmsg(r));
		goto errexit;
	}

	mbedtls_ssl_set_bio(&ctx->ssl, &ctx->net, mbedtls_net_send, mbedtls_net_recv, NULL);

	ctx->initialized = 1;
	TRACE("done\n");
	return 0;

 errexit:
	// cleanup
	TRACE("NG\n");
	return -1;
}

// ctx をクリーンアップしてメモリを解放します。
// オープンされていればクローズも行います。
void
mtls_free(mtlsctx_t* ctx)
{
	TRACE("called\n");
	if (ctx != NULL) {
		if (ctx->initialized) {
			mtls_close(ctx);
			mbedtls_ssl_free(&ctx->ssl);
			mbedtls_ssl_config_free(&ctx->conf);
		}
		free(ctx);
	}
}

// HTTPS かどうかを設定します。
// mtls_connect() より先に設定しておく必要があります。
void
mtls_setssl(mtlsctx_t* ctx, int value)
{
	ctx->usessl = value;
}

// 接続に使用する ciphersuites を RSA_WITH_AES_128_CBC_SHA に限定します。
// mtls_connect() より先に設定しておく必要があります。
// XXX どういう API にすべか
void
mtls_usersa(mtlsctx_t* ctx)
{
	mbedtls_ssl_conf_ciphersuites(&ctx->conf, ciphersuites_RSA);
}

// コネクトタイムアウトを設定します。
// mtls_connect() より先に設定しておく必要があります。
void
mtls_set_timeout(mtlsctx_t* ctx, int timeout)
{
	ctx->connect_timeout = timeout;
}


// 接続します。
int
mtls_connect(mtlsctx_t* ctx, const char* hostname, const char *servname)
{
	struct timeval start, end, result;

	TRACE_tv(&start, "called: %s:%s\n", hostname, servname);
	int r;

	r = mbedtls_net_connect_timeout(&ctx->net, hostname, servname,
			MBEDTLS_NET_PROTO_TCP, ctx->connect_timeout);
	if (r != 0) {
		ERROR("mbedtls_net_connect %s:%s - %s\n", hostname, servname,
			mtls_errmsg(r));
		return -1;
	}

	if (ctx->usessl == 0) {
		TRACE("connect (plain) OK\n");
		return 0;
	}

	while ((r = mbedtls_ssl_handshake(&ctx->ssl)) != 0) {
		if (r != MBEDTLS_ERR_SSL_WANT_READ && r != MBEDTLS_ERR_SSL_WANT_WRITE) {
			ERROR("mbedtls_ssl_handshake failed: %s\n", mtls_errmsg(r));
			return -1;
		}
	}

	TRACE_tv(&end, "connect OK\n");
	timersub(&end, &start, &result);
	TRACE("connect time = %d.%03d msec\n",
		(int)result.tv_sec * 1000 + result.tv_usec / 1000,
		(int)result.tv_usec % 1000);
	return 0;
}

// 接続をクローズします。
// 未接続や未初期化の状態で呼んでも副作用はありません。
int
mtls_close(mtlsctx_t* ctx)
{
	TRACE("called\n");

	if (ctx->initialized) {
		if (ctx->usessl) {
			mbedtls_ssl_close_notify(&ctx->ssl);
		}

		// mbedtls_net_free() という名前だが実は close。
		mbedtls_net_free(&ctx->net);
	}
	return 0;
}

// shutdown をします。
int
mtls_shutdown(mtlsctx_t* ctx, int how)
{
	int rv = 0;
	if (ctx->usessl == 0) {
		rv = shutdown(ctx->net.fd, how);
	}
	return rv;
}

int
mtls_read(mtlsctx_t* ctx, void* buf, int len)
{
	int rv;

	TRACE("called\n");

	if (ctx->usessl == 0) {
		rv = mbedtls_net_recv(&ctx->net, buf, len);
	} else {
		rv = mbedtls_ssl_read(&ctx->ssl, buf, len);
	}

	if (rv == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) {
		// EOF
		TRACE("EOF\n");
		return 0;
	}
	if (rv < 0) {
		ERROR("mtls_read failed: %s\n", mtls_errmsg(rv));
		return rv;
	}

	TRACE("%d bytes\n", rv);
	return rv;
}

int
mtls_write(mtlsctx_t* ctx, const void* buf, int len)
{
	int rv;

	TRACE("called\n");

	if (ctx->usessl == 0) {
		rv = mbedtls_net_send(&ctx->net, buf, len);
	} else {
		rv = mbedtls_ssl_write(&ctx->ssl, buf, len);
	}

	if (rv < 0) {
		ERROR("mtls_write failed: %s\n", mtls_errmsg(rv));
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
	mtlsctx_t* ctx = mtls_alloc();
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
	mtls_set_debuglevel(debuglevel);

	fprintf(stderr, "Test to %s:%s\n", hostname, servname);

	if (mtls_init(ctx) != 0) {
		errx(1, "mtls_init failed");
	}

	if (use_rsa_only) {
		mtls_usersa(ctx);
	}

	if (strcmp(servname, "443") == 0) {
		mtls_setssl(ctx, 1);
	}

	if (mtls_connect(ctx, hostname, servname) != 0) {
		errx(1, "mtls_connect failed");
	}

	char req[128];
	sprintf(req, "GET / HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n", hostname);
	TRACE("write buf=|%s|", req);

	r = mtls_write(ctx, req, strlen(req));
	if (r < 0) {
		errx(1, "write failed %d", r);
	}

	for (;;) {
		char buf[1024];
		r = mtls_read(ctx, buf, sizeof(buf) - 1);
		if (r == MBEDTLS_ERR_SSL_WANT_READ || r == MBEDTLS_ERR_SSL_WANT_WRITE) {
			continue;
		}
		if (r == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) {
			break;
		}
		if (r < 0) {
			errx(1, "mtls_read failed %d", r);
		}
		if (r == 0) {
			break;
		}
		buf[r] = '\0';
		fprintf(stderr, "%s", buf);
	}

	mtls_close(ctx);
	mtls_free(ctx);

	return 0;
}

#endif

