/* vi:set ts=4: */
/*
 * Copyright (C) 2023-2024 Tetsuya Isaki
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
// WebP 読み込み
//

#include "common.h"
#include "image.h"
#include "image_proto.h"
#include <string.h>
#include <webp/decode.h>

#define INCBUFSIZE	(4000)

static bool image_webp_loadinc(struct image *, FILE *, WebPIDecoder *,
	const struct diag *diag);

bool
image_webp_match(FILE *fp, const struct diag *diag)
{
	VP8StatusCode r = VP8_STATUS_BITSTREAM_ERROR;
	uint8 *buf = NULL;
	size_t bufsize = 0;
	size_t len = 0;

	do {
		size_t newsize = len + 64;
		uint8 *newbuf = realloc(buf, newsize);
		if (newbuf == NULL) {
			Debug(diag, "%s: realloc failed: %s", __func__, strerrno());
			break;
		}
		buf = newbuf;
		bufsize = newsize;

		size_t n = fread(buf + len, 1, bufsize - len, fp);
		if (n == 0) {
			break;
		}
		len += n;

		// フォーマットは WebPGetFeatures() で判定できる。
		// データが足りなければ VP8_STATUS_NOT_ENOUGH_DATA が返ってくる。
		WebPBitstreamFeatures features;
		r = WebPGetFeatures(buf, len, &features);
	} while (r == VP8_STATUS_NOT_ENOUGH_DATA);

	free(buf);

	if (r == VP8_STATUS_BITSTREAM_ERROR) {
		// Webp ではない。
		return false;
	} else if (r == 0) {
		// Webp っぽい。
		Debug(diag, "%s: OK", __func__);
		return true;
	} else {
		// それ以外のエラー。
		Debug(diag, "%s: WebPGetFeatures() failed: %d", __func__, (int)r);
		return false;
	}
}

struct image *
image_webp_read(FILE *fp, const struct diag *diag)
{
	uint8 *filebuf = NULL;
	size_t filecap = 0;
	size_t filelen = 0;
	struct image *img = NULL;
	WebPDecoderConfig config;
	VP8StatusCode r;
	size_t n;
	bool success;

	success = false;

	WebPInitDecoderConfig(&config);
	config.options.no_fancy_upsampling = 1;

	// まず Features を取得できる分だけ読み込む。
	r = VP8_STATUS_BITSTREAM_ERROR;
	do {
		size_t newcap = filecap + 64;
		uint8 *newbuf = realloc(filebuf, newcap);
		if (newbuf == NULL) {
			Debug(diag, "%s: realloc failed: %s", __func__, strerrno());
			break;
		}
		filebuf = newbuf;
		filecap = newcap;

		n = fread(filebuf + filelen, 1, filecap - filelen, fp);
		if (n == 0) {
			break;
		}
		filelen += n;

		// Feature を取得。
		r = WebPGetFeatures(filebuf, filelen, &config.input);
	} while (r == VP8_STATUS_NOT_ENOUGH_DATA);

	if (r == VP8_STATUS_BITSTREAM_ERROR) {
		// Webp ではない。
		goto abort;
	} else if (r != 0) {
		// それ以外のエラー。
		Debug(diag, "%s: WebPGetFeatures() failed: %d", __func__, (int)r);
		goto abort;
	}

	// ファイルサイズを取得。
	// +4バイト目から4バイトが 8バイト目以降のファイルサイズ(LE)。
	uint filesize = (uint)(filebuf[4]
				| (filebuf[5] << 8)
				| (filebuf[6] << 16)
				| (filebuf[7] << 24));
	filesize += 8;

	uint width = config.input.width;
	uint height = config.input.height;
	uint format = config.input.format;

	if (diag_get_level(diag) >= 1) {
		diag_print(diag, "%s: filesize=%u dim=(%u,%u)", __func__,
			filesize, width, height);

		const char *formatstr;
		char formatbuf[16];
		switch (format) {
		 case 0:	formatstr = "mixed(or undefined)";	break;
		 case 1:	formatstr = "lossy";				break;
		 case 2:	formatstr = "lossless";				break;
		 default:
			snprintf(formatbuf, sizeof(formatbuf), "0x%x", format);
			formatstr = formatbuf;
			break;
		}
		diag_print(diag, "%s: has_alpha=%u has_anime=%u format=%s", __func__,
			config.input.has_alpha,
			config.input.has_animation,
			formatstr);
	}

	// 出力画像サイズが決まったのでここで確保。
	img = image_create(width, height, 3);

	if (config.input.has_animation) {
		// アニメーションは処理が全然別。要 -lwebpdemux。
		Debug(diag, "%s: Use frame decoder", __func__);

	} else if (config.input.has_alpha) {
		// アルファチャンネルがあるとインクリメンタル処理できないっぽい?
		Debug(diag, "%s: use RGBA decoder", __func__);

	} else {
		// インクリメンタル処理が出来る。
		Debug(diag, "%s: use incremental RGB decoder", __func__);

		WebPIDecoder *idec = WebPINewDecoder(NULL);
		if (idec == NULL) {
			Debug(diag, "%s: WebPINewDecoder() failed", __func__);
			goto abort_inc;
		}

		// 読み込み済みの部分だけ先に処理。必ず SUSPENDED になるはず。
		int status = WebPIAppend(idec, filebuf, filelen);
		if (status != VP8_STATUS_SUSPENDED) {
			Debug(diag, "%s: WebPIAppend(first) failed", __func__);
			goto abort_inc;
		}

		success = image_webp_loadinc(img, fp, idec, diag);
 abort_inc:
		if (idec) {
			WebPIDelete(idec);
		}
	}

 abort:
	free(filebuf);
	if (success == false) {
		image_free(img);
		img = NULL;
	}
	return img;
}

// インクリメンタル処理が出来る場合。
static bool
image_webp_loadinc(struct image *img, FILE *fp, WebPIDecoder *idec,
	const struct diag *diag)
{
	uint8 buf[INCBUFSIZE];
	int status;
	int srcstride;
	const uint8 *s;
	uint8 *d;

	status = VP8_STATUS_SUSPENDED;
	do {
		size_t n = fread(buf, 1, sizeof(buf), fp);
		if (n == 0) {
			break;
		}
		status = WebPIAppend(idec, buf, n);
	} while (status == VP8_STATUS_SUSPENDED);

	if (status != VP8_STATUS_OK) {
		Debug(diag, "%s: Decode failed %d", __func__, (int)status);
		return false;
	}

	// RGB バッファを取得。
	s = WebPIDecGetRGB(idec, NULL, NULL, NULL, &srcstride);
	if (s == NULL) {
		Debug(diag, "%s: WebPIDecGetRGB() failed", __func__);
		return false;
	}

	// そのままコピー出来る。
	// image はパディングがないので1ラスター分はストライドでいい。
	uint dststride = image_get_stride(img);
	uint height = img->height;
	d = img->buf;
	for (uint y = 0; y < height; y++) {
		memcpy(d, s, dststride);
		s += srcstride;
		d += dststride;
	}

	return true;
}