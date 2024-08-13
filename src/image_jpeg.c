/* vi:set ts=4: */
/*
 * Copyright (C) 2021-2025 Tetsuya Isaki
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
// JPEG 読み込み
//

#include "common.h"
#include "image_priv.h"
#include <string.h>
#include <jpeglib.h>

bool
image_jpeg_match(FILE *fp, const struct diag *diag)
{
	uint8 magic[2];

	size_t n = fread(&magic[0], sizeof(magic), 1, fp);
	if (n == 0) {
		Debug(diag, "%s: fread failed: %s", __func__, strerrno());
		return false;
	}

	// マジックを確認。
	if (magic[0] != 0xff || magic[1] != 0xd8) {
		Debug(diag, "%s: Bad magic", __func__);
		return false;
	}

	return true;
}

struct image *
image_jpeg_read(FILE *fp, const struct diag *diag)
{
	struct jpeg_decompress_struct jinfo;
	struct jpeg_error_mgr jerr;
	struct image *img;
	uint8 *lineptr;
	uint width;
	uint height;
	uint stride;

	memset(&jinfo, 0, sizeof(jinfo));
	memset(&jerr, 0, sizeof(jerr));
	jinfo.err = jpeg_std_error(&jerr);

	jpeg_create_decompress(&jinfo);
	jpeg_stdio_src(&jinfo, fp);

	// ヘッダの読み込み。
	jpeg_read_header(&jinfo, (boolean)true);
	width  = jinfo.image_width;
	height = jinfo.image_height;

	img = image_create(width, height, IMAGE_FMT_RGB24);
	stride = image_get_stride(img);

	// データの読み込み。
	jpeg_start_decompress(&jinfo);
	lineptr = img->buf;
	for (uint y = 0; y < height; y++) {
		jpeg_read_scanlines(&jinfo, &lineptr, 1);
		lineptr += stride;
	}

	jpeg_finish_decompress(&jinfo);
	jpeg_destroy_decompress(&jinfo);

	return img;
}
