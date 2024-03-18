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

#include "Print.h"
#include "MathAlphaSymbols.h"
#include "StringUtil.h"
#include "UString.h"
#include "eaw_code.h"
#include "term.h"
#include <assert.h>
#include <array>

// 色定数
static const std::string BOLD		= "1";
static const std::string UNDERSCORE	= "4";
static const std::string STRIKE		= "9";
static const std::string BLACK		= "30";
static const std::string RED		= "31";
static const std::string GREEN		= "32";
static const std::string BROWN		= "33";
static const std::string BLUE		= "34";
static const std::string MAGENTA	= "35";
static const std::string CYAN		= "36";
static const std::string WHITE		= "37";
static const std::string GRAY		= "90";
static const std::string YELLOW		= "93";

#define BG_ISDARK()		(opt_bgtheme == BG_DARK)
#define BG_ISLIGHT()	(opt_bgtheme != BG_DARK) // 姑息な最適化
enum bgtheme opt_bgtheme;		// 背景用の色タイプ
std::string output_codeset;		// 出力文字コード ("" なら UTF-8)
bool opt_nocolor;				// テキストに(色)属性を一切付けない

static std::string str_join(const std::string& sep,
	const std::string& s1, const std::string& s2);

static std::array<UString, Color::Max> color2esc;	// 色エスケープ文字列

void
init_color()
{
	std::string blue;
	std::string green;
	std::string username;
	std::string fav;
	std::string gray;
	std::string verified;

	if (color_mode == 2) {
		// 2色モードなら色は全部無効にする。
		// ユーザ名だけボールドにすると少し目立って分かりやすいか。
		username = BOLD;
	} else {
		// それ以外のケースは色ごとに個別調整。

		// 青は黒背景か白背景かで色合いを変えたほうが読みやすい
		if (BG_ISLIGHT()) {
			blue = BLUE;
		} else {
			blue = CYAN;
		}

		// ユーザ名。白地の場合は出来ればもう少し暗めにしたい
		if (BG_ISLIGHT() && color_mode > 16) {
			username = "38;5;28";
		} else {
			username = BROWN;
		}

		// リツイートは緑色。出来れば濃い目にしたい
		if (color_mode == ColorFixedX68k) {
			green = "92";
		} else if (color_mode > 16) {
			green = "38;5;28";
		} else {
			green = GREEN;
		}

		// ふぁぼは黄色。白地の場合は出来れば濃い目にしたいが
		// こちらは太字なのでユーザ名ほどオレンジにしなくてもよさげ。
		if (BG_ISLIGHT() && color_mode > 16) {
			fav = "38;5;184";
		} else {
			fav = BROWN;
		}

		// x68k 独自16色パッチでは 90 は黒、97 がグレー。
		// mlterm では 90 がグレー、97 は白。
		if (color_mode == ColorFixedX68k) {
			gray = "97";
		} else {
			gray = "90";
		}

		// 認証マークは白背景でも黒背景でもシアンでよさそう
		verified = CYAN;
	}

	color2esc[Color::Username]	= UString(username);
	color2esc[Color::UserId]	= UString(blue);
	color2esc[Color::Time]		= UString(gray);
	color2esc[Color::Source]	= UString(gray);

	color2esc[Color::Retweet]	= UString(str_join(";", BOLD, green));
	color2esc[Color::Favorite]	= UString(str_join(";", BOLD, fav));
	color2esc[Color::Url]		= UString(str_join(";", UNDERSCORE, blue));
	color2esc[Color::Tag]		= UString(blue);
	color2esc[Color::Verified]	= UString(verified);
	color2esc[Color::Protected]	= UString(gray);
	color2esc[Color::NG]		= UString(str_join(";", STRIKE, gray));
}

// 文字列 s1 と s2 を sep で結合した文字列を返す。
// ただし (glib の) string.join() と異なり、(null と)空文字列の要素は排除
// した後に結合を行う。
// XXX 今の所、引数は2つのケースしかないので手抜き。
// 例)
//   string.join(";", "AA", "") -> "AA;"
//   str_join(";", "AA", "")    -> "AA"
static std::string
str_join(const std::string& sep, const std::string& s1, const std::string& s2)
{
	if (s1.empty()) {
		return s2;
	} else if (s2.empty()) {
		return s1;
	} else {
		return s1 + sep + s2;
	}
}

