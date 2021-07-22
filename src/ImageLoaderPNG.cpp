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

#include "ImageLoaderPNG.h"
#include "StringUtil.h"
#include "subr.h"
#include <cstring>
#include <errno.h>
#include <png.h>

static void png_read(png_structp png, png_bytep data, png_size_t length);

// コンストラクタ
ImageLoaderPNG::ImageLoaderPNG(InputStream *stream_, const Diag& diag_)
	: inherited(stream_, diag_)
{
}

// デストラクタ
ImageLoaderPNG::~ImageLoaderPNG()
{
}

// stream が PNG なら true を返す。
bool
ImageLoaderPNG::Check() const
{
	uint8 magic[4];

	auto n = stream->Peek(&magic, sizeof(magic));
	if (n < sizeof(magic)) {
		Trace(diag, "%s: Read(magic) failed: %s", __method__, strerrno());
		return false;
	}
	// マジックを確認
	if (png_sig_cmp(magic, 0, sizeof(magic)) != 0) {
		Trace(diag, "%s: Bad magic", __method__);
		return false;
	}
	Trace(diag, "%s: OK", __method__);
	return true;
}

// stream から画像をロードする。
bool
ImageLoaderPNG::Load(Image& img)
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
	std::vector<png_bytep> lines;
	bool rv;

	rv = false;
	png = NULL;
	info = NULL;

	png = png_create_read_struct(PNG_LIBPNG_VER_STRING,
		NULL, NULL, NULL);
	if (__predict_false(png == NULL)) {
		return false;
	}
	info = png_create_info_struct(png);
	if (__predict_false(info == NULL)) {
		goto done;
	}

	// libpng 内のエラーからは大域ジャンプで戻ってくるらしい…
	if (setjmp(png_jmpbuf(png))) {
		goto done;
	}

	// コールバック設定
	png_set_read_fn(png, stream, png_read);

	// ヘッダを読み込む
	png_read_info(png, info);
	png_get_IHDR(png, info, &width, &height, &bitdepth,
		&color_type, &interlace_type, &compression_type, &filter_type);
	Debug(diag, "IHDR width=%d height=%d bitdepth=%d", width, height, bitdepth);
	Debug(diag, "IHDR colortype=%s interlace=%d compression=%d filter=%d",
		ColorType2str(color_type).c_str(), interlace_type,
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
	// Alpha 無視
	png_set_strip_alpha(png);

	img.Create(width, height);

	// スキャンラインメモリのポインタ配列
	lines.resize(img.GetHeight());
	for (int y = 0, end = lines.size(); y < end; y++) {
		lines[y] = img.buf.data() + (y * img.GetStride());
	}

	png_read_image(png, lines.data());
	png_read_end(png, info);
	rv = true;

 done:
	png_destroy_read_struct(&png, &info, (png_infopp)NULL);
	return rv;
}

// PNG の color type
/*static*/ std::string
ImageLoaderPNG::ColorType2str(int type)
{
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
		return string_format("%d(?)", type);
	}
}

// コールバック
static void
png_read(png_structp png, png_bytep data, png_size_t length)
{
	InputStream *stream = (InputStream *)png_get_io_ptr(png);

	size_t total = 0;
	while (total < length) {
		auto r = stream->Read((char *)data + total, length - total);
		if (r <= 0)
			break;
		total += r;
	}
}
