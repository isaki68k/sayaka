/*
 * Copyright (C) 2016 isaki@NetBSD.org
 */

#if !defined(MBEDTLS_CONFIG_FILE)
#include "mbedtls/config.h"
#else
#include MBEDTLS_CONFIG_FILE
#endif

#if defined(MBEDTLS_AES_C) && defined(MBEDTLS_AES_ALT)

#include <string.h>
#include "mbedtls/aes.h"

void
mbedtls_aes_init(mbedtls_aes_context *ctx)
{
	memset(ctx, 0, sizeof(*ctx));
}

void
mbedtls_aes_free(mbedtls_aes_context *ctx)
{
	memset(ctx, 0, sizeof(*ctx));
}

int
mbedtls_aes_setkey_enc(mbedtls_aes_context *ctx,
	const unsigned char *key,
	unsigned int bits)
{
	return -1;
}

int
mbedtls_aes_setkey_dec(mbedtls_aes_context *ctx,
	const unsigned char *key,
	unsigned int bits)
{
	return -1;
}

void
mbedtls_aes_encrypt(mbedtls_aes_context *ctx,
	const unsigned char input[16],
	unsigned char output[16])
{
}

void
mbedtls_aes_decrypt(mbedtls_aes_context *ctx,
	const unsigned char input[16],
	unsigned char output[16])
{
}

int
mbedtls_aes_crypt_ecb(mbedtls_aes_context *ctx,
	int mode,
	const unsigned char input[16],
	unsigned char output[16])
{
	return -1;
}

#if defined(MBEDTLS_CIPHER_MODE_CBC)
int
mbedtls_aes_crypt_cbc(mbedtls_aes_context *ctx,
	int mode,
	size_t length,
	unsigned char iv[16],
	const unsigned char *input,
	unsigned char *output)
{
	return -1;
}
#endif

#if defined(MBEDTLS_CIPHER_MODE_CFB)
int
mbedtls_aes_crypt_cfb128( mbedtls_aes_context *ctx,
	int mode,
	size_t length,
	size_t *iv_off,
	unsigned char iv[16],
	const unsigned char *input,
	unsigned char *output)
{
	return -1;
}
#endif

#if defined(MBEDTLS_CIPHER_MODE_CTR)
int
mbedtls_aes_crypt_ctr(mbedtls_aes_context *ctx,
	size_t length,
	size_t *nc_off,
	unsigned char nonce_counter[16],
	unsigned char stream_block[16],
	const unsigned char *input,
	unsigned char *output)
{
	return -1;
}
#endif

#endif /* MBEDTLS_AES_C && MBEDTLS_AES_ALT */
