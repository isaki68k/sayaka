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

#include "FileStream.h"
#include <errno.h>

//
// FILE* ストリーム。
//

// 空のコンストラクタ
FileStream::FileStream()
{
}

// コンストラクタ
FileStream::FileStream(FILE *fp_, bool own_)
{
	fp = fp_;
	own = own_;
}

// コンストラクタ
FileStream::FileStream(const std::string& filename, const char *mode)
{
	Open(filename, mode);
}

// ムーブコンストラクタ
FileStream::FileStream(FileStream&& other)
{
	fp  = other.fp;
	own = other.own;

	other.fp = NULL;
	other.own = false;
}

// デストラクタ
FileStream::~FileStream()
{
	Close();
}

// オープン。
bool
FileStream::Open(const std::string& filename, const char *mode)
{
	Close();

	own = true;
	fp = fopen(filename.c_str(), mode);
	return (fp != NULL);
}

// クローズ。
void
FileStream::Close()
{
	if (own) {
		// 所有権があれば解放。
		if (fp) {
			fclose(fp);
		}
		fp = NULL;
	} else {
		// 所有権がなければ何もしない。
		// XXX クローズしたこと、くらいにはしてもいいかもしれないが。
	}
}

// 読み込み。
ssize_t
FileStream::Read(void *dst, size_t dstlen)
{
	size_t n = fread(dst, 1, dstlen, fp);
	if (__predict_false(n == 0)) {
		if (feof(fp)) {
			return 0;
		} else {
			errno = ferror(fp);
			return -1;
		}
	}

	return n;
}
