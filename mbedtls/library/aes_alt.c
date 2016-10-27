/*
 * Copyright (C) 2016 isaki@NetBSD.org
 */
/*
 *  FIPS-197 compliant AES implementation
 *
 *  Copyright (C) 2006-2015, ARM Limited, All Rights Reserved
 *  SPDX-License-Identifier: Apache-2.0
 *
 *  Licensed under the Apache License, Version 2.0 (the "License"); you may
 *  not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 *  WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 *  This file is part of mbed TLS (https://tls.mbed.org)
 */
/*
 *  The AES block cipher was designed by Vincent Rijmen and Joan Daemen.
 *
 *  http://csrc.nist.gov/encryption/aes/rijndael/Rijndael.pdf
 *  http://csrc.nist.gov/publications/fips/fips197/fips-197.pdf
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
mbedtls_aes_crypt_ecb(mbedtls_aes_context *ctx,
	int mode,
	const unsigned char input[16],
	unsigned char output[16])
{
#if defined(MBEDTLS_AESNI_C) && defined(MBEDTLS_HAVE_X86_64)
	if( mbedtls_aesni_has_support( MBEDTLS_AESNI_AES ) )
		return( mbedtls_aesni_crypt_ecb( ctx, mode, input, output ) );
#endif

#if defined(MBEDTLS_PADLOCK_C) && defined(MBEDTLS_HAVE_X86)
	if( aes_padlock_ace ) {
		if( mbedtls_padlock_xcryptecb( ctx, mode, input, output ) == 0 )
			return 0;

		// If padlock data misaligned, we just fall back to
		// unaccelerated mode
	}
#endif

	if( mode == MBEDTLS_AES_ENCRYPT )
		mbedtls_aes_encrypt( ctx, input, output );
	else
		mbedtls_aes_decrypt( ctx, input, output );

	return( 0 );
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
	int i;
	unsigned char temp[16];

	if( length % 16 )
		return( MBEDTLS_ERR_AES_INVALID_INPUT_LENGTH );

#if defined(MBEDTLS_PADLOCK_C) && defined(MBEDTLS_HAVE_X86)
	if( aes_padlock_ace )
	{
		if( mbedtls_padlock_xcryptcbc( ctx, mode, length, iv, input, output ) == 0 )
			return( 0 );

		// If padlock data misaligned, we just fall back to
		// unaccelerated mode
		//
	}
#endif

	if( mode == MBEDTLS_AES_DECRYPT )
	{
		while( length > 0 )
		{
			memcpy( temp, input, 16 );
			mbedtls_aes_crypt_ecb( ctx, mode, input, output );

			for( i = 0; i < 16; i++ )
				output[i] = (unsigned char)( output[i] ^ iv[i] );

			memcpy( iv, temp, 16 );

			input  += 16;
			output += 16;
			length -= 16;
		}
	}
	else
	{
		while( length > 0 )
		{
			for( i = 0; i < 16; i++ )
				output[i] = (unsigned char)( input[i] ^ iv[i] );

			mbedtls_aes_crypt_ecb( ctx, mode, output, output );
			memcpy( iv, output, 16 );

			input  += 16;
			output += 16;
			length -= 16;
		}
	}

	return( 0 );
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
	int c;
	size_t n = *iv_off;

	if( mode == MBEDTLS_AES_DECRYPT )
	{
		while( length-- )
		{
			if( n == 0 )
				mbedtls_aes_crypt_ecb( ctx, MBEDTLS_AES_ENCRYPT, iv, iv );

			c = *input++;
			*output++ = (unsigned char)( c ^ iv[n] );
			iv[n] = (unsigned char) c;

			n = ( n + 1 ) & 0x0F;
		}
	}
	else
	{
		while( length-- )
		{
			if( n == 0 )
				mbedtls_aes_crypt_ecb( ctx, MBEDTLS_AES_ENCRYPT, iv, iv );

			iv[n] = *output++ = (unsigned char)( iv[n] ^ *input++ );

			n = ( n + 1 ) & 0x0F;
		}
	}

	*iv_off = n;

	return( 0 );
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
	int c, i;
	size_t n = *nc_off;

	while( length-- )
	{
		if( n == 0 ) {
			mbedtls_aes_crypt_ecb( ctx, MBEDTLS_AES_ENCRYPT, nonce_counter, stream_block );

			for( i = 16; i > 0; i-- )
				if( ++nonce_counter[i - 1] != 0 )
					break;
		}
		c = *input++;
		*output++ = (unsigned char)( c ^ stream_block[n] );

		n = ( n + 1 ) & 0x0F;
	}

	*nc_off = n;

	return( 0 );
}
#endif

#endif /* MBEDTLS_AES_C && MBEDTLS_AES_ALT */
