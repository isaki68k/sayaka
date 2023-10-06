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

#include "MemoryStream.h"
#include <cstring>
#include <cstdio>
#include <tuple>

// コンストラクタ
MemoryStream::MemoryStream()
{
}

// コンストラクタ
MemoryStream::MemoryStream(const std::vector<uint8>& src)
{
	Append(src);
}

// デストラクタ
MemoryStream::~MemoryStream()
{
}

// データを末尾に追加
void
MemoryStream::Append(const char *src, int srclen)
{
	std::vector<uint8> data;
	data.resize(srclen);
	for (auto& c : data) {
		c = *src++;
	}
	Append(data);
}

// データを末尾に追加
void
MemoryStream::Append(const std::vector<uint8>& src)
{
	chunks.emplace_back(src, 0);
}

// dst に読み出す
ssize_t
MemoryStream::Read(void *dst, size_t dstsize)
{
	ssize_t rv = 0;

	while (chunks.empty() == false && rv < dstsize) {
		auto& [buf, offset] = chunks.front();

		// コピー
		auto copylen = std::min(dstsize - rv, buf.size() - offset);
		memcpy((char *)dst + rv, buf.data() + offset, copylen);
		rv += copylen;
		offset += copylen;

		// 末尾まで読んだらこの chunk を捨てる
		if (offset >= buf.size()) {
			chunks.pop_front();
		}
	}

	return rv;
}

// このストリームの残りバイト数を返す。
size_t
MemoryStream::GetSize() const
{
	size_t size = 0;

	for (const auto& chunk : chunks) {
		auto& [buf, offset] = chunk;

		size += buf.size() - offset;
	}
	return size;
}
