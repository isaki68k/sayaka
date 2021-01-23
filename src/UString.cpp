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
#define HAVE_ICONV
#if defined(HAVE_ICONV)
#include <iconv.h>
#endif

// 出力文字コードが UTF-8 以外 (iconv による変換が必要) なら true。
bool UString::use_iconv = false;

#if defined(HAVE_ICONV)
// UTF-32 から UTF-8 以外の出力文字コードへの変換用。
// 運用時は ^C でとめるので解放せず放置する。
// 本来は UString の static メンバにすべきだがヘッダに持ち出さないため。
static iconv_t cd;
#endif

// 文字コードの初期化。codeset は出力文字コード名。
// 失敗すれば errno をセットし false を返す。
/*static*/ bool
UString::Init(const std::string& codeset)
{
	use_iconv = false;

	if (codeset.empty()) {
		// UTF-8 なら iconv 不要。
		return true;
	} else {
#if defined(HAVE_ICONV)
		// UTF-8 以外なら iconv を使う。
		cd = iconv_open(codeset.c_str(), "utf-8");
		if (cd == (iconv_t)-1) {
			return false;
		}
		use_iconv = true;
		return true;
#else
		// UTF-8 以外が指定されたのに iconv がなければエラー。
		// (iconv_open() のエラーと区別つけるため errno = 0 にする)
		errno = 0;
		return false;
#endif
	}
}

// UTF-8 文字列 str を UString に変換する。
/*static*/ UString
UString::FromUTF8(const std::string& str)
{
	UString ustr;

	const char *s = str.c_str();
	for (int i = 0, end = str.size(); i < end; ) {
		auto [ code, len ] = UCharFromUTF8(s + i);

		ustr.Append(code);
		i += len;
	}

	return ustr;
}

// この UString を Init() で指定した文字コードの std::string に変換する。
std::string
UString::ToString() const
{
	std::vector<char> dst(size() * 4 + 1);

	// まず1文字ずつ UTF-8 文字列に変換する
	int offset = 0;
	for (const auto code : *this) {
		int len = UCharToUTF8(&dst[offset], code);
		offset += len;
	}
	dst[offset] = '\0';

	std::string utf8((const char *)dst.data());

#if defined(HAVE_ICONV)
	if (use_iconv) {
		return UTF8ToOutCode(utf8);
	}
#endif
	return utf8;
}

