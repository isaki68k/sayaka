/*
 * Copyright (C) 2021 Tetsuya Isaki
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

#include "config.h"
#include <cstdint>
#include <sys/types.h>

using int8    = int8_t;
using int16   = int16_t;
using int32   = int32_t;
using int64   = int64_t;
using uint8   = uint8_t;
using uint16  = uint16_t;
using uint32  = uint32_t;
using uint64  = uint64_t;
using unichar = uint32_t;

#if defined(HAVE_ENDIAN_H)
#include <endian.h>
#elif defined(HAVE_SYS_ENDIAN_H)
#include <sys/endian.h>
#else
#include "missing_endian.h"
#endif

#if !defined(__printflike)
#if defined(HAVE___ATTRIBUTE_FORMAT)
# define __printflike(a,b)	__attribute__((__format__(__printf__, (a), (b))))
#else
# define __printflike(a,b)
#endif
#endif

#if !defined(__predict_true)
#if defined(HAVE___BUILTIN_EXPECT)
# define __predict_true(exp)	__builtin_expect((exp) != 0, 1)
# define __predict_false(exp)	__builtin_expect((exp) != 0, 0)
#else
# define __predict_true(exp)	(exp)
# define __predict_false(exp)	(exp)
#endif
#endif

// iconv() の第2引数の型は OS によって違う…
#if defined(HAVE_ICONV_CONST)
#define ICONV(cd, s, slen, d, dlen)	iconv((cd), (s), (slen), (d), (dlen))
#else
#define ICONV(cd, s, slen, d, dlen)	\
	iconv((cd), (char **)(s), (slen), (d), (dlen))
#endif

static const int ColorFixedX68k = -1;
