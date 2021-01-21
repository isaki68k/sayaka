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

#include "UString.h"
#include <array>
#include <cstring>
#include <iconv.h>

// UTF-8 から UTF-32 への変換用。
// 運用時は ^C でとめるので解放せず放置する。
static iconv_t cd_utf8;

// UTF-32 から出力文字コードへの変換用。
// 運用時は ^C でとめるので解放せず放置する。
static iconv_t cd_out;

// 文字コードの初期化。codeset は出力文字コード名。
/*static*/ bool
UString::Init(const std::string& codeset)
{
	const char *codeset_c;

	// UTF-8 -> UTF-32 変換用
	cd_utf8 = iconv_open(UTF32_HE, "utf-8");
	if (cd_utf8 == (iconv_t)-1) {
		return false;
	}

	// 出力用
	if (codeset.empty()) {
		codeset_c = "utf-8";
	} else {
		codeset_c = codeset.c_str();
	}
	cd_out = iconv_open(codeset_c, UTF32_HE);
	if (cd_out == (iconv_t)-1) {
		return false;
	}

	return true;
}

// UTF-8 文字列 str を UString に変換する。
// Unicode コードポイントといいつつ UTF-32 なのだが (実際は別物)。
/*static*/ UString
UString::FromUTF8(const std::string& str)
{
	UString ustr;

	const char *src = str.data();
	size_t srcleft = str.size();
	std::vector<char> dstbuf(srcleft * 4);
	char *dst = dstbuf.data();
	size_t dstlen = dstbuf.size();
	size_t r = iconv(cd_utf8, &src, &srcleft, &dst, &dstlen);

	// 変換できたところまでの文字列にする
	const unichar *s = (const unichar *)dstbuf.data();
	const unichar *e = (const unichar *)dst;
	while (s < e) {
		ustr.Append(*s++);
	}

	// iconv(3) の戻り値は正数なら (変換できなかった文字をゲタに差し替えて
	// 変換した上で) 変換できなかった(差し替えた)文字数を返す。
	// またそもそも失敗した場合は -1 を返すらしいが、こっちの場合は
	// どうしたらいいか分からないのでとりあえずメッセージでも足しておくか。
	if (__predict_false(r == (size_t)-1)) {
		ustr.Append("\"iconv to utf-8 failed\"");
	}

	return ustr;
}

// この UString を文字コード codeset の std::string に変換する。
std::string
UString::ToString() const
{
	// 文字列全体を変換してみる
	const char *src = (const char *)data();
	size_t srcleft = size() * 4;		// 1文字は常に4バイト
	std::vector<char> dstbuf(srcleft);	// 足りるはず?
	char *dst = dstbuf.data();
	size_t dstlen = dstbuf.size();
	size_t r = iconv(cd_out, &src, &srcleft, &dst, &dstlen);

	// 変換できたところまでの文字列にする
	*dst = '\0';
	std::string str((const char *)dstbuf.data());

	// iconv(3) の戻り値は正数なら (変換出来なかった文字をゲタに差し替えて
	// 変換した上で) 変換できなかった(差し替えた)文字数を返す。
	// またそもそも失敗した場合は -1 を返すらしいが、こっちの場合は
	// どうしたらいいか分からないのでとりあえずメッセージでも足しておくか。
	if (__predict_false(r == (size_t)-1)) {
		str += "\"iconv failed\"";
	}

	return str;
}

// UTF-32 文字 uni が Init() で指定した文字コードに変換できるかどうかを返す。
// 変換できれば true、出来なければ false を返す。
/*static*/ bool
UString::IsUCharConvertible(unichar uni)
{
	union {
		unichar u;
		char b[4];
	} srcbuf;
	// UTF-32 が変換先で最大何バイトになるか分からんので適当
	std::array<char, 6> dstbuf;

	srcbuf.u = uni;
	const char *src = &srcbuf.b[0];
	size_t srcleft = sizeof(srcbuf);

	char *dst = dstbuf.data();
	size_t dstlen = dstbuf.size();

	auto r = iconv(cd_out, &src, &srcleft, &dst, &dstlen);
	if (__predict_false(r != 0)) {
		// 変換できない(>0)でもエラー(<0)でもとりあえず false で帰る
		return false;
	}
	return true;
}

