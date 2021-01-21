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

#include "RichString.h"
#include "UString.h"
#include "StringUtil.h"

// コンストラクタ
RichString::RichString(const std::string& text_)
{
	text = text_;

	// 先頭からの文字数とバイト数を数える
	MakeInfo(this, text_);
}

// UTF-8 文字列 srcstr から内部情報 info を作成する。
//
// 最後の文字のバイト長も統一的に扱うため、末尾に終端文字があるとして
// そのオフセットも保持しておく。そのため以下のように2文字の文字列なら
// TextTag 配列は要素数 3 になる。
//
// "あA" の2文字の場合
//                       +0    +1    +2    +3    +4
//                       'あ'               'A'
//                      +-----+-----+-----+-----+
//               text = | $e3   $81   $82 | $41 |
//                      +-----+-----+-----+-----+
//                       ^                 ^      ^
//                       |                 |      :
// info[0].byteoffset=0 -+                 |      :
// info[0].charoffset=0                    |      :
//                                         |      :
// info[1].byteoffset=3 -------------------+      :
// info[1].charoffset=1                           :
//                                                :
// info[2].byteoffset=4 - - - - - - - - - - - - - +
// info[2].charoffset=2
//
bool
RichString::MakeInfo(std::vector<RichChar> *info_, const std::string& srcstr)
	const
{
	std::vector<RichChar>& info = *info_;

	int charoffset = 0;
	int byteoffset = 0;
	for (int end = srcstr.size(); byteoffset < end; ) {
		// この文字
		RichChar rc;

		rc.byteoffset = byteoffset;
		rc.charoffset = charoffset++;

		// UTF-8 は1バイト目でこの文字のバイト数が分かる
		uint8 c = srcstr[byteoffset];
		int bytelen;
		if (__predict_true(c < 0x80)) {
			bytelen = 1;
		} else if (__predict_true(0xc0 <= c && c < 0xe0)) {
			bytelen = 2;
		} else if (__predict_true(0xe0 <= c && c < 0xf0)) {
			bytelen = 3;
		} else if (__predict_true(0xf0 <= c && c < 0xf8)) {
			bytelen = 4;
		} else if (__predict_true(0xf8 <= c && c < 0xfc)) {
			bytelen = 5;
		} else if (__predict_true(0xfc <= c && c < 0xfe)) {
			bytelen = 6;
		} else {
			// こないはずだけど、とりあえず
			bytelen = 1;
		}
		byteoffset += bytelen;

		// この1文字を UTF-32 に変換
		const char *src = &srcstr[rc.byteoffset];
		size_t srcleft = bytelen;
		rc.code = UString::UCharFromUTF8(&src, srcleft);
		if (0) {
			printf("%02X\n", rc.code);
		}

		info.emplace_back(rc);
	}

	// 終端文字分を持っておくほうが何かと都合がよい
	RichChar t;
	t.code = 0;
	t.byteoffset = byteoffset;
	t.charoffset = charoffset;
	info.emplace_back(t);

	return true;
}

std::string
RichString::dump() const
{
	std::string rv;

	// 通常の文字配列とは異なり、これは終端 '\0' も1要素を持つ配列
	for (int i = 0, sz = size(); i < sz; i++) {
		const auto& c = (*this)[i];
		uint32_t abscode = abs((int32_t)c.code);

		rv += string_format("[%d] char=%d byte=%d U+%02x ",
			i, c.charoffset, c.byteoffset, abscode);

		if (__predict_true(i < sz - 1)) {
			// 最後の一つ手前までが通常の文字列。
			// ただしこの c のバイト数は次の文字(next) の byteoffset を
			// 見ないと分からない。
			const auto& next = (*this)[i + 1];
			auto bytelen = next.byteoffset - c.byteoffset;
			rv += '\'';
			// とりあえず雑に改行だけ対処
			if (abscode == '\n') {
				rv += "\\n";
			} else {
				rv += text.substr(c.byteoffset, bytelen);
			}
			rv += '\'';
			if ((int32_t)c.code < 0) {
				rv += " Del";
			}
			if (bytelen > 1) {
				for (int j = c.byteoffset; j < next.byteoffset; j++) {
					rv += string_format(" %02x", (unsigned char)text[j]);
				}
			}
		} else {
			// 最後のエントリ (終端文字) なので next はないし必要ない。
		}

		if (!c.altesc.empty()) {
			rv += " altesc=";
			for (int j = 0; j < c.altesc.size(); j++) {
				rv += string_format(" %02x", (unsigned char)c.altesc[j]);
			}
		}
		if (!c.alturl.empty()) {
			rv += " alturl=|" + c.alturl + "|";
		}
		rv += "\n";
	}

	return rv;
}

#if defined(SELFTEST)
#include "test.h"

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
#endif
