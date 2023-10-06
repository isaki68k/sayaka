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

#include "PeekableStream.h"
#include <cstring>
#include <assert.h>

//#define DEBUG 1
#if defined(DEBUG)
#include "Diag.h"
#define DPRINTF(fmt, ...) printf(fmt, ## __VA_ARGS__)
#else
#define DPRINTF(fmt, ...) /**/
#endif

// コンストラクタ
PeekableStream::PeekableStream(Stream *stream_)
{
	stream = stream_;
}

// デストラクタ
PeekableStream::~PeekableStream()
{
}

// x が先読みバッファ内にあれば true。end 側が閉区間であることに注意。
// 最終文字の一つ次を指す位置は先読みバッファへの追加が可能なので。
#define InPeekbuf(x) (peekstart <= (x) || (x) <= peekstart + peekbuf.size())

// 現在位置から dst に dstlen だけ読み出してポジションを進める。
ssize_t
PeekableStream::Peek(void *dst, size_t dstlen)
{
	DPRINTF("%s(dstlen=%zd) peekbuf=%zd\n", __method__, dstlen, peekbuf.size());

	if (peekbuf.empty()) {
		// 先読みバッファが空の場合は、
		// 下位ストリームから読んだデータを先読みバッファにも置く。
		peekstart = pos;
		auto len = stream->Read(dst, dstlen);
		DPRINTF("%s Read=%zd\n", __method__, len);
		if (len <= 0) {
			return len;
		}
		peekbuf.assign((char *)dst, len);
		return len;
	} else {
		// 先読みバッファがある場合。
		// 現在位置によって処理が変わる。
		//
		//      peekstart     peekend
		// - - -|*******************| - -> stream
		//  (1)     (2)            (3) (4) : pos
		//
		// (1): peekstart より前からの Peek() は EINVAL。
		// (2): [peekstart, peekend) での Peek() は先読みバッファから読み出す。
		// (3): peekend ちょうどの位置からの Peek() は、
		//      下位ストリームから読み出しながら先読みバッファに追加。
		// (4): peekend より先からの Peek() は EINVAL。

		// (1) or (4)
		auto peekend = peekstart + peekbuf.size();
		if (pos < peekstart || pos > peekend) {
			DPRINTF("%s Out of range: pos=%zd peek=(%zd, %zd)\n",
				__method__, pos, peekstart, peekend);
			errno = EINVAL;
			return -1;
		}

		if (pos != peekend) {
			// (2): 先読みバッファ内なので、先読みバッファから読み出す。
			auto len = std::min(peekend - pos, dstlen);
			DPRINTF("%s InPeekbuf: pos=%zd peekend=%zd len=%zd\n",
				__method__, pos, peekend, len);
			memcpy(dst, &peekbuf[pos - peekstart], len);
			pos += len;
			return len;
		} else {
			// (3): peekend ちょうどからなら、先読みバッファに追加。
			auto len = stream->Read(dst, dstlen);
			DPRINTF("%s Append: len=%zd\n", __method__, len);
			if (len <= 0) {
				return len;
			}
			peekbuf += std::string((const char *)dst, len);
			pos += len;
			return len;
		}
	}
}

ssize_t
PeekableStream::Read(void *dst, size_t dstlen)
{
	DPRINTF("%s(dstlen=%zd)\n", __method__, dstlen);

	if (__predict_false(peekbuf.empty() == false)) {
		// 内部バッファがあればそちらから読み出す。
		assert(InPeekbuf(pos));
		auto offset = pos - peekstart;
		auto len = std::min(peekbuf.size() - offset, dstlen);
		DPRINTF("%s FromPeekbuf: len=%zd\n", __method__, len);
		memcpy(dst, peekbuf.data() + offset, len);
		pos += len;
		offset += len;
		// 読み終わったら削除。
		if (offset >= peekbuf.size()) {
			DPRINTF("%s FromPeekbuf: clear\n", __method__);
			peekbuf.clear();
		}
		return len;
	} else {
		// 内部バッファがなければ下位ストリームから読み出す。
		auto len = stream->Read(dst, dstlen);
		DPRINTF("%s FromStream: len=%zd\n", __method__, len);
		if (__predict_true(len > 0)) {
			pos += len;
		}
		return len;
	}
}

bool
PeekableStream::Seek(ssize_t offset, int whence)
{
	size_t newpos;
	switch (whence) {
	 case SEEK_SET:
		newpos = offset;
		break;
	 case SEEK_CUR:
		newpos = pos + offset;
		break;
	 case SEEK_END:
	 default:
		return false;
	}
	DPRINTF("%s(newpos=%zd)\n", __method__, newpos);

	if (peekbuf.empty() == false && InPeekbuf(newpos)) {
		// newpos が先読みバッファ内なら
		// こっちで持ってる pos を移動するだけ。
		DPRINTF("%s InPeekbuf\n", __method__);
		pos = newpos;
	} else {
		// そうでなければ (バッファがないか範囲外なら)、下位ストリームに委任。
		bool r = stream->Seek(newpos, SEEK_SET);
		DPRINTF("%s Seek=%s\n", __method__, r ? "true" : "false");
		if (r == false) {
			return false;
		}
		pos = newpos;

		// 先読みバッファがあって範囲外に出たらバッファは無効。
		if (peekbuf.empty() == false && InPeekbuf(pos) == false) {
			DPRINTF("%s Seek: clear\n", __method__);
			peekbuf.clear();
		}
	}
	return true;
}
