/* vi:set ts=4: */
/*
 * Copyright (C) 2024-2026 Tetsuya Isaki
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
// 画像処理 (private)
//

#ifndef sayaka_image_priv_h
#define sayaka_image_priv_h

#include "image.h"
#include "missing_endian.h"

// テストなどで一時的に無効にしたい場合のため。
#define USE_BLURHASH
#define USE_BUILTIN_BMP
#if defined(HAVE_BUILTIN_PNM)
#define USE_BUILTIN_PNM
#endif
#if defined(HAVE_GIFLIB)
#define USE_GIFLIB
#endif
#if defined(HAVE_LIBJPEG)
#define USE_LIBJPEG
#endif
#if defined(HAVE_LIBJXL)
#define USE_LIBJXL
#endif
#if defined(HAVE_LIBPNG)
#define USE_LIBPNG
#endif
#if defined(WITH_STB_IMAGE)
#define USE_STB_IMAGE
#endif
#if defined(HAVE_LIBTIFF)
#define USE_LIBTIFF
#endif
#if defined(HAVE_LIBWEBP)
#define USE_LIBWEBP
#endif

// image.c
extern struct image *image_create(uint, uint, uint);

// image_*.c
typedef bool (*image_match_t)(FILE *, const struct diag *);
typedef struct image *(*image_read_t)(FILE *, const image_read_hint *,
	const struct diag *);
#define IMAGE_HANDLER(name)	\
	extern bool image_##name##_match(FILE *, const struct diag *);	\
	extern struct image *image_##name##_read(FILE *,	\
		const image_read_hint *, const struct diag *)

IMAGE_HANDLER(bmp);
IMAGE_HANDLER(gif);
IMAGE_HANDLER(jpeg);
IMAGE_HANDLER(jxl);
IMAGE_HANDLER(png);
IMAGE_HANDLER(pnm1);
IMAGE_HANDLER(pnm2);
IMAGE_HANDLER(pnm3);
IMAGE_HANDLER(pnm5);
IMAGE_HANDLER(pnm6);
IMAGE_HANDLER(stb);
IMAGE_HANDLER(tiff);
IMAGE_HANDLER(webp);

#undef IMAGE_HANDLER

#endif // !sayaka_image_priv_h
