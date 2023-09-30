/*
 * Copyright (C) 2021 Tetsuya Isaki
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

#include "StreamBase.h"
#include <cstring>

//
// 入力ストリームの基本クラス
//

// デストラクタ
InputStream::~InputStream()
{
}

// 読み出す。
ssize_t
InputStream::Read(void *dst, size_t dstsize)
{
	ssize_t copylen = 0;

	// pos が内部バッファ内にいればまずそっちを使う。
	if (pos < peekbuf.size()) {
		auto len = std::min(peekbuf.size() - pos, dstsize);
		memcpy(dst, peekbuf.data() + pos, len);
		pos += len;
		copylen += len;
	}

	// 足りなければ読み込む。
	if (copylen < dstsize) {
		auto r = NativeRead((char *)dst + copylen, dstsize - copylen);
		if (r < 0) {
			return r;
		}
		pos += r;
		copylen += r;
	}
	return copylen;
}

// 覗き見する。
ssize_t
InputStream::Peek(void *dst, size_t dstsize)
{
	ssize_t rv = 0;

	// 内部バッファを超えていたらもうだめ。
	if (pos > peekbuf.size()) {
		return -1;
	}

	// (不足なら)内部バッファに追加する
	auto peeksize = peekbuf.size() - pos;
	if (peeksize < dstsize) {
		std::vector<uint8> tmp(dstsize - peeksize);
		auto r = NativeRead(tmp.data(), tmp.size());
		if (__predict_false(r <= 0)) {
			return r;
		}

		tmp.resize(r);
		for (const auto c : tmp) {
			peekbuf.emplace_back(c);
		}
	}

	// その上で内部バッファから取り出す
	rv = std::min(peekbuf.size() - pos, dstsize);
	memcpy(dst, peekbuf.data() + pos, rv);
	pos += rv;
	return rv;
}

off_t
InputStream::Seek(off_t offset, int whence)
{
	size_t newpos;

	switch (whence) {
	 case SEEK_SET:
		newpos = (size_t)offset;
		break;
	 case SEEK_CUR:
		newpos = pos + (size_t)offset;
		break;
	 case SEEK_END:
	 default:
		return -1;	// Not supported
	}

	if (pos <= peekbuf.size()) {
		// 現在内部バッファ内にいて
		if (newpos <= peekbuf.size()) {
			// 内部バッファ内に移動する場合。
			pos = newpos;
			goto done;
		} else {
			// 内部バッファから出る場合。読み捨てる。
			pos = peekbuf.size();
		}
	} else {
		// 現在内部バッファ内にいない場合。
		if (newpos < pos) {
			// 戻れない。
			return -1;
		} else {
			// 進む場合は読み捨てる。
		}
	}

	// pos から newpos まで読み捨てる。
	if (newpos > pos) {
		std::vector<uint8> tmp(newpos - pos);
		auto r = NativeRead(tmp.data(), tmp.size());
		if (r > 0) {
			pos += r;
		}
	}

 done:
	return (off_t)pos;
}

// 1行読み出す。
ssize_t
InputStream::ReadLine(std::string *retval)
{
	std::string& str = *retval;
	char buf[1];
	ssize_t retlen;

	str.clear();
	retlen = 0;

	for (;;) {
		ssize_t readlen;
		readlen = Read(buf, sizeof(buf));
		if (__predict_false(readlen < 0))
			return readlen;

		if (__predict_false(readlen == 0)) {
			break;
		}

		str += buf[0];
		retlen++;
		if (buf[0] == '\n')
			break;
	}

	// 返す文字列から改行を削除
	for (;;) {
		char c = str.back();
		if (c == '\r' || c == '\n') {
			str.pop_back();
		} else {
			break;
		}
	}

	// (改行を削除する前の) 受信したバイト数を返す
	return retlen;
}

// クローズ
void
InputStream::Close()
{
}

//
// 出力ストリームの基本クラス
//

// デストラクタ
OutputStream::~OutputStream()
{
}

// クローズ
void
OutputStream::Close()
{
}
