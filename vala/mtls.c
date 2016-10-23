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
#include "mtls.h"

//#define DEBUG

#if defined(DEBUG)
#include <sys/time.h>
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


// private
int mtls_internal_free(mtlsctx_t* ctx);


////////////////////////////////////

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


// mtlsctx_t のメモリを確保します。
// alloc と init を分離しないと、エラー通知が難しいので
// 分離されています。
// alloc で確保したメモリは free で開放してください。
mtlsctx_t*
mtls_alloc()
{
	TRACE("called\n");
	return (mtlsctx_t*)malloc(sizeof(mtlsctx_t));
}

// ctx のメモリを開放します。
// init が成功した後のコンテキストには、
// 呼び出す前に close を呼び出してください。
void
mtls_free(mtlsctx_t* ctx)
{
	free(ctx);
}

// コンテキストを初期化します。
int
mtls_init(mtlsctx_t* ctx)
{
	int r;

	TRACE("start\n");

	ctx->usessl = 0;
	mbedtls_net_init(&ctx->net);
	mbedtls_ssl_init(&ctx->ssl);
	mbedtls_ssl_config_init(&ctx->conf);
	mbedtls_x509_crt_init(&ctx->cacert);
	mbedtls_ctr_drbg_init(&ctx->ctr_drbg);

	// init RNG
	mbedtls_entropy_init(&ctx->entropy);
	r = mbedtls_ctr_drbg_seed(&ctx->ctr_drbg, mbedtls_entropy_func,
			&ctx->entropy, "a", 1);
	if (r != 0) {
		ERROR("mbedtls_entropy_init failed: %s\n", mtls_errmsg(r));
		goto errexit;
	}

	// init CA root
	r = mbedtls_x509_crt_parse(&ctx->cacert,
			(const unsigned char*)mbedtls_test_cas_pem,
			mbedtls_test_cas_pem_len);
	if (r < 0) {
		ERROR("mbedtls_x509_crt_parse failed: %s\n", mtls_errmsg(r));
		goto errexit;
	}

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
	mbedtls_ssl_conf_ca_chain(&ctx->conf, &ctx->cacert, NULL);
	mbedtls_ssl_conf_rng(&ctx->conf, mbedtls_ctr_drbg_random, &ctx->ctr_drbg);
	mbedtls_ssl_conf_dbg(&ctx->conf, debug_callback, stderr);

	r = mbedtls_ssl_setup(&ctx->ssl, &ctx->conf);
	if (r != 0) {
		ERROR("mbedtls_ssl_setup failed: %s\n", mtls_errmsg(r));
		goto errexit;
	}

	mbedtls_ssl_set_bio(&ctx->ssl, &ctx->net, mbedtls_net_send, mbedtls_net_recv, NULL);

	TRACE("done\n");
	return 0;

errexit:
	// cleanup
	mtls_internal_free(ctx);
	TRACE("NG\n");
	return -1;
}

// 内部のフィールドを開放します。
// private
int
mtls_internal_free(mtlsctx_t* ctx)
{
	mbedtls_net_free(&ctx->net);
	mbedtls_x509_crt_free(&ctx->cacert);
	mbedtls_ssl_free(&ctx->ssl);
	mbedtls_ssl_config_free(&ctx->conf);
	mbedtls_ctr_drbg_free(&ctx->ctr_drbg);
	mbedtls_entropy_free(&ctx->entropy);
	TRACE("internal_free OK\n");
	return 0;
}

// 接続を閉じて、コンテキストを init する前の状態にします。
int
mtls_close(mtlsctx_t* ctx)
{
	if (&ctx->usessl) {
		mbedtls_ssl_close_notify(&ctx->ssl);
	}
	mtls_internal_free(ctx);
	TRACE("OK\n");
	return 0;
}

// HTTPS かどうかを設定します。
// mtls_connect() より先に設定しておく必要があります。
void
mtls_setssl(mtlsctx_t* ctx, int value)
{
	ctx->usessl = value;
}


// 接続します。
int
mtls_connect(mtlsctx_t* ctx, const char* hostname, const char *servname)
{
	struct timeval start, end, result;

	TRACE_tv(&start, "called: %s:%s\n", hostname, servname);
	int r;

	r = mbedtls_net_connect(&ctx->net, hostname, servname,
			MBEDTLS_NET_PROTO_TCP);
	if (r != 0) {
		ERROR("mbedtls_net_connect failed: %s\n", mtls_errmsg(r));
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

int my_ciphersuites[] = {
	MBEDTLS_TLS_RSA_WITH_AES_128_CBC_SHA,
	0,
};

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
	mbedtls_debug_set_threshold(debuglevel);

	fprintf(stderr, "Test to %s:%s\n", hostname, servname);

	if (mtls_init(ctx) != 0) {
		errx(1, "mtls_init failed");
	}

	if (use_rsa_only) {
		// オレオレ ciphersuite リストを指定する。
		mbedtls_ssl_conf_ciphersuites(&ctx->conf, my_ciphersuites);
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

