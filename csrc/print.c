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

uint indent_depth;				// 現在のインデント深さ
const char *output_codeset;		// 出力文字コード (NULL なら UTF-8)
bool opt_mathalpha;				// Mathematical AlphaNumeric を全角英数字に変換
bool opt_nocombine;				// Combining Enclosing Keycap を合成しない

// src をインデントをつけて出力する。
void
iprint(const ustring *src)
{
	// Stage1: Unicode 文字単位でいろいろフィルターかける。
	ustring *utext = ustring_init();

	const unichar *s = ustring_get(src);
	uint srclen = ustring_len(src);

	if (0) {
		char header[32];
		snprintf(header, sizeof(header), "%s src", __func__);
		ustring_dump(src, header);
	}

	for (uint i = 0; i < srclen; i++) {
		unichar uni = s[i];

		// Private Use Area (外字) をコードポイント形式(?)にする
		if (__predict_false((  0xe000 <= uni && uni <=   0xf8ff))	// BMP
		 || __predict_false(( 0xf0000 <= uni && uni <=  0xffffd))	// 第15面
		 || __predict_false((0x100000 <= uni && uni <= 0x10fffd))) 	// 第16面
		{
			char buf[16];
			snprintf(buf, sizeof(buf), "<U+%X>", uni);
			ustring_append_ascii(utext, buf);
			continue;
		}

		// Mathematical Alphanumeric Symbols を全角英数字に変換
		if (__predict_false(opt_mathalpha) &&
			__predict_false(0x1d400 <= uni && uni <= 0x1d7ff))
		{
#if 0
			// Mathematical Alphanumeric Symbols を全角英数字に変換
			utext.Append(ConvMathAlpha(uni));
#endif
			continue;
		}

		// --no-combine なら Combining Enclosing * (U+20DD-U+20E4) の前に
		// スペースを入れて、囲まれるはずだった文字とは独立させる。
		// 前の文字(たいていただの ASCII 数字)が潰されて読めなくなるのを
		// 防ぐため。
		// U+20E1 は「上に左右矢印を前の文字につける」で囲みではないが
		// 面倒なので混ぜておく。なぜ間に入れたのか…。
		if (opt_nocombine &&
			__predict_false(0x20dd <= uni && uni <= 0x20e4))
		{
			ustring_append_unichar(utext, 0x20);
		}

		if (__predict_false(output_codeset)) {
			// JIS/EUC-JP(/Shift-JIS) に変換する場合のマッピング
			// 本当は変換先がこれらの時だけのほうがいいだろうけど。
#if 0
#endif
		}

		ustring_append_unichar(utext, uni);
	}

	if (0) {
		char header[32];
		snprintf(header, sizeof(header), "%s utext", __func__);
		ustring_dump(utext, header);
	}

	// Stage2: インデントつけていく。
	ustring *utext2 = ustring_alloc(ustring_len(utext) + 32);

	// インデント階層
	uint left = indent_cols * (indent_depth + 1);
	char indent[12];
	{
		char *p = indent;
		*p++ = ESCchar;
		*p++ = '[';
		p += PUTD(p, left, sizeof(indent) - (p - indent));
		*p++ = 'C';
		*p = '\0';
	}
	ustring_append_ascii(utext2, indent);

	if (__predict_false(screen_cols == 0)) {
		// 桁数が分からない場合は何もしない
		ustring_append(utext2, utext);
	} else {
		// 1文字ずつ文字幅を数えながら出力用に整形していく
		uint in_escape = 0;
		uint x = left;
		const unichar *utextbuf = ustring_get(utext);
		unichar uni;
		for (int i = 0; (uni = utextbuf[i]) != 0; i++) {
			if (__predict_false(in_escape > 0)) {
				// 1: ESC直後
				// 2: ESC [
				// 3: ESC (
				ustring_append_unichar(utext2, uni);
				switch (in_escape) {
				 case 1:
					// ESC 直後の文字で二手に分かれる
					if (uni == '[') {
						in_escape = 2;
					} else {
						in_escape = 3;	// 手抜き
					}
					break;
				 case 2:
					// ESC [ 以降 'm' まで
					if (uni == 'm') {
						in_escape = 0;
					}
					break;
				 case 3:
					// ESC ( の次の1文字だけ
					in_escape = 0;
					break;
				}
			} else {
				if (uni == ESCchar) {
					ustring_append_unichar(utext2, uni);
					in_escape = 1;
				} else if (uni == '\n') {
					ustring_append_unichar(utext2, uni);
					ustring_append_ascii(utext2, indent);
					x = left;
				} else {
					// 文字幅を取得
#if 0
					uint width = get_eaw_width(uni);
#else
					uint width = (uni < 0x80) ? 1 : 2;
#endif
					if (width == 1) {
						ustring_append_unichar(utext2, uni);
						x++;
					} else {
						assert(width == 2);
						if (x > screen_cols - 2) {
							ustring_append_unichar(utext2, '\n');
							ustring_append_ascii(utext2, indent);
							x = left;
						}
						ustring_append_unichar(utext2, uni);
						x += 2;
					}
				}
				if (x > screen_cols - 1) {
					ustring_append_unichar(utext2, '\n');
					ustring_append_ascii(utext2, indent);
					x = left;
				}
			}

			// デバッグ用
			if (0) {
				printf("[%d] U+%04x, x = %d", i, uni, x);
				if (uni == ESCchar) {
					printf(" ESC");
				} else if (uni == '\n') {
					printf(" '\\n'");
				} else if (0x20 <= uni && uni < 0x7f) {
					printf(" '%c'", uni);
				}
				printf("\n");
			}
		}
	}

	// 出力文字コードに変換。
	string *outstr = ustring_to_utf8(utext2);
	fputs(string_get(outstr), stdout);

	ustring_free(utext);
	ustring_free(utext2);
}
