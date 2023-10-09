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
#include "PeekableStream.h"
#include "subr.h"
#include <cstring>
#include <webp/decode.h>

// <webp/demux.h> has a cast warning...
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

#define MAGIC_LEN	(64)
#define BUFSIZE		(4000)

#define TRANSBG		(0xe1)	// ?

// コンストラクタ
ImageLoaderWebp::ImageLoaderWebp(PeekableStream *stream_, const Diag& diag_)
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
	std::vector<uint8> magic(MAGIC_LEN);
	WebPBitstreamFeatures f;

	auto n = stream->Peek(magic.data(), magic.size());
	if (n < 0) {
		Trace(diag, "%s: Read(magic) failed: %s", __method__, strerrno());
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
	std::vector<uint8> magic(MAGIC_LEN);
	WebPDecoderConfig config;
	ssize_t n;
	bool rv = false;

	// Features を取得できる分だけ読み込む。
	n = stream->Read(magic.data(), magic.size());
	if (n < 0) {
		Trace(diag, "%s: Read(magic) failed: %s", __method__, strerrno());
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

	WebPInitDecoderConfig(&config);
	config.options.no_fancy_upsampling = 1;

	// Feature を取得。
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
		// アニメーションは処理が全然別。要 -lwebpdemux。
		Debug(diag, "%s: Use frame decoder", __method__);

		WebPAnimDecoderOptions opt;
		WebPData data;
		WebPAnimDecoder *dec;
		uint8 *outbuf;
		int stride;
		int timestamp;

		// ファイル全体を読み込む。
		std::vector<uint8> buf(filesize);
		n = ReadAll(buf, magic);
		if (n < 0) {
			return false;
		}

		WebPAnimDecoderOptionsInit(&opt);
		opt.color_mode = MODE_RGBA;
		data.bytes = buf.data();
		data.size = filesize;
		dec = WebPAnimDecoderNew(&data, &opt);
		if (dec == NULL) {
			Trace(diag, "%s: WebpAnimDecoderNew() failed", __method__);
			return false;
		}

		// 次のフレームがある間ループで回るやつだがここでは最初の1枚だけ。
		if (WebPAnimDecoderHasMoreFrames(dec) == false) {
			Trace(diag, "%s: No frames?", __method__);
			goto abort_anime;
		}

		// このフレームをデコード。outbuf にセットされて返ってくるらしい。
		if (WebPAnimDecoderGetNext(dec, &outbuf, &timestamp) == false) {
			Trace(diag, "%s: WebpAnimDecoderGetNext() failed", __method__);
			goto abort_anime;
		}

		// RGB に変換。
		stride = width * 4;
		RGBAtoRGB(img.GetBuf(), outbuf, width, height, stride, TRANSBG);
		rv = true;

 abort_anime:
		WebPAnimDecoderDelete(dec);
		return rv;
	} else if (config.input.has_alpha) {
		// アルファチャンネルがあるとインクリメンタル処理できないっぽい?
		Debug(diag, "%s: use RGBA decoder", __method__);

		// ファイル全体を読み込む。
		std::vector<uint8> buf(filesize);
		n = ReadAll(buf, magic);
		if (n < 0) {
			return false;
		}

		// RGBA 出力バッファを用意。
		int stride = width * 4;
		int outbufsize = stride * height;
		std::vector<uint8> outbuf(outbufsize);

		// RGBA で出力。
		config.output.colorspace = MODE_RGBA;
		config.output.u.RGBA.rgba = outbuf.data();
		config.output.u.RGBA.size = outbuf.size();
		config.output.u.RGBA.stride = stride;
		int status = WebPDecode(buf.data(), buf.size(), &config);
		if (status != VP8_STATUS_OK) {
			Trace(diag, "%s: WebpDecode() failed", __method__);
			goto abort_alpha;
		}

		// RGB に変換。
		RGBAtoRGB(img.GetBuf(), outbuf.data(), width, height, stride, TRANSBG);
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

// buf (filesize だけ確保してある) に stream から読み込む。
// magic はすでに読んである buf の先頭部分。
// 成功すれば magic も含めて読み込めたというか buf に書き込んだバイト数を返す。
// 失敗すれば errno をセットして -1 を返す。
ssize_t
ImageLoaderWebp::ReadAll(std::vector<uint8>& buf,
	const std::vector<uint8>& magic)
{
	size_t filesize = buf.size();
	size_t len;

	// すでに読み込んでる部分。
	memcpy(buf.data(), magic.data(), magic.size());
	len = magic.size();

	while (len < filesize) {
		auto n = stream->Read(buf.data() + len, buf.size() - len);
		if (__predict_false(n < 0)) {
			Trace(diag, "%s: Read() failed: %s", __method__, strerrno());
			return -1;
		}
		if (__predict_false(n == 0)) {
			Trace(diag, "%s: Read(): Unexpected EOF", __method__);
			break;
		}

		len += n;
	}

	if (len < filesize) {
		Debug(diag, "%s: too short: %zd < %zd", __method__, len, filesize);
	}

	return len;
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
	int stride;
	const uint8 *s;
	uint8 *d;

	status = VP8_STATUS_NOT_ENOUGH_DATA;
	for (;;) {
		auto n = stream->Read(buf.data(), buf.size());
		if (n < 0) {
			Trace(diag, "%s: Read(inc) failed: %s", __method__, strerrno());
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
	s = WebPIDecGetRGB(idec, NULL, NULL, NULL, &stride);
	if (s == NULL) {
		Trace(diag, "%s: WebPIDecGetRGB() failed", __method__);
		return false;
	}

	// そのままコピー出来る。
	int height = img.GetHeight();
	int width = img.GetWidth();
	d = img.GetBuf();
	for (int y = 0; y < height; y++) {
		memcpy(d, s, width * 3);
		s += stride;
		d += width * 3;
	}

	return true;
}
