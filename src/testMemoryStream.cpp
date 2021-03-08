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

#include "test.h"
#include "MemoryStream.h"

void
test_MemoryInputStream()
{
	printf("%s\n", __func__);

	// 空コンストラクタ
	{
		MemoryInputStream ms;
		xp_eq(0, ms.GetSize());
	}
	// chunk1つを小分けに取り出す
	{
		std::vector<uint8> src { 'a', 'b', 'c' };
		MemoryInputStream ms(src);
		xp_eq(3, ms.GetSize());

		char buf[2];
		memset(buf, 0, sizeof(buf));
		auto actual = ms.Read(buf, sizeof(buf));
		xp_eq(2, actual);
		xp_eq(1, ms.GetSize());
		xp_eq('a', buf[0]);
		xp_eq('b', buf[1]);

		memset(buf, 0, sizeof(buf));
		actual = ms.Read(buf, sizeof(buf));
		xp_eq(1, actual);
		xp_eq(0, ms.GetSize());
		xp_eq('c', buf[0]);

		actual = ms.Read(buf, sizeof(buf));
		xp_eq(0, actual);
		xp_eq(0, ms.GetSize());
	}
	// 複数chunk
	{
		std::vector<uint8> src { 'a', 'b' };
		MemoryInputStream ms(src);

		ms.AddData(src);

		char buf[4];
		memset(buf, 0, sizeof(buf));
		auto actual = ms.Read(buf, sizeof(buf));
		xp_eq(4, actual);
		xp_eq(0, ms.GetSize());
		xp_eq('a', buf[0]);
		xp_eq('b', buf[1]);
		xp_eq('a', buf[2]);
		xp_eq('b', buf[3]);
	}

	// Peek1
	{
		std::vector<uint8> src { 'a', 'b', 'c' };
		MemoryInputStream ms(src);

		// Peek してみる
		std::vector<uint8> buf(2);
		auto actual = ms.Peek(buf.data(), buf.size());
		xp_eq(2, actual);
		xp_eq('a', buf[0]);
		xp_eq('b', buf[1]);

		// Read すると同じものが読める
		actual = ms.Read(buf.data(), 1);
		xp_eq(1, actual);
		xp_eq('a', buf[0]);
		actual = ms.Read(buf.data(), 1);
		xp_eq(1, actual);
		xp_eq('b', buf[0]);

		// 残りを Read
		actual = ms.Read(buf.data(), buf.size());
		xp_eq(1, actual);
		xp_eq('c', buf[0]);
	}

	// Peek2
	{
		std::vector<uint8> src { 'a', 'b', 'c' };
		MemoryInputStream ms(src);

		// Peek してみる
		std::vector<uint8> buf(2);
		auto actual = ms.Peek(buf.data(), buf.size());
		xp_eq(2, actual);
		xp_eq('a', buf[0]);
		xp_eq('b', buf[1]);

		// Peek した以上に Read する
		buf.resize(3);
		actual = ms.Read(buf.data(), buf.size());
		xp_eq(3, actual);
		xp_eq('a', buf[0]);
		xp_eq('b', buf[1]);
		xp_eq('c', buf[2]);
	}

	// Peek3
	{
		std::vector<uint8> src { 'a', 'b', 'c', 'd' };
		MemoryInputStream ms(src);
		std::vector<uint8> buf(4);

		// 2バイト Peek
		auto actual = ms.Peek(buf.data(), 2);
		xp_eq(2, actual);
		xp_eq('a', buf[0]);
		xp_eq('b', buf[1]);

		// 今度は 3バイト Peek してみる。まだポインタは先頭にある
		actual = ms.Peek(buf.data(), 3);
		xp_eq(3, actual);
		xp_eq('a', buf[0]);
		xp_eq('b', buf[1]);
		xp_eq('c', buf[2]);

		// 1バイト Read
		actual = ms.Read(buf.data(), 1);
		xp_eq(1, actual);
		xp_eq('a', buf[0]);

		// ここから 3バイト Peek。1バイト目から3バイト覗き見れるはず
		actual = ms.Peek(buf.data(), 3);
		xp_eq(3, actual);
		xp_eq('b', buf[0]);
		xp_eq('c', buf[1]);
		xp_eq('d', buf[2]);
	}
}
