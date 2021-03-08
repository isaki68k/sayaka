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
#include "term.h"

Diag diag;

void
test_parse_bgcolor()
{
	printf("%s\n", __func__);

	std::vector<std::pair<std::string, enum bgcolor>> table = {
		{ ESC "]11;rgb:0000/0000/0000" ESC "\\",	BG_BLACK },
		{ ESC "]11;rgb:ffff/ffff/ffff" ESC "\\",	BG_WHITE },

		// ヘッダ部分が誤り
		{ ESC "]0;rgb:0100/0100/0100"  ESC "\\",	BG_BLACK },
		// RGB が各2桁
		{ ESC "]11;rgb:f0/f0/f0"       ESC "\\",	BG_WHITE },
		// 実は RGB は何桁でも受け付けている
		{ ESC "]11;rgb:f/fff/fffff"    ESC "\\",	BG_WHITE },
	};
	for (const auto& a : table) {
		const auto& src = a.first;
		auto expected = a.second;

		char buf[128];
		strlcpy(buf, src.c_str(), sizeof(buf));
		auto actual = parse_bgcolor(buf);
		xp_eq((int)expected, (int)actual, termdump(src.c_str()).c_str());
	}
}

void
test_term()
{
	test_parse_bgcolor();
}
