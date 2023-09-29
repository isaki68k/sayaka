/*
 * Copyright (C) 2023 Tetsuya Isaki
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

#include "ImageLoaderWebp.h"
#include <array>
#include <cstring>
#include <webp/decode.h>

#define MAGIC_LEN	(64)
#define BUFSIZE		(4000)

#define TRANSBG		(0xe1)	// ?

// コンストラクタ
ImageLoaderWebp::ImageLoaderWebp(InputStream *stream_, const Diag& diag_)
	: inherited(stream_, diag_)
{
	static_assert(MAGIC_LEN >= 12);
}

// デストラクタ
ImageLoaderWebp::~ImageLoaderWebp()
{
}

// stream が webp なら true を返す。
bool
ImageLoaderWebp::Check() const
{
	std::array<uint8, MAGIC_LEN> magic;
	WebPBitstreamFeatures f;

	auto n = stream->Peek(magic.data(), magic.size());
	if (n < 0) {
		Trace(diag, "%s: Read(magic) failed: %s", __method__, strerror(errno));
		return false;
	}
	if (n < magic.size()) {
		Trace(diag, "%s: Read(magic) too short: %d", __method__, (int)n);
		return false;
	}

	// フォーマットは WebpGetFeatures() で判定できる。
	// データが足りなければ VP8_STATUS_NOT_ENOUGH_DATA が返ってくるが、
	// とりあえず長さは決め打ち。
	auto r = WebPGetFeatures(magic.data(), magic.size(), &f);
	if (r == VP8_STATUS_BITSTREAM_ERROR) {
		// Webp ではない。
		return false;
	}
	if (r != 0) {
		// それ以外のエラー。
		Trace(diag, "%s: WebPGetFeatures() failed: %d", __method__, (int)r);
		return false;
	}
	// Webp っぽい。
	Trace(diag, "%s: OK", __method__);
	return true;
}

// stream から画像をロードする。
bool
ImageLoaderWebp::Load(Image& img)
{
	std::array<uint8, MAGIC_LEN> magic;
	WebPDecoderConfig config;
	ssize_t n;
	bool rv = false;

	// まず Check() で読んだのと同じ分だけ Read() で読む。
	// 今の所 Stream は同じ場所を二度 Peek() できない。
	n = stream->Read(magic.data(), magic.size());
	if (n < 0) {
		Trace(diag, "%s: Read(magic) failed: %s", __method__, strerror(errno));
		return false;
	}
	if (n < magic.size()) {
		Trace(diag, "%s: Read(magic) too short: %d", __method__, (int)n);
		return false;
	}

	// ファイルサイズを取得。
	// +4バイト目から4バイトが 8バイト目以降のファイルサイズ(LE)。
	int filesize = (int)(magic[4]
				| (magic[5] << 8)
				| (magic[6] << 16)
				| (magic[7] << 24));
	filesize += 8;

	// Feature を取得。
	WebPInitDecoderConfig(&config);
	auto r = WebPGetFeatures(magic.data(), magic.size(), &config.input);
	if (r != 0) {
		// さっき成功してるのでエラーになるはずはない。
		Trace(diag, "%s: WebPGetFeatures failed", __method__);
		return false;
	}

	int width = config.input.width;
	int height = config.input.height;
	int format = config.input.format;

	static const char * const formatname[] = {
		"mixed(or undefined)",
		"lossy",
		"lossless",
	};
	Debug(diag, "%s: filesize=%d dim=(%d,%d)", __method__,
		filesize, width, height);
	Debug(diag, "%s: has_alpha=%d has_anime=%d format=%d/%s", __method__,
		config.input.has_alpha, config.input.has_animation,
		format, ((0 <= format && format <= 2) ? formatname[format] : "?"));

	// 出力画像サイズが決まったのでここで確保。
	img.Create(width, height);

	if (config.input.has_animation)
	{
		// アニメーションは処理が全然別。
		Debug(diag, "use frame");
		return rv;
	} else if (config.input.has_alpha) {
		// アルファチャンネルがあるとインクリメンタル処理できないっぽい?
		Debug(diag, "%s: use RGBA decoder", __method__);

		// ファイル全体を読み込む。
		std::vector<uint8> buf(filesize);
		memcpy(buf.data(), magic.data(), magic.size());
		n = stream->Read(buf.data() + magic.size(), filesize - magic.size());
		if (n < 0) {
			Trace(diag, "%s: Read() failed: %s", __method__, strerror(errno));
			return false;
		}

		// RGBA 出力バッファを用意。
		int stride = width * 4;
		int outbufsize = stride * height;
		uint8 *outbuf = (uint8 *)calloc(outbufsize, 1);
		if (outbuf == NULL) {
			Trace(diag, "%s: calloc(%d) failed: %s", __method__,
				outbufsize, strerror(errno));
			return false;
		}

		// RGBA で出力。
		config.output.colorspace = MODE_RGBA;
		config.output.is_external_memory = 1;
		config.output.u.RGBA.rgba = outbuf;
		config.output.u.RGBA.size = outbufsize;
		config.output.u.RGBA.stride = stride;
		int status = WebPDecode(buf.data(), buf.size(), &config);
		if (status != VP8_STATUS_OK) {
			Trace(diag, "%s: WebpDecode() failed", __method__);
			goto abort_alpha;
		}

		// RGB に変換。
		RGBAtoRGB(img.GetBuf(), outbuf, width, height, stride, TRANSBG);
		rv = true;
 abort_alpha:
		WebPFreeDecBuffer(&config.output);
		return rv;
	} else {
		// インクリメンタル処理が出来る。
		Debug(diag, "%s: use incremental RGB decoder", __method__);

		WebPIDecoder *idec = WebPINewDecoder(NULL);
		if (idec == NULL) {
			Trace(diag, "%s: WebPINewDecoder() failed", __method__);
			return false;
		}

		// 読み込み済みの部分だけ先に処理。必ず SUSPENDED になるはず。
		int status = WebPIAppend(idec, magic.data(), magic.size());
		if (status != VP8_STATUS_SUSPENDED) {
			Trace(diag, "%s: WebPIAppend(first) failed", __method__);
			goto abort_inc;
		}

		rv = LoadInc(img, idec);
 abort_inc:
		WebPIDelete(idec);
		return rv;
	}
}

#define Grad(fg, bg, alpha)	\
	(((fg) * (alpha) / 255) + ((bg) * (255 - (alpha)) / 255))

// Webp の RGBA を RGB に変換。
void
ImageLoaderWebp::RGBAtoRGB(uint8 *dst, const uint8 *src,
	int width, int height, int stride, int bgcolor)
{
	for (int y = 0; y < height; y++) {
		const uint8 *s = src + y * stride;
		for (int x = 0; x < width; x++) {
			int alpha = s[3];
			*dst++ = Grad(s[0], bgcolor, alpha);	// R
			*dst++ = Grad(s[1], bgcolor, alpha);	// G
			*dst++ = Grad(s[2], bgcolor, alpha);	// B
			s += 4;
		}
	}
}

// インクリメンタル処理で stream から Webp を読み込んで img に書き出す。
bool
ImageLoaderWebp::LoadInc(Image& img, WebPIDecoder *idec)
{
	std::vector<uint8> buf(BUFSIZE);
	int status;
	int width;
	int height;
	int stride;
	const uint8 *s;
	uint8 *d;

	status = VP8_STATUS_NOT_ENOUGH_DATA;
	for (;;) {
		auto n = stream->Read(buf.data(), buf.size());
		if (n < 0) {
			Trace(diag, "%s: Read(inc) failed: %s", __method__,
					strerror(errno));
			return false;
		}
		if (n == 0) {
			break;
		}
		status = WebPIAppend(idec, buf.data(), n);
		if (status != VP8_STATUS_SUSPENDED) {
			break;
		}
	}
	if (status != VP8_STATUS_OK) {
		Trace(diag, "%s: Decode failed %d", __method__, (int)status);
		return false;
	}

	// RGB バッファを取得。
	s = WebPIDecGetRGB(idec, NULL, &width, &height, &stride);
	if (s == NULL) {
		Trace(diag, "%s: WebPIDecGetRGB() failed", __method__);
		return false;
	}

	// そのままコピー出来る。
	d = img.GetBuf();
	for (int y = 0; y < height; y++) {
		memcpy(d, s, width * 3);
		s += stride;
		d += width * 3;
	}

	return true;
}
