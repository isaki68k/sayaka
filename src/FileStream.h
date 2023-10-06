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

#pragma once

#include "Stream.h"
#include <cstdio>

// FILE* ストリーム
class FileStream : public Stream
{
 public:
	FileStream(FILE *fp_, bool own_);
	virtual ~FileStream() override;

	void Close() override;

	ssize_t Read(void *dst, size_t dstlen) override;

	ssize_t Write(const void *src, size_t srclen) override {
		return fwrite(src, 1, srclen, fp);
	}

	void Flush() override {
		fflush(fp);
	}

	// 成功すれば true を、
	// 失敗すれば errno をセットして false を返す。
	bool Seek(ssize_t offset, int whence) override {
		auto r = fseek(fp, (long)offset, whence);
		return (r == 0);
	}

	ssize_t GetPos() const {
		return ftell(fp);
	}

 private:
	FILE *fp {};
	bool own {};
};
