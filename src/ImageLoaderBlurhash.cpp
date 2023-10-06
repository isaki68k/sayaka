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

#include "ImageLoaderBlurhash.h"
#include "Blurhash.h"
#include "subr.h"
#include <string.h>

// コンストラクタ
ImageLoaderBlurhash::ImageLoaderBlurhash(PeekableStream *stream_,
		const Diag& diag_)
	: inherited(stream_, diag_)
{
}

// デストラクタ
ImageLoaderBlurhash::~ImageLoaderBlurhash()
{
}

// stream が Blurhash そうなら true を返す。
bool
ImageLoaderBlurhash::Check() const
{
	std::string src {};

	// この時点で一旦全部 Peek で読み込む。
	// const 関数じゃなければよかった…。
	for (;;) {
		std::vector<char> loadbuf(256);
		auto n = stream->Peek(loadbuf.data(), loadbuf.size());
		if (n < 0) {
			Trace(diag, "%s: Peek failed: %s", __method__, strerrno());
			return false;
		}
		if (n == 0) {
			break;
		}
		src.append(loadbuf.data(), n);
	}

	Blurhash bh(src);
	return bh.IsValid();
}

// stream から画像をロードする。
bool
ImageLoaderBlurhash::Load(Image& img)
{
	std::string src;

	// もう一度 Read で読む。読めるはずだが。
	for (;;) {
		std::vector<char> loadbuf(256);
		auto n = stream->Read(loadbuf.data(), loadbuf.size());
		if (n < 0) {
			Trace(diag, "%s: Peek failed: %s", __method__, strerrno());
			return false;
		}
		if (n == 0) {
			break;
		}
		src.append(loadbuf.data(), n);
	}

	img.Create(width, height);

	// デコード。
	Blurhash bh(src);
	if (bh.Decode(img.GetBuf(), width, height) == false) {
		return false;
	}

	return true;
}

void
ImageLoaderBlurhash::SetSize(int width_, int height_)
{
	width = width_;
	height = height_;
}
