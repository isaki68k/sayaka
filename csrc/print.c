/* vi:set ts=4: */
/*
 * Copyright (C) 2014-2024 Tetsuya Isaki
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

//
// 文字列の表示周り
//

#include "sayaka.h"

// src をインデントをつけて出力する。
void
iprint(const ustring *src)
{
	if (0) {
		const unichar *arr = ustring_get(src);
		for (uint i = 0, end = ustring_len(src); i < end; i++) {
			printf("%s src [%u] %02x\n", __func__, i, arr[i]);
		}
	}

	// Stage1: Unicode 文字単位でいろいろフィルターかける。
	ustring *utext = ustring_init();

	const unichar *s = ustring_get(src);
	for (uint i = 0, end = ustring_len(src); i < end; i++) {
		unichar uni = s[i];

		// Private Use Area (外字) をコードポイント形式(?)にする
		if (__predict_false((  0xe000 <= uni && uni <=   0xf8ff))	// BMP
		 || __predict_false(( 0xf0000 <= uni && uni <=  0xffffd))	// 第15面
		 || __predict_false((0x100000 <= uni && uni <= 0x10fffd))) 	// 第16面
		{
			string *tmp = string_alloc(16);
			string_append_printf(tmp, "<U+%X>", uni);
			ustring_append_ascii(utext, string_get(tmp));
			continue;
		}

		// Mathematical Alphanumeric Symbols を全角英数字に変換

		// --no-combine なら Combining Enclosing * (U+20DD-U+20E4) の前に

		// codeset

		ustring_append_unichar(utext, uni);
	}

	if (0) {
		const unichar *arr = ustring_get(utext);
		for (uint i = 0, end = ustring_len(utext); i < end; i++) {
			printf("%s [%u] %02x\n", __func__, i, arr[i]);
		}
	}

	// 出力文字コードに変換。
	string *outstr = ustring_to_utf8(utext);
	fputs(string_get(outstr), stdout);
}
