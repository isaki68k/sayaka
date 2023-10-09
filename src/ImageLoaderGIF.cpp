/*
 * Copyright (C) 2022 Tetsuya Isaki
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

#include "ImageLoaderGIF.h"
#include "PeekableStream.h"
#include "subr.h"
#include <cstring>
#include <errno.h>
#include <gif_lib.h>

static int gif_read(GifFileType *, GifByteType *, int);

// コンストラクタ
ImageLoaderGIF::ImageLoaderGIF(PeekableStream *stream_, const Diag& diag_)
	: inherited(stream_, diag_)
{
}

// デストラクタ
ImageLoaderGIF::~ImageLoaderGIF()
{
}

// stream が GIF なら true を返す。
bool
ImageLoaderGIF::Check() const
{
	char magic[4];

	auto n = stream->Peek(&magic, sizeof(magic));
	if (n < sizeof(magic)) {
		Trace(diag, "%s: Peek() failed: %s", __method__, strerrno());
		return false;
	}
	// マジックを確認
	if (strncmp(&magic[0], "GIF8", 4) != 0) {
		Trace(diag, "%s: Bad magic", __method__);
		return false;
	}
	Trace(diag, "%s: OK", __method__);
	return true;
}

// stream から画像をロードする。
bool
ImageLoaderGIF::Load(Image& img)
{
	GifFileType *gif;
	const SavedImage *image;
	const GifImageDesc *desc;
	const ColorMapObject *cmap;
	uint8 *d;
	int errcode;
	bool rv = false;

	// コールバックを指定してオープン
	gif = DGifOpen(stream, gif_read, &errcode);
	if (gif == NULL) {
		return false;
	}

	// 展開
	errcode = DGifSlurp(gif);
	if (errcode != GIF_OK) {
		Trace(diag, "%s: DGifSlurp failed: %s", __method__,
			GifErrorString(gif->Error));
		goto abort;
	}

	img.Create(gif->SWidth, gif->SHeight);

	// アニメーション GIF でも1枚目だけ
	image = &gif->SavedImages[0];
	desc = &image->ImageDesc;
	cmap = desc->ColorMap ?: gif->SColorMap;

	// RasterBits[] に Width*Height のカラーコードが並んでいる
	d = img.GetBuf();
	for (int y = 0; y < desc->Height; y++) {
		for (int x = 0; x < desc->Width; x++) {
			int c = image->RasterBits[y * desc->Width + x];
			GifColorType rgb = cmap->Colors[c];
			*d++ = rgb.Red;
			*d++ = rgb.Green;
			*d++ = rgb.Blue;
		}
	}
	rv = true;

 abort:
	DGifCloseFile(gif, &errcode);
	return rv;
}

// 読み込みコールバック
int
gif_read(GifFileType *gf, GifByteType *dst, int length)
{
	Stream *stream = (Stream *)gf->UserData;

	size_t total = 0;
	while (total < length) {
		auto r = stream->Read((char *)dst + total, length - total);
		if (r <= 0)
			break;
		total += r;
	}
	return total;
}
