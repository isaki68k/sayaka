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

//
// PNG 読み込み
//

#include "common.h"
#include "image_priv.h"
#include <png.h>

static const char *colortype2str(int type);
static void png_read_cb(png_structp png, png_bytep data, png_size_t length);

bool
image_png_match(FILE *fp, const diag *diag)
{
	uint8 magic[4];

	size_t n = fread(&magic[0], 1, sizeof(magic), fp);
	if (n < sizeof(magic)) {
		Debug(diag, "%s: fread failed: %s", __func__, strerrno());
		return false;
	}

	// マジックを確認。
	if (png_sig_cmp(magic, 0, sizeof(magic)) != 0) {
		return false;
	}

	Debug(diag, "%s: OK", __func__);
	return true;
}

image *
image_png_read(FILE *fp, const diag *diag)
{
	png_structp png;
	png_infop info;
	png_uint_32 width;
	png_uint_32 height;
	int bitdepth;
	int color_type;
	int interlace_type;
	int compression_type;
	int filter_type;
	uint8 **lines;
	image *img;

	png = png_create_read_struct(PNG_LIBPNG_VER_STRING,
		NULL, NULL, NULL);
	if (__predict_false(png == NULL)) {
		return NULL;
	}

	info = png_create_info_struct(png);
	if (__predict_false(info == NULL)) {
		goto abort;
	}

	// libpng 内のエラーからは大域ジャンプで戻ってくるらしい…
	if (setjmp(png_jmpbuf(png))) {
		goto abort;
	}

	// コールバック設定。
	png_set_read_fn(png, fp, png_read_cb);

	// ヘッダを読み込む。
	png_read_info(png, info);
	png_get_IHDR(png, info, &width, &height, &bitdepth,
		&color_type, &interlace_type, &compression_type, &filter_type);
	Debug(diag, "%s: IHDR width=%d height=%d bitdepth=%d",
		__func__, width, height, bitdepth);
	Debug(diag, "%s: IHDR colortype=%s interlace=%d compress=%d filter=%d",
		__func__,
		colortype2str(color_type), interlace_type,
		compression_type, filter_type);

	// color_type によっていろいろ設定が必要。
	// see libpng(4)
	if (color_type == PNG_COLOR_TYPE_PALETTE) {
		png_set_palette_to_rgb(png);
	}
	if ((color_type & PNG_COLOR_MASK_COLOR) == 0) {
		if (bitdepth < 8) {
			png_set_expand_gray_1_2_4_to_8(png);
		}
		png_set_gray_to_rgb(png);
	}
	// Alpha 無視。
	png_set_strip_alpha(png);

	img = image_create(width, height, 3);

	// スキャンラインメモリのポインタ配列。
	lines = malloc(sizeof(char *) * height);
	for (int y = 0; y < height; y++) {
		lines[y] = img->buf + y * image_get_stride(img);
	}

	png_read_image(png, lines);
	png_read_end(png, info);
	free(lines);
	return img;

 abort:
	png_destroy_read_struct(&png, &info, (png_infopp)NULL);
	return NULL;
}

// PNG の color type のデバッグ表示用。
static const char *
colortype2str(int type)
{
	static char buf[16];

	switch (type) {
	 case PNG_COLOR_TYPE_GRAY:
		return "Gray";
	 case PNG_COLOR_TYPE_PALETTE:
		return "Palette";
	 case PNG_COLOR_TYPE_RGB:
		return "RGB";
	 case PNG_COLOR_TYPE_RGBA:
		return "RGBA";
	 case PNG_COLOR_TYPE_GRAY_ALPHA:
		return "GrayA";
	 default:
		snprintf(buf, sizeof(buf), "%d(?)", type);
		return buf;
	}
}

// コールバック。
static void
png_read_cb(png_structp png, png_bytep data, png_size_t length)
{
	FILE *fp = (FILE *)png_get_io_ptr(png);

	size_t total = 0;
	while (total < length) {
		size_t r = fread((char *)data + total, 1, length - total, fp);
		if (r < 1)
			break;
		total += r;
	}
}
