/* vi:set ts=4: */
/*
 * Copyright (C) 2022-2025 Tetsuya Isaki
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
// GIF 読み込み
//

#include "common.h"
#include "image_priv.h"
#include <err.h>
#include <gif_lib.h>

static int gif_read(GifFileType *, GifByteType *, int);
static const char *disposal2str(int);

bool
image_gif_match(FILE *fp, const struct diag *diag)
{
	uint8 buf[4];

	size_t n = fread(&buf[0], sizeof(buf), 1, fp);
	if (n == 0) {
		Debug(diag, "%s: fread failed: %s", __func__, strerrno());
		return false;
	}

	// マジックを確認。
	if (buf[0] != 'G' ||
		buf[1] != 'I' ||
		buf[2] != 'F' ||
		buf[3] != '8')
	{
		return false;
	}

	return true;
}

struct image *
image_gif_read(FILE *fp, const image_read_hint *dummy, const struct diag *diag)
{
	GifFileType *gif;
	const SavedImage *src;
	const GifImageDesc *desc;
	const ColorMapObject *cmap;
	GraphicsControlBlock gcb;
	struct image *img;
	int errcode;
	int transparent_color;

	img = NULL;

	// コールバックを指定してオープン。
	gif = DGifOpen(fp, gif_read, &errcode);
	if (gif == NULL) {
		warnx("%s: DGifOpen failed: %d", __func__, errcode);
		return NULL;
	}

	// 展開。
	errcode = DGifSlurp(gif);
	if (errcode != GIF_OK) {
		warnx("%s: DGifSlurp failed: %s", __func__, GifErrorString(gif->Error));
		goto done;
	}

	// 静止画でもアニメーション画像でも1枚目しか見ない。
	const int idx = 0;

	if (diag_get_level(diag) >= 1) {
		diag_print(diag, "%s: frame_count=%u bgcolor=%d global_colormap=%s",
			__func__, gif->ImageCount, gif->SBackGroundColor,
			(gif->SColorMap ? "yes" : "no"));
		for (uint i = 0; i < gif->ImageCount; i++) {
			DGifSavedExtensionToGCB(gif, i, &gcb);
			desc = &gif->SavedImages[i].ImageDesc;
			diag_print(diag, "%c[%2u] (%u,%u)-(%ux%u) "
				"disposal=%s cmap=%s trans=%d delay=%u[msec]",
				(i == idx ? '*' : ' '), i,
				desc->Left, desc->Top, desc->Width, desc->Height,
				disposal2str(gcb.DisposalMode),
				(desc->ColorMap != NULL ? "yes" : "no"),
				gcb.TransparentColor,
				gcb.DelayTime * 10);
		}
	}

	// 透過色を取り出す。使用してなければ -1。
	DGifSavedExtensionToGCB(gif, idx, &gcb);
	transparent_color = gcb.TransparentColor;

	// カラーマップを取り出す。
	src = &gif->SavedImages[idx];
	desc = &src->ImageDesc;
	cmap = desc->ColorMap ?: gif->SColorMap;

	img = image_create(gif->SWidth, gif->SHeight,
		((transparent_color < 0) ? IMAGE_FMT_RGB24 : IMAGE_FMT_ARGB32));
	if (img == NULL) {
		warnx("%s: image_create failed: %s", __func__, strerrno());
		goto done;
	}

	// RasterBits[] に width x height のカラーコードが並んでいる。
	const GifByteType *s = src->RasterBits;
	uint8 *d = img->buf;
	if (transparent_color < 0) {
		for (uint y = 0; y < desc->Height; y++) {
			for (uint x = 0; x < desc->Width; x++) {
				uint cc = *s++;
				GifColorType rgb = cmap->Colors[cc];
				*d++ = rgb.Red;
				*d++ = rgb.Green;
				*d++ = rgb.Blue;
			}
		}
	} else {
		for (uint y = 0; y < desc->Height; y++) {
			for (uint x = 0; x < desc->Width; x++) {
				uint cc = *s++;
				GifColorType rgb = cmap->Colors[cc];
				*d++ = rgb.Red;
				*d++ = rgb.Green;
				*d++ = rgb.Blue;
				*d++ = (cc == transparent_color) ? 0 : 0xff;
			}
		}
	}

 done:
	DGifCloseFile(gif, &errcode);
	return img;
}

static int
gif_read(GifFileType *gf, GifByteType *dst, int length)
{
	FILE *fp = (FILE *)gf->UserData;

	size_t total = 0;
	while (total < length) {
		ssize_t r = fread((uint8 *)dst + total, 1, length - total, fp);
		if (r <= 0) {
			break;
		}
		total += r;
	}
	return total;
}

// Disposal Mode のデバッグ表示用。
static const char *
disposal2str(int mode)
{
	switch (mode) {
	 case DISPOSAL_UNSPECIFIED:
		return "unspecified";
	 case DISPOSE_DO_NOT:
		return "no_dispose";
	 case DISPOSE_BACKGROUND:
		return "background";
	 case DISPOSE_PREVIOUS:
		return "previous";
	 default:
		return "?";
	}
}
