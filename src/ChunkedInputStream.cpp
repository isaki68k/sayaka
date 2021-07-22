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
#include <errno.h>

// コンストラクタ
ChunkedInputStream::ChunkedInputStream(InputStream *src_, Diag& diag_)
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
ChunkedInputStream::NativeRead(void *dst, size_t dstsize)
{
	Trace(diag, "Read(%zd)", dstsize);

	// 要求サイズに満たない間 src から1チャンクずつ読み込む
	for (;;) {
		std::string slen;
		ssize_t r;

		// chunksLength は内部バッファ長
		size_t chunksLength = Chunks.GetSize();
		Trace(diag, "dstsize=%zd chunksLength=%zd", dstsize, chunksLength);
		if (chunksLength >= dstsize) {
			Trace(diag, "Filled");
			break;
		} else {
			Trace(diag, "Need to fill");
		}

		// 先頭行はチャンク長+CRLF
		r = src->ReadLine(&slen);
		if (__predict_false(r < 0)) {
			Debug(diag, "ReadLine failed: %s", strerrno());
			return -1;
		}
		if (__predict_false(r == 0)) {
			// EOF
			Trace(diag, "src is EOF");
			break;
		}

		// チャンク長を取り出す
		char *end;
		int intlen = stox32def(slen.c_str(), -1, &end);
		if (intlen < 0) {
			Debug(diag, "Invalid chunk length: %s", slen.c_str());
			errno = 0;
			return -1;
		}
		if (*end != '\0') {
			Debug(diag, "Chunk length has a trailing garbage: %s",
				slen.c_str());
			errno = EIO;
			return -1;
		}
		Trace(diag, "intlen=%d", intlen);

		if (intlen == 0) {
			// データ終わり。CRLF を読み捨てる
			src->ReadLine(&slen);
			Trace(diag, "This was the last chunk");
			break;
		}

		std::unique_ptr<char[]> bufp = std::make_unique<char[]>(intlen);
		ssize_t readlen = src->Read(bufp.get(), intlen);
		if (__predict_false(readlen < 0)) {
			Debug(diag, "Read failed: %s", strerrno());
			return -1;
		}
		Trace(diag, "readlen=%zd", readlen);
		if (__predict_false(readlen != intlen)) {
			Debug(diag, "readlen=%zd intlen=%d", readlen, intlen);
			errno = EIO;
			return -1;
		}

		// 内部バッファに追加
		Chunks.AddData(bufp.get(), intlen);

		// 最後の CRLF を読み捨てる
		src->ReadLine(&slen);
	}

	// dst に入るだけコピー
	auto copylen = Chunks.Read(dst, dstsize);
	Trace(diag, "copylen=%zd\n", copylen);

	// Chunks の作り直しは C++ では不要なはず

	return copylen;
}
