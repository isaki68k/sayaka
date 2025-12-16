/* vi:set ts=4: */
/*
 * Copyright (C) 2020-2025 Tetsuya Isaki
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
// エンディアン関係
//

// エンディアン関係が定義してあるヘッダと、定義内容が全員微妙に異なる。
// バイトオーダは BYTE_ORDER、LITTLE_ENDIAN、BIG_ENDIAN の3つの定義。
//
// OS		| ヘッダ			 | バイトオーダ	| htobeNN() 系マクロ
// ---------+--------------------+--------------+-------------------
// NetBSD	| <sys/endian.h>	 | _BYTE_ORDER	| あり
// FreeBSD	| <sys/endian.h>	 |				| あり
// OpenBSD	| <sys/endian.h>	 |				| あり(*1)
// Linux	| <endian.h>		 | BYTE_ORDER	| あり
// OSX		| <machine/endian.h> | BYTE_ORDER	| なし
//
// *1: OpenBSD は本来 betohNN() のように NN は常に後置だったが beNNtoh()
//     形式も使える。

#ifndef sayaka_missing_endian_h
#define sayaka_missing_endian_h

#if defined(HAVE_ENDIAN_H)
#include <endian.h>					// Linux
#endif
#if defined(HAVE_SYS_ENDIAN_H)
#include <sys/endian.h>				// *BSD
#endif
#if defined(HAVE_MACHINE_ENDIAN_H)
#include <machine/endian.h>			// OSX
#endif

#if defined(BYTE_ORDER)
#elif defined(_BYTE_ORDER)
# define BYTE_ORDER	_BYTE_ORDER
#elif defined(__BYTE_ORDER)
# define BYTE_ORDER	__BYTE_ORDER
#else
# error no BYTE_ORDER defined
#endif

#if defined(BIG_ENDIAN)
#elif defined(_BIG_ENDIAN)
# define BIG_ENDIAN	_BIG_ENDIAN
#elif defined(__BIG_ENDIAN)
# define BIG_ENDIAN	__BIG_ENDIAN
#else
# error no BIG_ENDIAN defined
#endif

#if defined(LITTLE_ENDIAN)
#elif defined(_LITTLE_ENDIAN)
# define LITTLE_ENDIAN	_LITTLE_ENDIAN
#elif defined(__LITTLE_ENDIAN)
# define LITTLE_ENDIAN	__LITTLE_ENDIAN
#else
# error no LITTLE_ENDIAN defined
#endif

// htobeNN() 系マクロ。
// 代表として htobe16() の有無だけチェックしている。
#if !defined(HAVE_HTOBE16)
# if defined(HAVE_LIBKERN_OSBYTEORDER_H)
#  include <libkern/OSByteOrder.h>
#  define htobe16(x)	OSSwapHostToBigInt16(x)
#  define htobe32(x)	OSSwapHostToBigInt32(x)
#  define htobe64(x)	OSSwapHostToBigInt64(x)
#  define htole16(x)	OSSwapHostToLittleInt16(x)
#  define htole32(x)	OSSwapHostToLittleInt32(x)
#  define htole64(x)	OSSwapHostToLittleInt64(x)
#  define be16toh(x)	OSSwapBigToHostInt16(x)
#  define be32toh(x)	OSSwapBigToHostInt32(x)
#  define be64toh(x)	OSSwapBigToHostInt64(x)
#  define le16toh(x)	OSSwapLittleToHostInt16(x)
#  define le32toh(x)	OSSwapLittleToHostInt32(x)
#  define le64toh(x)	OSSwapLittleToHostInt64(x)
# else
#  error No htobeNN() family defined
# endif
#endif // !HAVE_HTOBE16

#endif // !sayaka_missing_endian_h
