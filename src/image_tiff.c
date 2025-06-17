/* vi:set ts=4: */
/*
 * Copyright (C) 2025 Tetsuya Isaki
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
// TIFF 読み込み
//

// XXX uint16 とかが使えず uint16_t とかにする必要があるようだ…

#include "common.h"
#include "image_priv.h"
#include <tiffio.h>

static tmsize_t tiff_read(thandle_t, void *, tmsize_t);
static tmsize_t tiff_write(thandle_t, void *, tmsize_t);
static toff_t   tiff_seek(thandle_t, toff_t, int);
static int      tiff_close(thandle_t);
static toff_t   tiff_size(thandle_t);
static void     tiff_null_error_handler(const char *, const char *, va_list);
static const char *photometric2str(uint16_t);

bool
image_tiff_match(FILE *fp, const struct diag *diag)
{
	TIFF *tiff;
	TIFFErrorHandler orig_handler;
	bool matched = false;

	// エラーハンドラを設定。
	// TIFFClientOpen() は TIFF じゃないファイルだと標準エラー出力に
	// メッセージを出してしまうので、それを抑制する。
	orig_handler = TIFFSetErrorHandler(tiff_null_error_handler);

	tiff = TIFFClientOpen("<input>", "rh", fp,
		tiff_read,
		tiff_write,
		tiff_seek,
		tiff_close,
		tiff_size,
		NULL, NULL);
	if (tiff) {
		matched = true;
		TIFFClose(tiff);
	}

	TIFFSetErrorHandler(orig_handler);
	return matched;
}

struct image *
image_tiff_read(FILE *fp, const image_read_hint *dummy, const struct diag *diag)
{
	TIFF *tiff;
	struct image *img;
	uint32_t width;
	uint32_t height;
	uint16_t bits_per_sample;
	uint16_t samples_per_pixel;
	uint16_t photo_metric;

	tiff = TIFFClientOpen("<input>", "r", fp,
		tiff_read,
		tiff_write,
		tiff_seek,
		tiff_close,
		tiff_size,
		NULL, NULL);
	if (__predict_false(tiff == NULL)) {
		return NULL;
	}

	TIFFGetField(tiff, TIFFTAG_IMAGEWIDTH, &width);
	TIFFGetField(tiff, TIFFTAG_IMAGELENGTH, &height);
	TIFFGetField(tiff, TIFFTAG_BITSPERSAMPLE, &bits_per_sample);
	TIFFGetField(tiff, TIFFTAG_SAMPLESPERPIXEL, &samples_per_pixel);
	TIFFGetField(tiff, TIFFTAG_PHOTOMETRIC, &photo_metric);
	Debug(diag, "%s: PhotoMetric=%s BitsPerSample=%u SamplesPerPixel=%u",
		__func__, photometric2str(photo_metric),
		bits_per_sample, samples_per_pixel);

	// 雑。
	uint fmt;
	if (samples_per_pixel == 4) {
		fmt = IMAGE_FMT_ARGB32;
	} else {
		fmt = IMAGE_FMT_RGB24;
	}

	img = image_create(width, height, fmt);
	if (img == NULL) {
		goto done;
	}

	uint8_t *d = img->buf;
	uint32_t stride = image_get_stride(img);
	for (uint y = 0; y < height; y++) {
		TIFFReadScanline(tiff, d, y, samples_per_pixel);
		d += stride;
	}

 done:
	TIFFClose(tiff);
	return img;
}

// コールバック

static tmsize_t
tiff_read(thandle_t arg, void *buf, tmsize_t len)
{
	FILE *fp = (FILE *)arg;

	return fread(buf, 1, len, fp);
}

static tmsize_t
tiff_write(thandle_t arg, void *buf, tmsize_t len)
{
	// 使わないが、用意しておかないといけない。
	return -1;
}

// 成功なら現在位置を返す。
static toff_t
tiff_seek(thandle_t arg, toff_t pos, int whence)
{
	FILE *fp = (FILE *)arg;

	if (fseek(fp, pos, whence) < 0) {
		return -1;
	}
	return ftell(fp);
}

static toff_t
tiff_size(thandle_t arg)
{
	// 下のレイヤが seekable ならファイルサイズを求めることは出来るが、
	// とりあえず 0 を返しておいても動くようだ。
	return 0;
}

static int
tiff_close(thandle_t arg)
{
	// ここでは何もしない。
	return 0;
}

// 何もしないエラーハンドラ。
static void
tiff_null_error_handler(const char *module, const char * fmt, va_list ap)
{
}

// TIFF の PhotoMetric のデバッグ表示用。
static const char *
photometric2str(uint16_t val)
{
	static const char * const names[] = {
		"WhiteIsZero",
		"BlackIsZero",
		"RGB",
		"Palette",
		"TransparencyMask",
	};

	if (val >= countof(names)) {
		static char buf[16];
		snprintf(buf, sizeof(buf), "0x%x(?)", val);
	}
	return names[val];
}
