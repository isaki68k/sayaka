/* vi:set ts=4: */
/*
 * Copyright (C) 2023 Tetsuya Isaki
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
// stb_image による読み込み
//

#include "common.h"
#include "image_priv.h"

// 外部ライブラリでサポートしているフォーマットを除く。
#if defined(USE_LIBGIF)
#define STBI_NO_GIF
#endif
#if defined(USE_LIBJPEG)
#define STBI_NO_JPEG
#endif
#if defined(USE_LIBPNG)
#define STBI_NO_PNG
#endif

// stb is too dirty against strict warnings...
#if defined(__clang__)
_Pragma("clang diagnostic push")
_Pragma("clang diagnostic ignored \"-Wcast-qual\"")
_Pragma("clang diagnostic ignored \"-Wdisabled-macro-expansion\"")
#else
_Pragma("GCC diagnostic push")
_Pragma("GCC diagnostic ignored \"-Wcast-qual\"")
_Pragma("GCC diagnostic ignored \"-Wunused-but-set-variable\"")
#endif

// I use the stb_image as public domain.
#define STB_IMAGE_IMPLEMENTATION
#define STBI_FAILURE_USERMSG
#include "stb/stb_image.h"

#if defined(__clang__)
_Pragma("clang diagnostic pop")
#else
_Pragma("GCC diagnostic pop")
#endif

bool
image_stb_match(FILE *fp, const diag *diag)
{
	int ok;
	int w;
	int h;
	int ch;

	ok = stbi_info_from_file(fp, &w, &h, &ch);
	if (ok) {
		Debug(diag, "%s: OK", __func__);
	}
	return ok;
}

image *
image_stb_read(FILE *fp, const image_opt *opt, const diag *diag)
{
	image *img;
	stbi_uc *data;
	int width;
	int height;
	int nch;

	data = stbi_load_from_file(fp, &width, &height, &nch, 3);
	if (data == NULL) {
		return NULL;
	}

	img = image_create(width, height, 3);
	if (img != NULL) {
		memcpy(img->buf, data, width * height * 3);
	}

	stbi_image_free(data);
	return img;
}
