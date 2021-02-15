/*
 * Copyright (C) 2020 Tetsuya Isaki
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

#pragma once

#if !defined(_LITTLE_ENDIAN)
# define _LITTLE_ENDIAN	1234
# define _BIG_ENDIAN	4321
#endif

#if !defined(_BYTE_ORDER)
# if defined(WORDS_BIGENDIAN)
#  define _BYTE_ORDER _BIG_ENDIAN
# else
#  define _BYTE_ORDER _LITTLE_ENDIAN
# endif
#endif

// __builtin_bswap* がないような環境があったらその時考える
#if !defined(HAVE___BUILTIN_BSWAP16) || \
    !defined(HAVE___BUILTIN_BSWAP32) || \
    !defined(HAVE___BUILTIN_BSWAP64)
#error No __builtin_bswap*
#endif

#define bswap16(x)	__builtin_bswap16(x)
#define bswap32(x)	__builtin_bswap32(x)
#define bswap64(x)	__builtin_bswap64(x)

#if _BYTE_ORDER == _BIG_ENDIAN
#define be16toh(x)	((uint16)(x))
#define be32toh(x)	((uint32)(x))
#define be64toh(x)	((uint64)(x))
#define le16toh(x)	bswap16((uint16)(x))
#define le32toh(x)	bswap32((uint32)(x))
#define le64toh(x)	bswap64((uint64)(x))
#else
#define be16toh(x)	bswap16((uint16)(x))
#define be32toh(x)	bswap32((uint32)(x))
#define be64toh(x)	bswap64((uint64)(x))
#define le16toh(x)	((uint16)(x))
#define le32toh(x)	((uint32)(x))
#define le64toh(x)	((uint64)(x))
#endif

#define htobe16(x)	be16toh(x)
#define htobe32(x)	be32toh(x)
#define htobe64(x)	be64toh(x)

#define htole16(x)	le16toh(x)
#define htole32(x)	le32toh(x)
#define htole64(x)	le64toh(x)
