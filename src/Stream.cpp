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

#include "Stream.h"

//
// ストリーム (基本クラス)
//

// デストラクタ
Stream::~Stream()
{
}

// クローズ。
void
Stream::Close()
{
}

// 読み込み (ダミー)
ssize_t
Stream::Read(void *dst, size_t dstlen)
{
	errno = EBADF;
	return -1;
}

// 書き込み (ダミー)
ssize_t
Stream::Write(const void *src, size_t srclen)
{
	errno = EBADF;
	return -1;
}

// フラッシュ (ダミー)
void
Stream::Flush()
{
}

// シーク (ダミー)
bool
Stream::Seek(ssize_t offset, int whence)
{
	errno = ESPIPE;
	return -1;
}

// 1行読み出す。
ssize_t
Stream::ReadLine(std::string *retval)
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
