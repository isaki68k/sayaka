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
#include "RichString.h"

void
test_RichString()
{
	printf("%s\n", __func__);

	// Twitter のタグの位置とかは Unicode の正しい文字数の数え方ではなく、
	// 何個目のコードポイント(?)かだけでカウントしているようなので、
	// どんな不思議な合字が来ても1個ずつカウントする。

	std::vector<std::pair<std::string, std::vector<int>>> table = {
		// テスト表示名,入力文字列						期待値
		{ "A,A!",										{ 0, 1, 2 } },

		// UTF-16 でサロゲートペアになる文字 (UTF-8/32 では関係ない)
		// U+20BB7 (吉野家のツチヨシ)
		{ "吉,\xf0\xa0\xae\xb7" "!",					{ 0, 1, 2 } },

		// IVS
		// "葛" U+845B (IVSなし) (= くさかんむりに日に匂)
		{ "葛,\xe8\x91\x9b" "!",						{ 0, 1, 2 } },
		// "葛" U+845B U+E0101 (IVSあり) (= くさかんむりに曷)
		// https://seiai.ed.jp/sys/text/csd/cf14/c14a090.html
		// https://xtech.nikkei.com/it/article/COLUMN/20100126/343783/
		{ "葛IVS,\xe8\x91\x9b" "\xf3\xa0\x84\x81" "!",	{ 0, 1, 2, 3 } },

		// SVS
		// https://qiita.com/_sobataro/items/47989ee4b573e0c2adfc
		// U+231b "Hourglass" (SVSなし)
		{ "HG,\xe8\x8c\x9b" "!",						{ 0, 1, 2 } },
		// U+231b U+FE0E (TPVS)
		{ "HG+TPVS,\xe8\x8c\x9b\xef\xb8\x8e" "!",		{ 0, 1, 2, 3 } },
		// U+231b U+FE0F (EPVS)
		{ "HG+EPVS,\xe8\x8c\x9b\xef\xb8\x8f" "!",		{ 0, 1, 2, 3 } },

#if 0
		// VS が連続すると2つ目のほうを独立した1文字と数えたようだ。
		// どう解釈したらそうなるのか分からんけど。
		// ← これは別の原因でデータがバグったんじゃないかという気がする。
		{ "VS2,\xe8\x8c\x9b" "\xef\xb8\x8f" "\xef\xb8\x8e" "!",
														{ 0, 0, 1, 2, 3 } },
#endif

		// Emoji Combining Sequence (囲み文字)
		//        1   U+FE0F         U+20E3
		{ "Keycap,1" "\xef\xb8\x8f" "\xe2\x83\xa3" "!",	{ 0, 1, 2, 3, 4 } },

		// (EPVS を挟まない)囲み文字
		//         2   U+20E3
		{ "Keycap2,2" "\xe2\x83\xa3" "!",				{ 0, 1, 2, 3 } },

		// Skin tone
		{ "Skin,\xf0\x9f\x91\xa8" "\xf0\x9f\x8f\xbd" "!", { 0, 1, 2, 3 } },

		// Regional Indicator (国旗絵文字)
		{ "Flag,\xf0\x9f\x87\xaf" "\xf0\x9f\x87\xb5"
		       "\xf0\x9f\x87\xaf" "\xf0\x9f\x87\xb5",	{ 0, 1, 2, 3, 4 } },

		// 正しくない UTF-8 ストリーム
		// あ            (不正) あ
		{ "Invalid,\xe3\x81\x82" "\x80" "\xe3\x81\x82",	{ 0, 1, 2, 3 } },
	};
	for (const auto& a : table) {
		const auto& name_input = Split2(a.first, ',');
		const auto& expected = a.second;

		const auto& testname = name_input.first;
		const auto& input = name_input.second;

		RichString rtext(input);
		if (rtext.size() == expected.size()) {
			for (int i = 0; i < expected.size(); i++) {
				xp_eq(expected[i], rtext[i].charoffset,
					testname + string_format("[%d]", i));
			}
		} else {
			xp_eq(expected.size(), rtext.size(), testname);
			printf("expected");
			for (auto& x : expected) {
				printf(" %d", x);
			}
			printf(" but");
			for (auto& c : rtext) {
				printf(" %d", c.charoffset);
			}
			printf("\n");
		}
	}
}