#if defined(HAVE_ICONV)
// UTF-8 文字列 utf8 を Init() で設定した出力文字コードに変換して返す。
/*static*/ std::string
UString::UTF8ToOutCode(const std::string& utf8)
{
	// 文字列全体を変換してみる
	const char *src = (const char *)utf8.data();
	size_t srcleft = utf8.size();
	std::vector<char> dstbuf(srcleft * 4 + 1);	// 適当だけど足りるはず
	char *dst = dstbuf.data();
	size_t dstlen = dstbuf.size();
	size_t r = iconv(cd, &src, &srcleft, &dst, &dstlen);

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
#endif

// UTF-32 文字 uni が Init() で指定した文字コードに変換できるかどうかを返す。
// 変換できれば true、出来なければ false を返す。
/*static*/ bool
UString::IsUCharConvertible(unichar uni)
{
#if defined(HAVE_ICONV)
	// UTF-32 の uni を UTF-8 の srcbuf に変換
	std::array<char, 4> srcbuf;
	size_t srcleft = UCharToUTF8(srcbuf.data(), uni);
	const char *src = srcbuf.data();

	std::array<char, 4> dstbuf;
	char *dst = dstbuf.data();
	size_t dstlen = dstbuf.size();

	// UTF-8 を出力文字コードに変換してみる
	auto r = iconv(cd, &src, &srcleft, &dst, &dstlen);
	if (__predict_true(r == 0)) {
		return true;
	}
	// 変換できない(>0)でもエラー(<0)でもとりあえず false で帰る
#else
	// これは起きないはずなので、これでいい
#endif
	return false;
}

// UTF-8 文字列 src から始まる1文字を Unicode コードポイントに変換する。
// 戻り値はコードポイントとこの文字のバイト数のペア。
// 変換出来なかったりしたら諦めてその時点での状態で帰る。
/*static*/ std::pair<unichar, int>
UString::UCharFromUTF8(const char *src)
{
	unichar code;
	int bytelen;
	uint8 c;

	c = *src;
	if (__predict_false(c == 0)) {
		return { 0, 1 };
	}

	// 1バイト目。UTF-8 は1バイト目でこの文字のバイト数が分かる
	if (__predict_true(c < 0x80)) {
		// 1バイト
		return { c, 1 };

	} else if (__predict_true(0xc2 <= c && c <= 0xdf)) {
		bytelen = 2;
		code = c & 0x1f;
	} else if (__predict_true(0xe0 <= c && c <= 0xef)) {
		bytelen = 3;
		code = c & 0x0f;
	} else if (__predict_true(0xf0 <= c && c <= 0xf4)) {
		bytelen = 4;
		code = c & 0x07;
	} else {
		// こないはずだけど、とりあえず
		return { c, 1 };
	}

	// 2バイト目以降。
	int pos;
	for (pos = 1; pos < bytelen && src[pos] != 0; pos++) {
		code = (code << 6) | (src[pos] & 0x3f);
	}

	// 文字の途中で '\0' になってしまったらエラーだが、
	// 正常系と同じ値でそのまま帰るくらいしか、出来ることはない。

	return { code, pos };
}

// Unicode コードポイント code を UTF-8 に変換して dst に書き出す。
// dst は '\0' 終端しない。
// 戻り値は書き出したバイト数。
/*static*/ int
UString::UCharToUTF8(char *dst, unichar code)
{
	if (code < 0x80) {
		// 1バイト
		*dst = (char)code;
		return 1;

	} else if (code < 0x7ff) {
		// 2バイト
		*dst++ = 0xc0 | (code >> 6);
		*dst++ = 0x80 | (code & 0x3f);
		return 2;

	} else if (code < 0x10000) {
		// 3バイト
		*dst++ = 0xe0 |  (code >> 12);
		*dst++ = 0x80 | ((code >> 6) & 0x3f);
		*dst++ = 0x80 |  (code & 0x3f);
		return 3;

	} else {
		// 4バイト
		*dst++ = 0xf0 |  (code >> 18);
		*dst++ = 0x80 | ((code >> 12) & 0x3f);
		*dst++ = 0x80 | ((code >>  6) & 0x3f);
		*dst++ = 0x80 |  (code & 0x3f);
		return 4;
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
#include <map>
#include "test.h"
#include "term.h"

// テスト表示用に文字列をクォートする
std::string
quote(const std::string& s)
{
	std::string q;

	for (const auto c : s) {
		if (c == ESCchar) {
			q += "<ESC>";
		} else if (c == '\n') {
			q += "\\n";
		} else if ((signed char)c < 0x20 || c == 0x7e) {
			q += string_format("\\x%02x", (unsigned char)c);
		} else {
			q += c;
		}
	}
	return q;
}

// 実運用系では cd を解放しないので、テストでは手動で解放しておく
void
test_iconv_close()
{
#if defined(HAVE_ICONV)
	if (cd != (iconv_t)-1) {
		iconv_close(cd);
		cd = (iconv_t)-1;
	}
#endif
}

// test_Init() 以外にも後でポカ避けにも使う
std::map<std::string, bool> table_Init = {
	// encoding		expected
#if defined(HAVE_ICONV)
	{ "",			true },
	{ "euc-jp",		true },
#else
	{ "",			true },		// UTF-8 へは変換可能
	{ "euc-jp",		false },
#endif
};

void
test_Init()
{
	printf("%s\n", __func__);

	for (const auto& a : table_Init) {
		const auto& enc = a.first;
		const auto  exp = a.second;

		bool init = UString::Init(enc);
		xp_eq(exp, init, enc);
		test_iconv_close();
	}
}

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

	test_iconv_close();
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
#if defined(HAVE_ICONV)
		// 亜
		{ { 0x4e9c },					"euc-jp,\xb0\xa1" },

		// "あいA"
		{ { 0x3042, 0x3044, 0x41 },		"euc-jp,\xa4\xa2\xa4\xa4" "A" },

		// ￥ (FULLWIDTH YEN SIGN)
		{ { 0xffe5 },					"euc-jp,\xa1\xef" },

		//  "あ" 'LOUDLY CRYING FACE' "あ"
		{ { 0x3042, 0x1f62d, 0x3042 },	"euc-jp,\xa4\xa2\xa2\xae\xa4\xa2" },
#else
		// iconv サポートがない時にこの変換は起きないはずなのでこれでいい
		// (e4 ba 9c は U+4e9c の UTF-8 表現)
		{ { 0x4e9c },					"euc-jp,\xe4\xba\x9c" },
#endif
	};

	for (const auto& a : table) {
		const auto& input = a.first;
		const auto& enc_exp = a.second;
		auto [ enc, expected ] = Split2(enc_exp, ',');
		auto where = quote(enc_exp);

		bool init = UString::Init(enc);
		xp_eq(table_Init[enc], init, where);
		auto actual = input.ToString();

		xp_eq(quote(expected), quote(actual), where);

		test_iconv_close();
	}
}

void
test_IsUCharConvertible()
{
	printf("%s\n", __func__);

	std::vector<std::pair<unichar, bool>> table = {
		{ 0x4e9c,	true },		// U+4e9c  亜
		{ 0x1f62d,	false },	// U+1f62d LOUDLY CRYING FACE
	};

	for (const auto& a : table) {
		const auto uni = a.first;
#if defined(HAVE_ICONV)
		const auto exp = a.second;
#else
		const auto exp = false;
#endif
		auto where = string_format("U+%04x", uni);

		// 変換先が euc-jp の場合
		UString::Init("euc-jp");
		auto act = UString::IsUCharConvertible(uni);
		xp_eq(exp, act, where);
		test_iconv_close();

		// 変換先がない場合は呼ばれないはずなので false でいい
		UString::Init("");
		act = UString::IsUCharConvertible(uni);
		xp_eq(false, act, where);
		test_iconv_close();
	}
}

std::vector<std::pair<unichar, std::vector<uint8>>> table_UCharToUTF8 = {
	// code		expected_bytes
	{ 0x0041,	{ 0x41 } },						// 'A'
	{ 0x07b0,	{ 0xde, 0xb0 } },				// THAANA SUKUN
	{ 0xffe5,	{ 0xef, 0xbf, 0xa5 } },			// FULLWIDTH YEN SIGN
	{ 0x10280,	{ 0xf0, 0x90, 0x8a, 0x80 } },	// LYCIAN LETTER A
};
void
test_UCharFromUTF8()
{
	printf("%s\n", __func__);

	for (const auto& a : table_UCharToUTF8) {
		const auto& expcode = a.first;
		const auto& input = a.second;
		const auto  explen = input.size();
		auto where = string_format("U+%04x", expcode);

		char src[6];
		memset(src, 0xff, sizeof(src));
		memcpy(src, input.data(), input.size());

		auto actual = UString::UCharFromUTF8(src);
		xp_eq(expcode, actual.first, where);
		xp_eq(explen, actual.second, where);
	}
}

void
test_UCharToUTF8()
{
	printf("%s\n", __func__);

	for (const auto& a : table_UCharToUTF8) {
		const auto& code = a.first;
		const auto& expected = a.second;
		auto where = string_format("U+%04x", code);

		std::array<uint8, 5> dst;
		dst.fill(0xff);
		auto actual = UString::UCharToUTF8((char *)dst.data(), code);

		xp_eq(expected.size(), actual, where);
		int i = 0;
		for (i = 0; i < expected.size(); i++) {
			xp_eq(expected[i], dst[i], where + string_format("[%d]", i));
		}
		for (; i < sizeof(dst); i++) {
			xp_eq(0xff, dst[i], where + string_format("[%d]", i));
		}
	}
}

void
test_UString()
{
	test_Init();
	test_FromUTF8();
	test_ToString();
	test_IsUCharConvertible();
	test_UCharFromUTF8();
	test_UCharToUTF8();
}
#endif
