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
#include <err.h>
#include <setjmp.h>
#include <string.h>
#include <jpeglib.h>

struct my_jpeg_error_mgr {
	struct jpeg_error_mgr mgr;
	jmp_buf jmp;
};

static void my_error_exit(j_common_ptr);
static const char *colorspace2str(J_COLOR_SPACE);

static char my_msgbuf[JMSG_LENGTH_MAX];

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
image_jpeg_read(FILE *fp, const image_read_hint *hint, const struct diag *diag)
{
	volatile struct jpeg_decompress_struct jinfo;
	struct my_jpeg_error_mgr jerr;
	volatile struct image *img;
	uint8 *lineptr;
	uint width;
	uint height;
	uint stride;

	memset(UNVOLATILE(&jinfo), 0, sizeof(jinfo));
	memset(&jerr, 0, sizeof(jerr));
	img = NULL;

	jinfo.err = jpeg_std_error(&jerr.mgr);
	jerr.mgr.error_exit = my_error_exit;

	// libjpeg 内でエラーが起きたら大域ジャンプで戻ってくる…。
	if (setjmp(jerr.jmp)) {
		warnx("libjpeg: %s", my_msgbuf);
		free(UNVOLATILE(img));
		img = NULL;
		goto done;
	}

	jpeg_create_decompress(UNVOLATILE(&jinfo));
	jpeg_stdio_src(UNVOLATILE(&jinfo), fp);

	// ヘッダの読み込み。
	jpeg_read_header(UNVOLATILE(&jinfo), (boolean)true);
	width  = jinfo.image_width;
	height = jinfo.image_height;
	Debug(diag, "%s: color_space=%s num_components=%u", __func__,
		colorspace2str(jinfo.jpeg_color_space), jinfo.num_components);

	// 必要なら縮小スケールを計算。
	if (hint->width != 0 || hint->height != 0) {
		uint pref_width;
		uint pref_height;
		image_get_preferred_size(width, height,
			hint->axis, hint->width, hint->height,
			&pref_width, &pref_height);

		// 有効なスケールは 1, 2, 4, 8 らしい。
		uint scale;
		for (scale = 3; scale > 0; scale--) {
			if (pref_width  <= (width  >> scale)
			 && pref_height <= (height >> scale)) {
				break;
			}
		}

		Debug(diag, "OrigSize=(%u, %u) scale=1/%u", width, height, 1U << scale);

		// スケールを指定。
		jinfo.scale_num = 1;
		jinfo.scale_denom = 1U << scale;
	}

	// 出力を RGB に。
	jinfo.out_color_space = JCS_RGB;

	jpeg_start_decompress(UNVOLATILE(&jinfo));
	// 端数対応のため、向こうが計算した幅と高さを再取得。
	width  = jinfo.output_width;
	height = jinfo.output_height;

	img = image_create(width, height, IMAGE_FMT_RGB24);
	stride = image_get_stride(UNVOLATILE(img));

	// データの読み込み。
	lineptr = img->buf;
	for (uint y = 0; y < height; y++) {
		jpeg_read_scanlines(UNVOLATILE(&jinfo), &lineptr, 1);
		lineptr += stride;
	}

	jpeg_finish_decompress(UNVOLATILE(&jinfo));
 done:
	jpeg_destroy_decompress(UNVOLATILE(&jinfo));
	return UNVOLATILE(img);
}

static void
my_error_exit(j_common_ptr jinfo)
{
	struct my_jpeg_error_mgr *err = (struct my_jpeg_error_mgr *)jinfo->err;
	(*jinfo->err->format_message)(jinfo, my_msgbuf);
	longjmp(err->jmp, 1);
}

// JPEG の color_space のデバッグ表示用。
static const char *
colorspace2str(J_COLOR_SPACE c)
{
	static const char * const names[] = {
		"Unknown",
		"Grayscale",
		"RGB",
		"YCbCr",
		"CMYK",
		"YCCK",
		"BG_RGB",
		"BG_YCC",
	};

	if (c >= countof(names)) {
		static char buf[16];
		snprintf(buf, sizeof(buf), "%d(?)", c);
		return buf;
	}
	return names[c];
}
