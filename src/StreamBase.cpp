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
	ssize_t rv = 0;

	// すでに Peek() で読んだのがあれば使う
	auto peeksize = peekbuf.size();
	if (__predict_false(peeksize > 0)) {
		auto len = std::min(peeksize, dstsize);
		memcpy(dst, peekbuf.data(), len);
		rv = len;
		peekbuf.erase(peekbuf.begin(), peekbuf.begin() + len);

		if (rv >= dstsize) {
			return rv;
		}
	}

	// 残りを読み込む
	auto r = NativeRead((char *)dst + rv, dstsize - rv);
	if (r < 0) {
		return r;
	}
	rv += r;
	return rv;
}

// 覗き見する。
ssize_t
InputStream::Peek(void *dst, size_t dstsize)
{
	ssize_t rv = 0;

	// (不足なら)内部バッファに追加する
	auto peeksize = peekbuf.size();
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
	rv = std::min(peekbuf.size(), dstsize);
	memcpy(dst, peekbuf.data(), rv);
	return rv;
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
