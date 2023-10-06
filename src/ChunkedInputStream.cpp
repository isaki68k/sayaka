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

//
// ChunkedInputStream
//

#include "ChunkedInputStream.h"
#include "StringUtil.h"
#include "subr.h"
#include <cstring>
#include <memory>
#include <assert.h>
#include <errno.h>

// コンストラクタ
ChunkedInputStream::ChunkedInputStream(Stream *src_, Diag& diag_)
	: diag(diag_)
{
	src = src_;

	// ここでは CRLF はデフォルト
}

// デストラクタ
ChunkedInputStream::~ChunkedInputStream()
{
}

ssize_t
ChunkedInputStream::Read(void *dst, size_t dstsize)
{
	Trace(diag, "Read(%zd)", dstsize);

	// バッファが空なら次のチャンクを読み込む。
	if (chunk.empty()) {
		Trace(diag, "Need to fill");
		auto r = ReadChunk();
		Trace(diag, "ReadChunk %zd", r);
		if (__predict_false(r < 0)) {
			return -1;
		}

		if (__predict_false(r == 0)) {
			return 0;
		}
	}

	// バッファから dst に入るだけコピー。
	auto copylen = std::min(chunk.size() - chunkpos, dstsize);
	Trace(diag, "copylen=%zd\n", copylen);
	memcpy(dst, chunk.data() + chunkpos, copylen);
	chunkpos += copylen;
	// 末尾まで読んだら捨てる。
	if (chunkpos == chunk.size()) {
		chunk.clear();
		chunkpos = 0;
	}
	return copylen;
}

// 1つのチャンクを読み込んで内部バッファ chunk に代入する。
// 成功すれば読み込んだバイト数を返す。
// 失敗すれば errno をセットして -1 を返す。
ssize_t
ChunkedInputStream::ReadChunk()
{
	std::string slen;
	ssize_t r;

	assert(chunk.empty());

	// 先頭行はチャンク長+CRLF
	r = src->ReadLine(&slen);
	if (__predict_false(r < 0)) {
		Debug(diag, "ReadLine failed: %s", strerrno());
		return -1;
	}
	if (__predict_false(r == 0)) {
		// EOF
		Trace(diag, "Unexpected EOF while reading chunk length.");
		return 0;
	}

	// チャンク長を取り出す
	char *end;
	int intlen = stox32def(slen.c_str(), -1, &end);
	if (intlen < 0) {
		Debug(diag, "Invalid chunk length: %s", slen.c_str());
		errno = EIO;
		return -1;
	}
	if (*end != '\0') {
		Debug(diag, "Chunk length has a trailing garbage: %s", slen.c_str());
		errno = EIO;
		return -1;
	}
	Trace(diag, "intlen=%d", intlen);

	if (intlen == 0) {
		// データ終わり。CRLF を読み捨てる
		src->ReadLine(&slen);
		Trace(diag, "This was the last chunk.");
		return 0;
	}

	// チャンク本体を読み込む
	chunk.resize(intlen);
	chunkpos = 0;
	int readlen = 0;
	for (; readlen < intlen; readlen += r) {
		r = src->Read(chunk.data() + readlen, intlen - readlen);
		if (__predict_false(r < 0)) {
			Debug(diag, "Read failed: %s", strerrno());
			errno = EIO;
			return -1;
		}
		if (__predict_false(r == 0)) {
			break;
		}
		Trace(diag, "readlen=%d", readlen);
	}
	if (__predict_false(readlen != intlen)) {
		Debug(diag, "readlen=%d intlen=%d", readlen, intlen);
		errno = EIO;
		return -1;
	}

	// 最後の CRLF を読み捨てる
	src->ReadLine(&slen);

	return intlen;
}
