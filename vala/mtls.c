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

#if 0
#define ERRORLOG(x...)	fprintf(stderr, x)
#else
#define ERRORLOG(x...)	(void)0
#endif


// private
int mtls_internal_free(mtlsctx_t* ctx);


////////////////////////////////////

// mtlsctx_t のメモリを確保します。
// alloc と init を分離しないと、エラー通知が難しいので
// 分離されています。
// alloc で確保したメモリは free で開放してください。
mtlsctx_t*
mtls_alloc()
{
	ERRORLOG("alloc called\n");
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

	mbedtls_net_init(&ctx->fd);
	mbedtls_ssl_init(&ctx->ssl);
	mbedtls_ssl_config_init(&ctx->conf);
	mbedtls_x509_crt_init(&ctx->cacert);
	mbedtls_ctr_drbg_init(&ctx->ctr_drbg);

	// init RNG
	mbedtls_entropy_init(&ctx->entropy);
	r = mbedtls_ctr_drbg_seed(&ctx->ctr_drbg, mbedtls_entropy_func,
			&ctx->entropy, "a", 1);
	if (r != 0) {
		ERRORLOG("mbedtls_entropy_init=%d", r);
		goto errexit;
	}

	// init CA root
	r = mbedtls_x509_crt_parse(&ctx->cacert,
			(const unsigned char*)mbedtls_test_cas_pem,
			mbedtls_test_cas_pem_len);
	if (r < 0) {
		ERRORLOG("mbedtls_x509_crt_parse=%d", r);
		goto errexit;
	}

	// TLS config
	r = mbedtls_ssl_config_defaults(&ctx->conf,
			MBEDTLS_SSL_IS_CLIENT,
			MBEDTLS_SSL_TRANSPORT_STREAM,
			MBEDTLS_SSL_PRESET_DEFAULT);
	if (r != 0) {
		ERRORLOG("mbedtls_ssl_config_defaults=%d", r);
		goto errexit;
	}

	mbedtls_ssl_conf_authmode(&ctx->conf, MBEDTLS_SSL_VERIFY_OPTIONAL);
	mbedtls_ssl_conf_ca_chain(&ctx->conf, &ctx->cacert, NULL);
	mbedtls_ssl_conf_rng(&ctx->conf, mbedtls_ctr_drbg_random, &ctx->ctr_drbg);

	r = mbedtls_ssl_setup(&ctx->ssl, &ctx->conf);
	if (r != 0) {
		ERRORLOG("mbedtls_ssl_setup=%d", r);
		goto errexit;
	}

	mbedtls_ssl_set_bio(&ctx->ssl, &ctx->fd, mbedtls_net_send, mbedtls_net_recv, NULL);

	ERRORLOG("init OK\n");
	return 0;

errexit:
	// cleanup
	mtls_internal_free(ctx);
	ERRORLOG("init NG\n");
	return -1;
}

// 内部のフィールドを開放します。
// private
int
mtls_internal_free(mtlsctx_t* ctx)
{
	mbedtls_net_free(&ctx->fd);
	mbedtls_x509_crt_free(&ctx->cacert);
	mbedtls_ssl_free(&ctx->ssl);
	mbedtls_ssl_config_free(&ctx->conf);
	mbedtls_ctr_drbg_free(&ctx->ctr_drbg);
	mbedtls_entropy_free(&ctx->entropy);
	ERRORLOG("internal_free OK\n");
	return 0;
}

// 接続を閉じて、コンテキストを init する前の状態にします。
int
mtls_close(mtlsctx_t* ctx)
{
	mbedtls_ssl_close_notify(&ctx->ssl);
	mtls_internal_free(ctx);
	ERRORLOG("close OK\n");
	return 0;
}

// 接続します。
int
mtls_connect(mtlsctx_t* ctx, const char* hostname, const char *servname)
{
	ERRORLOG("connect called: %s,%s\n", hostname, servname);
	int r;

	r = mbedtls_net_connect(&ctx->fd, hostname, servname,
			MBEDTLS_NET_PROTO_TCP);
	if (r != 0) {
		ERRORLOG("mbedtls_net_connect=%d", r);
		return -1;
	}

	while ((r = mbedtls_ssl_handshake(&ctx->ssl)) != 0) {
		if (r != MBEDTLS_ERR_SSL_WANT_READ && r != MBEDTLS_ERR_SSL_WANT_WRITE) {
			ERRORLOG("mbedtls_ssl_handshake=%d", r);
			return -1;
		}
	}

	ERRORLOG("connect OK\n");

	return 0;
}

#if !defined(INLINE_RW)
int
mtls_read(mtlsctx_t* ctx, void* buf, int len)
{
	ERRORLOG("read called\n");
	int rv = mbedtls_ssl_read(&ctx->ssl, buf, len);
	if (rv > 0) {
		((char*)buf)[rv] = 0;
		ERRORLOG("read=%s\n", buf);
	} else {
		// error to close
		rv = 0;
	}
	return rv;
}

int
mtls_write(mtlsctx_t* ctx, const void* buf, int len)
{
	ERRORLOG("write called\n");
	((char*)buf)[len] = 0;
	ERRORLOG("write=%s\n", buf);
	return mbedtls_ssl_write(&ctx->ssl, buf, len);
}
#endif

//#define TEST
#if defined(TEST)

int
main(int ac, char *av[])
{
	mtlsctx_t* ctx = mtls_alloc();
	int r;

	if (mtls_init(ctx) != 0) {
		return 1;
	}

	printf("init ok\n");

	const char* hostname = "www.google.com";
	const char* servname = "443";
	if (mtls_connect(ctx, hostname, servname) != 0) {
		return 1;
	}

	printf("connect ok\n");

	const char* req = "GET / HTTP/1.1\r\nHost: www.google.com\r\nConnection: close\r\n\r\n";
	printf("write=%s\n", req);

	r = mtls_write(ctx, req, strlen(req));
	if (r < 0) {
		ERRORLOG("write=%d", r);
		return 1;
	}

	printf("write ok\n");


	for (;;) {
		char buf[1024];
		r = mtls_read(ctx, buf, sizeof(buf));
		if (r == MBEDTLS_ERR_SSL_WANT_READ || r == MBEDTLS_ERR_SSL_WANT_WRITE) {
			continue;
		}
		if (r == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) {
			break;
		}
		if (r < 0) {
			ERRORLOG("read=%d", r);
			return 1;
		}
		if (r == 0) {
			break;
		}
		fwrite(buf, r, 1, stdout);
	}

	mtls_close(ctx);
	mtls_free(ctx);

	return 0;
}

#endif

