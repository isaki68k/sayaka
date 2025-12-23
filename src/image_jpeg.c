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

#define JPEG_APP(n)	(JPEG_APP0 + (n))

struct my_jpeg_error_mgr {
	struct jpeg_error_mgr mgr;
	jmp_buf jmp;
};

static void print_marker(jpeg_saved_marker_ptr, const char *,
	const struct diag *);
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
	uint color_space;
	int scale;

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

	if (__predict_false(diag_get_level(diag) >= 1)) {
		// マーカーを調査。デバッグ表示用。
		for (uint i = 2; i < 16; i++) {
			jpeg_save_markers(UNVOLATILE(&jinfo), JPEG_APP(i), 0xffff);
		}
	}

	// ヘッダの読み込み。
	jpeg_read_header(UNVOLATILE(&jinfo), (boolean)true);
	width  = jinfo.image_width;
	height = jinfo.image_height;
	color_space =jinfo.jpeg_color_space;
	Debug(diag, "%s: color_space=%s num_components=%u", __func__,
		colorspace2str(color_space), jinfo.num_components);

	if (__predict_false(diag_get_level(diag) >= 1)) {
		// 一部のマーカーを表示。
		print_marker(jinfo.marker_list, __func__, diag);
	}

	// 出力形式を選択。
	switch (color_space) {
	 case JCS_GRAYSCALE:
	 case JCS_RGB:
	 case JCS_YCbCr:
		// RGB として取り出せる。
		jinfo.out_color_space = JCS_RGB;
		break;

	 case JCS_CMYK:
	 case JCS_YCCK:
		// 一旦 CMYK として取り出す。
		jinfo.out_color_space=JCS_CMYK;
		break;

	 default:
		break;
	}

	// 必要なら縮小スケールを計算。
	scale = -1;
	if (hint->width != 0 || hint->height != 0) {
		uint pref_width;
		uint pref_height;
		image_get_preferred_size(width, height,
			hint->axis, hint->width, hint->height,
			&pref_width, &pref_height);

		// 有効なスケールは 1, 2, 4, 8 らしい。
		for (scale = 3; scale > 0; scale--) {
			if (pref_width  <= (width  >> scale)
			 && pref_height <= (height >> scale)) {
				break;
			}
		}

		// スケールを指定。
		jinfo.scale_num = 1;
		jinfo.scale_denom = 1U << scale;
	}

	jpeg_start_decompress(UNVOLATILE(&jinfo));
	if (jinfo.out_color_space != color_space) {
		Debug(diag, "%s: filtered color_space=%s", __func__,
			colorspace2str(jinfo.out_color_space));
	}
	if (scale >= 0) {
		Debug(diag, "%s: OrigSize=(%u, %u) scale=1/%u", __func__,
			width, height, 1U << scale);
	}

	// 端数対応のため、向こうが計算した幅と高さを再取得。
	width  = jinfo.output_width;
	height = jinfo.output_height;

	img = image_create(width, height, IMAGE_FMT_RGB24);
	if (img == NULL) {
		warn("%s: image_create failed", __func__);
		goto done;
	}
	stride = image_get_stride(UNVOLATILE(img));

	// データの読み込み。
	lineptr = img->buf;
	switch (jinfo.out_color_space) {
	 default:	// とりあえず適当なところへ落ちておく。
	 case JCS_RGB:
		// 直接 RGB で読み出せる。
		for (uint y = 0; y < height; y++) {
			jpeg_read_scanlines(UNVOLATILE(&jinfo), &lineptr, 1);
			lineptr += stride;
		}
		break;

	 case JCS_CMYK:
	 {
		// 一旦 CMYK で取り出して変換する。
		uint8 cmykbuf[width * 4];
		for (uint y = 0; y < height; y++) {
			uint8 *bufp = cmykbuf;
			jpeg_read_scanlines(UNVOLATILE(&jinfo), &bufp, 1);
			// CMYK -> RGB 変換。本来(?)の式は
			//  R = (255 - C) * (255 - K) / 255
			//  G = (255 - M) * (255 - K) / 255
			//  B = (255 - Y) * (255 - K) / 255
			// だが libjpeg の返す CMYK は反転済みらしい…。issue#53
			for (uint x = 0; x < width; x++) {
				uint C = *bufp++;
				uint M = *bufp++;
				uint Y = *bufp++;
				uint K = *bufp++;
				*lineptr++ = (C * K + 127) / 255;
				*lineptr++ = (M * K + 127) / 255;
				*lineptr++ = (Y * K + 127) / 255;
			}
		}
		break;
	 }
	}

	jpeg_finish_decompress(UNVOLATILE(&jinfo));
 done:
	jpeg_destroy_decompress(UNVOLATILE(&jinfo));
	return UNVOLATILE(img);
}

// 一部のマーカーが存在していることだけ表示。
static void
print_marker(jpeg_saved_marker_ptr marker_list,
	const char *fn, const struct diag *diag)
{
	jpeg_saved_marker_ptr m;
	bool has_icc_profile = false;
	bool has_adobe = false;

	for (m = marker_list; m; m = m->next) {
		switch (m->marker) {
		 case JPEG_APP(2):
			if (m->data_length > 11 && memcmp(m->data, "ICC_PROFILE", 11) == 0) {
				has_icc_profile = true;
			}
			break;
		 case JPEG_APP(14):
			if (m->data_length > 5 && memcmp(m->data, "Adobe", 5) == 0) {
				has_adobe = true;
			}
			break;
		 default:
			break;
		}
	}

	if (has_icc_profile) {
		diag_print(diag, "%s: ICC Profile found (Not supported)", fn);
	}
	if (has_adobe) {
		diag_print(diag, "%s: APP14\"Adobe\" found (Not supported)", fn);
	}
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