// *srcp から srclen バイトの UTF-8 文字を unichar に変換する。
/*static*/ unichar
UString::UCharFromUTF8(const char **srcp, size_t srclen)
{
	const char *src = *srcp;
	union {
		unichar u;
		char b[4];
	} dstbuf;
	char *dst = &dstbuf.b[0];
	size_t dstlen = sizeof(dstbuf);

	auto r = iconv(cd_utf8, &src, &srclen, &dst, &dstlen);
	if (__predict_true(r == 0)) {
		// 成功なら進んだ src を書き戻して取得した unichar を返す
		*srcp = src;
		return dstbuf.u;
	} else {
		// 変換できないとすれば元の UTF-8 バイトストリームが壊れていると
		// 思われるので、わりと出来ることはない気がするが、とりあえず
		// 適当に先へ進むことにしてみる。戻り値はたぶんもはや意味ない。
		*srcp = *srcp + 1;
		return '?';
	}
}

std::string
UString::dump() const
{
	std::string str;

	for (int i = 0, end = size(); i < end; i++) {
		const auto c = (*this)[i];

		str += string_format("[%d] U+%04x", i, c);
		if (c < 0x80) {
			if (c == 0x1b) {
				str += " ESC";
			} else if (c == 0x0a) {
				str += " \\n";
			} else {
				str.push_back(' ');
				str.push_back('\'');
				str.push_back(c);
				str.push_back('\'');
			}
		}
		str += "\n";
	}
	return str;
}

#if defined(SELFTEST)
#include "test.h"

void
test_FromUTF8()
{
	printf("%s\n", __func__);

	std::vector<std::pair<std::string, UString>> table = {
		// input				expected

		{ "AB\n",				{ 0x41, 0x42, 0x0a } },
		{ "亜",					{ 0x4e9c } },
		{ "￥",					{ 0xffe5 } },	// FULLWIDTH YEN SIGN
		{ "\xf0\x9f\x98\xad",	{ 0x1f62d } },	// LOUDLY CRYING FACE
		// UTF-8 -> UTF-32 の不正シーケンスいる?
	};

	UString::Init("");

	for (const auto& a : table) {
		const auto& input = a.first;
		const auto& expected = a.second;

		auto actual = UString::FromUTF8(input);
		if (expected.size() == actual.size()) {
			for (int i = 0; i < expected.size(); i++) {
				xp_eq(expected[i], actual[i], input);
			}
		} else {
			xp_eq(expected.size(), actual.size(), input);
		}
	}

	// 実運用系ではこれらを解放しないので、テストでは手動で解放しておく
	iconv_close(cd_utf8);
	iconv_close(cd_out);
}

void
test_ToString()
{
	printf("%s\n", __func__);

	std::vector<std::pair<UString, std::string>> table = {
		// input				[input_encoding],expected

		// --- UTF-8 への変換 ---
		{ { 0x41, 0x42, 0x0a },	",AB\n" },
		{ { 0x4e9c },			",亜" },
		{ { 0xffe5 },			",￥" },				// FULLWIDTH YEN SIGN
		{ { 0x1f62d },			",\xf0\x9f\x98\xad" },	// LOUDLY CRYING FACE
		// UTF-32 -> UTF-8 の不正シーケンスいる?

		// --- euc-jp への変換 ---

		// 亜
		{ { 0x4e9c },					"euc-jp,\xb0\xa1" },

		// "あいA"
		{ { 0x3042, 0x3044, 0x41 },		"euc-jp,\xa4\xa2\xa4\xa4" "A" },

		// ￥ (FULLWIDTH YEN SIGN)
		{ { 0xffe5 },					"euc-jp,\xa1\xef" },

		//  "あ" 'LOUDLY CRYING FACE' "あ"
		{ { 0x3042, 0x1f62d, 0x3042 },	"euc-jp,\xa4\xa2\xa2\xae\xa4\xa2" },
	};

	int n = 0;
	for (const auto& a : table) {
		const auto& input = a.first;
		const auto& enc_exp = a.second;
		auto [ enc, expected] = Split2(enc_exp, ',');
		auto where = string_format("[%d] %s", n++, enc_exp.c_str());

		bool init = UString::Init(enc);
		xp_eq(true, init, where);
		auto actual = input.ToString();
		xp_eq(expected, actual, where);

		// 実運用系ではこれらを解放しないので、テストでは手動で解放しておく
		iconv_close(cd_utf8);
		iconv_close(cd_out);
	}
}

void
test_UString()
{
	test_FromUTF8();
	test_ToString();
}
#endif
