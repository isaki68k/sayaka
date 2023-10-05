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
#include "ChunkedInputStream.h"
#include "MemoryStream.h"
#include "StringUtil.h"

void
test_ChunkedInputStream()
{
	Diag diag;

	printf("%s\n", __func__);

	// 空入力 (EOF)
	{
		MemoryInputStream src;
		ChunkedInputStream chunk(&src, diag);
		std::string str;
		auto r = chunk.ReadLine(&str);
		xp_eq(0, r);
		// EOF からもう一度読んでも EOF
		r = chunk.ReadLine(&str);
		xp_eq(0, r);
	}

	// 入力行あり
	{
		MemoryInputStream src;
		std::vector<uint8> data {
			'a','\r','\n',	// このチャンクのバイト数
			'0','1','2','3',	// 本文
			'4','5','6','7',
			'8','9',
			'\r','\n',		// 終端 CRLF
			'0','\n'		// このチャンクで終了 (LF のみの改行も許容したい)
		};
		src.AddData(data);
		ChunkedInputStream chunk(&src, diag);
		std::string str;
		// 戻り値は改行分を含んだバイト数
		auto r = chunk.ReadLine(&str);
		xp_eq(10, r);
		xp_eq("0123456789", str);

		r = chunk.ReadLine(&str);
		xp_eq(0, r);
	}

	// 複数チャンク
	{
		MemoryInputStream src;
		std::vector<uint8> data {
			'2', '\r', '\n',	// このチャンクのバイト数
			'a', '\r',			// 本文
			'\r', '\n',			// チャンク終端 CRLF

			'3', '\r', '\n',	// このチャンクのバイト数
			'\n','\r', '\n',	// 本文(2行目は空行)
			'\r', '\n',			// チャンク終端 CRLF

			'2', '\r', '\n',	// このチャンクのバイト数
			'b', 'c',			// 本文(3行目は改行なしで終端)
			'\r', '\n',			// チャンク終端 CRLF

			'0', '\r', '\n',	// このチャンクで終了
		};
		src.AddData(data);
		ChunkedInputStream chunk(&src, diag);
		// ReadLine() なので chunk 境界に関わらず行ずつ取り出している。
		// ついでに ReadLine(std::string*) のほうをテストする。
		std::string str;
		ssize_t r;
		// ReadLine() は読み込んだ行から改行を除いて返す。
		// 1行目 ("a\r\n")
		r = chunk.ReadLine(&str);
		xp_eq(3, r);
		xp_eq("a", str);

		// 2行目 ("\r\n")
		r = chunk.ReadLine(&str);
		xp_eq(2, r);
		xp_eq("", str);

		// 3行目 ("bc")。改行なしで終端すればそのまま返す
		r = chunk.ReadLine(&str);
		xp_eq(2, r);
		xp_eq("bc", str);

		// EOF
		r = chunk.ReadLine(&str);
		xp_eq(0, r);

		// EOF 後にもう一度読んでも EOF
		r = chunk.ReadLine(&str);
		xp_eq(0, r);
	}
}
