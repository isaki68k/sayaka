/* vi:set ts=4: */
/*
 * Copyright (C) 2021-2024 Tetsuya Isaki
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

#ifndef sayaka_header_h
#define sayaka_header_h

#define __USE_MISC

#include "config.h"
#include <inttypes.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sys/types.h>

#if defined(HAVE_BSD_BSD_H)
#include <bsd/bsd.h>
#endif

#if !defined(__packed)
# if defined(HAVE___ATTRIBUTE_PACKED)
#  define __packed	__attribute__((__packed__))
# else
#  define __packed
# endif
#endif

// FreeBSD の __predict_true/false は定義がいまいちで使えない。
// __builtin_expect() があることが分かればどの環境でも自前で定義できるので
// 既存定義は取り消す。
#undef  __predict_true
#undef  __predict_false
#if defined(HAVE___BUILTIN_EXPECT)
# define __predict_true(exp)	__builtin_expect((exp) != 0, 1)
# define __predict_false(exp)	__builtin_expect((exp) != 0, 0)
#else
# define __predict_true(exp)	(exp)
# define __predict_false(exp)	(exp)
#endif

#if !defined(__unreachable)
#if defined(HAVE___BUILTIN_UNREACHABLE)
# define __unreachable()	__builtin_unreachable()
#else
# define __unreachable()
#endif
#endif

#if !defined(__unused)
# if defined(HAVE___ATTRIBUTE_UNUSED)
#  define __unused	__attribute__((__unused__))
# else
#  define __unused
# endif
#endif

#define MAX(a, b)	({	\
	__typeof__(a) a_ = (a);	\
	__typeof__(b) b_ = (b);	\
	a_ > b_ ? a_ : b_;	\
})

#define MIN(a, b)	({	\
	__typeof__(a) a_ = (a);	\
	__typeof__(b) b_ = (b);	\
	a_ < b_ ? a_ : b_;	\
})

#ifndef UNCONST
#define UNCONST(p)	((void *)(uintptr_t)(const void *)(p))
#endif

#ifndef UNVOLATILE
#define UNVOLATILE(p)	((void *)(uintptr_t)(volatile void *)(p))
#endif

#ifndef countof
#define countof(x)	(sizeof(x) / sizeof(x[0]))
#endif

#ifndef howmany
#define howmany(x, y)	(((x) + (y) - 1) / (y))
#endif
#ifndef roundup
#define roundup(x, y)	((((x) + ((y) - 1)) / (y)) * (y))
#endif
#ifndef rounddown
#define rounddown(x, y)	(((x) / (y)) * (y))
#endif

typedef unsigned int	uint;
typedef uint8_t		uint8;
typedef uint16_t	uint16;
typedef uint32_t	uint32;
typedef uint64_t	uint64;
typedef int8_t		int8;
typedef int16_t		int16;
typedef int32_t		int32;
typedef int64_t		int64;

// iconv() の第2引数の型は OS によって違う…
#if defined(ICONV_HAS_CONST_SRC)
#define ICONV(cd, s, slen, d, dlen)	iconv((cd), (s), (slen), (d), (dlen))
#else
#define ICONV(cd, s, slen, d, dlen)	iconv((cd), UNCONST(s), (slen), (d), (dlen))
#endif

// ざっくり遅マシン判定。
#if defined(__hppa__)	|| \
    defined(__m68k__)	|| \
    defined(__sh3__)	|| \
    (defined(__sparc__) && !defined(__sparc64__))	|| \
    defined(__vax__)
#define SLOW_ARCH
#endif

#endif // !sayaka_header_h
