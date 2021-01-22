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
// FILE* 入力ストリーム
//

// コンストラクタ
FileInputStream::FileInputStream(FILE *fp_, bool own_)
{
	fp = fp_;
	own = own_;
}

// デストラクタ
FileInputStream::~FileInputStream()
{
	Close();
}

// dst に読み出す
ssize_t
FileInputStream::NativeRead(void *dst, size_t dstsize)
{
	ssize_t rv = 0;

	while (rv < dstsize) {
		size_t reqsize = dstsize - rv;
		size_t n = fread((char *)dst + rv, 1, reqsize, fp);
		if (__predict_false(n == 0)) {
			if (feof(fp)) {
				return 0;
			} else {
				errno = ferror(fp);
				return -1;
			}
		}
		rv += n;
	}

	return rv;
}

void
FileInputStream::Close()
{
	// この fp を所有するモードならクローズ
	if (own) {
		if (fp) {
			fclose(fp);
		}
		fp = NULL;
	}
}


//
// FILE* 出力ストリーム
//

// コンストラクタ
FileOutputStream::FileOutputStream(FILE *fp_, bool own_)
{
	fp = fp_;
	own = own_;
}

// デストラクタ
FileOutputStream::~FileOutputStream()
{
	Close();
}

// buf を書き出す
ssize_t
FileOutputStream::Write(const void *buf, size_t len)
{
	return fwrite(buf, 1, len, fp);
}

// フラッシュする
void
FileOutputStream::Flush()
{
	fflush(fp);
}

void
FileOutputStream::Close()
{
	// この fp を所有するモードならクローズ
	if (own) {
		if (fp) {
			fclose(fp);
		}
		fp = NULL;
	}
}
