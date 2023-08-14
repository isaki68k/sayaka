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

		auto pair = UString::UCharFromUTF8(srcstr.c_str() + byteoffset);
		auto [ code, bytelen ] = pair;

		rc.code = code;
		byteoffset += bytelen;
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
		uint32 abscode = abs((int32)c.code);

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
			if ((int32)c.code < 0) {
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