// UString をインデントを付けて文字列を表示する
void
print_(const UString& src)
{
	// Stage1: Unicode 文字単位でいろいろフィルターかける。
	UString utext;
	for (const auto uni : src) {
		// Private Use Area (外字) をコードポイント形式(?)にする
		if (__predict_false((  0xe000 <= uni && uni <=   0xf8ff))	// BMP
		 || __predict_false(( 0xf0000 <= uni && uni <=  0xffffd))	// 第15面
		 || __predict_false((0x100000 <= uni && uni <= 0x10fffd))) 	// 第16面
		{
			auto tmp = string_format("<U+%X>", uni);
			utext.AppendASCII(tmp);
			continue;
		}

		if (__predict_false((0x1d400 <= uni && uni <= 0x1d7ff)) &&
		    opt_mathalpha == true)
		{
			// Mathematical Alphanumeric Symbols を全角英数字に変換
			utext.Append(ConvMathAlpha(uni));
			continue;
		}

		// --no-combine なら Combining Enclosing * (U+20DD-U+20E4) の前に
		// スペースを入れて、囲まれるはずだった文字とは独立させる。
		// 前の文字(たいていただの ASCII 数字)が潰されて読めなくなるのを
		// 防ぐため。
		// U+20E1 は「上に左右矢印を前の文字につける」で囲みではないが
		// 面倒なので混ぜておく。なぜ間に入れたのか…。
		if (__predict_false(0x20dd <= uni && uni <= 0x20e4) && opt_nocombine) {
			utext.Append(0x20);
		}

		if (__predict_false(!output_codeset.empty())) {
			// JIS/EUC-JP(/Shift-JIS) に変換する場合のマッピング
			// 本当は変換先がこれらの時だけのほうがいいだろうけど。

			// 全角チルダ(U+FF5E) -> 波ダッシュ(U+301C)
			if (uni == 0xff5e) {
				utext.Append(0x301c);
				continue;
			}

			// 全角ハイフンマイナス(U+FF0D) -> マイナス記号(U+2212)
			if (uni == 0xff0d) {
				utext.Append(0x2212);
				continue;
			}

			// BULLET (U+2022) -> 中黒(U+30FB)
			if (uni == 0x2022) {
				utext.Append(0x30fb);
				continue;
			}

			// NetBSD/x68k なら半角カナは表示できる。
			// XXX 正確には JIS という訳ではないのだがとりあえず
			if (output_codeset == "iso-2022-jp") {
				if (__predict_false(0xff61 <= uni && uni < 0xffa0)) {
					utext.AppendASCII(ESC "(I");
					utext.Append(uni - 0xff60 + 0x20);
					utext.AppendASCII(ESC "(B");
					continue;
				}
			}

			// 変換先に対応する文字がなければゲタ'〓'(U+3013)にする
			if (__predict_false(UString::IsUCharConvertible(uni) == false)) {
				utext.Append(0x3013);
				continue;
			}
		}

		utext.Append(uni);
	}

	if (0) {
		for (int i = 0; i < utext.size(); i++) {
			printf("print_[%d] %02x\n", i, utext[i]);
		}
	}

	// Stage2: インデントつけていく。
	UString utext2;
	// インデント階層
	auto left = indent_cols * (indent_depth + 1);
	auto indent = UString(string_format(CSI "%dC", left));
	utext2.Append(indent);

	if (__predict_false(screen_cols == 0)) {
		// 桁数が分からない場合は何もしない
		utext2.Append(utext);
	} else {
		// 1文字ずつ文字幅を数えながら出力用に整形していく
		int in_escape = 0;
		auto x = left;
		for (int i = 0, end = utext.size(); i < end; i++) {
			const auto uni = utext[i];
			if (__predict_false(in_escape > 0)) {
				// 1: ESC直後
				// 2: ESC [
				// 3: ESC (
				utext2.Append(uni);
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
					utext2.Append(uni);
					in_escape = 1;
				} else if (uni == '\n') {
					utext2.Append(uni);
					utext2.Append(indent);
					x = left;
				} else {
					// 文字幅を取得
					auto width = get_eaw_width(uni);
					if (width == 1) {
						utext2.Append(uni);
						x++;
					} else {
						assert(width == 2);
						if (x > screen_cols - 2) {
							utext2.Append('\n');
							utext2.Append(indent);
							x = left;
						}
						utext2.Append(uni);
						x += 2;
					}
				}
				if (x > screen_cols - 1) {
					utext2.Append('\n');
					utext2.Append(indent);
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

	// 出力文字コードに変換
	std::string outstr = utext2.ToString();
	fputs(outstr.c_str(), stdout);
}

// 属性付け開始文字列を UString で返す
UString
ColorBegin(Color col)
{
	UString esc;

	if (__predict_false(opt_nocolor)) {
		// --no-color なら一切属性を付けない
	} else {
		esc.AppendASCII(CSI);
		esc.Append(color2esc[col]);
		esc.Append('m');
	}
	return esc;
}

// 属性付け終了文字列を UString で返す
UString
ColorEnd(Color col)
{
	UString esc;

	if (__predict_false(opt_nocolor)) {
		// --no-color なら一切属性を付けない
	} else {
		esc.AppendASCII(CSI "0m");
	}
	return esc;
}

// 文字列 text を UString に変換して、色属性を付けた UString を返す
UString
coloring(const std::string& text, Color col)
{
	UString utext;

	utext += ColorBegin(col);
	utext += UString::FromUTF8(text);
	utext += ColorEnd(col);

	return utext;
}
