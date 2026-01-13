/* vi:set ts=4: */
/*
 * Copyright (C) 2023-2025 Tetsuya Isaki
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
#include "image_priv.h"
#include <err.h>
#include <string.h>
#include <webp/decode.h>

// <webp/demux.h> has cast warnings...
#if defined(__clang__)
_Pragma("clang diagnostic push")
_Pragma("clang diagnostic ignored \"-Wcast-qual\"")
#else
_Pragma("GCC diagnostic push")
_Pragma("GCC diagnostic ignored \"-Wcast-qual\"")
#endif
#include <webp/demux.h>
#if defined(__clang__)
_Pragma("clang diagnostic pop")
#else
_Pragma("GCC diagnostic pop")
#endif

static bool read_all(uint8 **, size_t *, FILE *, uint32, const struct diag *);
static bool image_webp_loadinc(struct image *, FILE *, WebPIDecoder *,
	const struct diag *);

bool
image_webp_match(FILE *fp, const struct diag *diag)
{
	uint32 buf[3];

	uint len = fread(buf, sizeof(buf), 1, fp);
	if (__predict_false(len == 0)) {
		Debug(diag, "%s: fread failed: %s", __func__, strerrno());
		return false;
	}
	if (be32toh(buf[0]) != (('R' << 24) | ('I' << 16) | ('F' << 8) | 'F')) {
		return false;
	}
	if (be32toh(buf[2]) != (('W' << 24) | ('E' << 16) | ('B' << 8) | 'P')) {
		return false;
	}
	return true;
}

struct image *
image_webp_read(FILE *fp, const image_read_hint *hint, const struct diag *diag)
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
		Debug(diag, "%s: Bitstream error.", __func__);
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

	if (config.input.has_animation) {
		// アニメーションは処理が全然別。要 -lwebpdemux。
		Debug(diag, "%s: Use frame decoder", __func__);

		WebPDemuxer *demux = NULL;
		WebPAnimDecoder *dec = NULL;
		WebPAnimDecoderOptions opt;
		WebPData data;
		uint8 *outbuf;
		int timestamp;

		img = image_create(width, height, IMAGE_FMT_ARGB32);

		// ファイル全体を読み込む。
		if (read_all(&filebuf, &filelen, fp, filesize, diag) == false) {
			goto abort_anime;
		}

		WebPAnimDecoderOptionsInit(&opt);
		opt.color_mode = MODE_RGBA;
		data.bytes = filebuf;
		data.size = filelen;

		// ページ数(フレーム数)を取得。
		demux = WebPDemux(&data);
		if (demux == NULL) {
			Debug(diag, "%s: WebPDemux() failed", __func__);
			goto abort_anime;
		}
		uint total_pages = WebPDemuxGetI(demux, WEBP_FF_FRAME_COUNT);
		Debug(diag, "%s: frame_count=%u", __func__, total_pages);
		if (hint->page >= total_pages) {
			// 戻っても仕方ないので終了する?
			errx(1, "%s: No page found: %u", __func__, hint->page);
		}

		dec = WebPAnimDecoderNew(&data, &opt);
		if (dec == NULL) {
			Debug(diag, "%s: WebpAnimDecoderNew() failed", __func__);
			goto abort_anime;
		}

		for (uint page = 0; page < total_pages ; page++) {
			// このフレームをデコード。outbuf にセットされて返ってくるらしい。
			if (WebPAnimDecoderGetNext(dec, &outbuf, &timestamp) == false) {
				Debug(diag, "%s: WebpAnimDecoderGetNext() failed", __func__);
				goto abort_anime;
			}

			if (page == hint->page) {
				break;
			}
		}

		memcpy(img->buf, outbuf, image_get_stride(img) * height);
		success = true;

 abort_anime:
		if (demux) {
			WebPDemuxDelete(demux);
		}
		if (dec) {
			WebPAnimDecoderDelete(dec);
		}

	} else if (config.input.has_alpha) {
		// アルファチャンネルがあるとインクリメンタル処理できないっぽい?
		Debug(diag, "%s: use RGBA decoder", __func__);

		img = image_create(width, height, IMAGE_FMT_ARGB32);

		// ファイル全体を読み込む。
		if (read_all(&filebuf, &filelen, fp, filesize, diag) == false) {
			goto abort;
		}

		// RGBA 出力バッファを用意。
		uint outstride = width * 4;
		uint outbufsize = outstride * height;

		// RGBA で出力。
		config.output.colorspace = MODE_RGBA;
		config.output.u.RGBA.size = outbufsize;
		config.output.u.RGBA.stride = outstride;
		int status = WebPDecode(filebuf, filelen, &config);
		if (status != VP8_STATUS_OK) {
			Debug(diag, "%s: WebpDecode() failed", __func__);
			goto abort_alpha;
		}

		memcpy(img->buf, config.output.u.RGBA.rgba, outbufsize);
		success = true;
 abort_alpha:
		WebPFreeDecBuffer(&config.output);

	} else {
		// インクリメンタル処理が出来る。
		Debug(diag, "%s: use incremental RGB decoder", __func__);

		img = image_create(width, height, IMAGE_FMT_RGB24);

		WebPIDecoder *idec = WebPINewDecoder(NULL);
		if (idec == NULL) {
			Debug(diag, "%s: WebPINewDecoder() failed", __func__);
			goto abort_inc;
		}

		// 読み込み済みの部分だけ先に処理。
		// 全域読み終えていたら 0、そうでなければ SUSPENDED になるはず。
		int status = WebPIAppend(idec, filebuf, filelen);
		if (status != 0 && status != VP8_STATUS_SUSPENDED) {
			Debug(diag, "%s: WebPIAppend(first) failed: %d", __func__, status);
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

// *buf から始まる長さ *buflen (長さは 0 ではないかも知れない) のバッファを
// newsize にリサイズし、そこに fp から読み込んで追加する。
// 成功すれば、buf と buflen を更新し true を返す。
static bool
read_all(uint8 **bufp, size_t *buflenp, FILE *fp, uint32 newsize,
	const struct diag *diag)
{
	uint8 *buf = *bufp;
	size_t len = *buflenp;
	size_t pos = len;

	uint8 *newbuf = realloc(buf, newsize);
	if (newbuf == NULL) {
		return false;
	}
	buf = newbuf;
	len = newsize;
	// 更新出来たこの時点でもう書き戻しておくほうがいい。
	*bufp = buf;
	*buflenp = len;

	while (pos < newsize) {
		size_t n = fread(buf + pos, 1, len - pos, fp);
		if (n == 0) {
			Debug(diag, "%s: fread: Unexpected EOF", __func__);
			return false;
		}
		pos += n;
	}

	return true;
}

// インクリメンタル処理が出来る場合。
static bool
image_webp_loadinc(struct image *img, FILE *fp, WebPIDecoder *idec,
	const struct diag *diag)
{
	uint8 *buf;
	int status;
	int srcstride;
	const uint8 *s;
	uint8 *d;
	bool rv;

	rv = false;

	const size_t bufsize = IMAGE_BUFSIZE;
	buf = malloc(bufsize);
	if (buf == NULL) {
		Debug(diag, "%s: malloc(%zu) failed: %s", __func__,
			bufsize, strerrno());
		return false;
	}

	status = VP8_STATUS_NOT_ENOUGH_DATA;
	do {
		size_t n = fread(buf, 1, bufsize, fp);
		if (n == 0) {
			break;
		}
		status = WebPIAppend(idec, buf, n);
	} while (status == VP8_STATUS_SUSPENDED);

	if (status != VP8_STATUS_OK) {
		Debug(diag, "%s: Decode failed %d", __func__, status);
		goto done;
	}

	// RGB バッファを取得。
	s = WebPIDecGetRGB(idec, NULL, NULL, NULL, &srcstride);
	if (s == NULL) {
		Debug(diag, "%s: WebPIDecGetRGB() failed", __func__);
		goto done;
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

	rv = true;
 done:
	free(buf);
	return rv;
}
