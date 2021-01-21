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

static std::string UStringToStringFallback(iconv_t cd, const UString& str);
static std::string UCharToString(iconv_t cd, const char *src);

// 文字列 s を(UString に変換して)末尾に追加
UString&
UString::Append(const std::string& s)
{
	UString us = StringToUString(s);
	return Append(us);
}

// 文字列 str を UString (Unicode コードポイント配列) に変換する。
// str の文字コードは from で指定する (省略不可)。
// Unicode コードポイントといいつつ UTF-32 なのだが (実際は別物)。
// 変換できなければ空配列を返す。
UString
StringToUString(const std::string& str, const std::string& from)
{
	iconv_t cd;
	UString ustr;

	cd = iconv_open(UTF32_HE, from.c_str());
	if (__predict_false(cd == (iconv_t)-1)) {
		return ustr;
	}

	size_t srcleft = str.size();
	std::vector<char> srcbuf(srcleft + 1);
	std::vector<char> dstbuf(srcleft * 4 + 1);
	memcpy(srcbuf.data(), str.c_str(), srcbuf.size());
	const char *src = srcbuf.data();
	char *dst = dstbuf.data();
	size_t dstlen = dstbuf.size();
	size_t r = iconv(cd, &src, &srcleft, &dst, &dstlen);
	iconv_close(cd);
	if (__predict_false(r != 0)) {
		if (r == (size_t)-1) {
			return ustr;
		}
		if (r > 0) {
			// 戻り値は invalid conversion の数
			// どうすべ
			errno = 0;
			return ustr;
		}
	}

	// デバッグ用
	if (0) {
		printf("src=+%x srcleft=%d->%d dst=+%x dstlen=%d:",
			(int)(src-srcbuf.data()),
			(int)str.size(),
			(int)srcleft,
			(int)(dst-dstbuf.data()),
			(int)dstlen);
		for (int i = 0; i < (dst - dstbuf.data()); i++) {
			printf(" %02x", (unsigned char)dstbuf[i]);
		}
		printf("\n");
	}

	const uint32_t *s = (const uint32_t *)dstbuf.data();
	const uint32_t *e = (const uint32_t *)dst;
	while (s < e) {
		ustr.Append(*s++);
	}
	return ustr;
}

// UString (Unicode コードポイント配列) を std::string に変換する。
// 変換先の文字コードを to で指定する (省略不可)。
// Unicode コードポイントといいつつ実際は UTF-32 なのだが。
// 変換できなければ "" を返す。
std::string
UStringToString(const UString& ustr, const std::string& to)
{
	iconv_t cd;
	std::string str;

	cd = iconv_open(to.c_str(), UTF32_HE);
	if (__predict_false(cd == (iconv_t)-1)) {
		return str;
	}

	size_t srcleft = ustr.size() * 4;
	std::vector<char> srcbuf(srcleft);
	std::vector<char> dstbuf(srcleft);	// 足りるはず?
	memcpy(srcbuf.data(), ustr.data(), ustr.size() * 4);
	const char *src = srcbuf.data();
	char *dst = dstbuf.data();
	size_t dstlen = dstbuf.size();
	size_t r = iconv(cd, &src, &srcleft, &dst, &dstlen);
	if (__predict_true(r == 0)) {
		// 成功なら、文字列にして返す
		iconv_close(cd);
		*dst = '\0';
		return std::string((const char *)dstbuf.data());
	} else {
		// iconv() の戻り値は -1 なら失敗、>0 なら変換できなかった文字数。
		// とりあえずどちらの場合も1文字ずつフォールバックしながらやってみる
		// ことにする。
		str = UStringToStringFallback(cd, ustr);
		iconv_close(cd);
		return str;
	}
}

// Unicode コードポイント配列 str を1文字ずつ文字コード変換した文字列を返す。
// 変換できない文字は下駄(〓) に変換する (それも出来なければ '?' になるけど)。
static std::string
UStringToStringFallback(iconv_t cd, const UString& ustr)
{
	std::string str;

	for (int i = 0, end = ustr.size(); i < end; i++) {
		// i 番目の文字を変換
		std::string chr = UCharToString(cd,
			(const char *)ustr.data() + (i * 4));
		if (__predict_false(chr.empty())) {
			// 出来なければ下駄を変換
			uint32 altchr = 0x3013;	// U+3013
			chr = UCharToString(cd, (const char *)&altchr);
			if (__predict_false(chr.empty())) {
				// それでも出来なければ知らん…
				chr = "?";
			}
		}
		str += chr;
	}

	return str;
}

// src から始まる UTF-32 の1文字を変換して返す。
// 変換できなければ empty を返す。
static std::string
UCharToString(iconv_t cd, const char *src)
{
	// UTF-32 が変換先で最大何バイトになるか分からんけど。
	std::array<char, 6> dstbuf;

	size_t srcleft = 4;	// 入力1文字は常に4バイト
	char *dst = dstbuf.data();
	size_t dstlen = dstbuf.size();

	auto r = iconv(cd, &src, &srcleft, &dst, &dstlen);
	if (__predict_false(r != 0)) {
		// ここではフォールバックしない
		return "";
	}
	const char *s = (const char *)dstbuf.data();
	const char *e = (const char *)dst;
	return std::string(s, (size_t)(e - s));
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
test_StringToUString()
{
	printf("%s\n", __func__);

	std::vector<std::pair<std::string, UString>> table = {
		// [encoding],input		expected

		// --- UTF-8 からの変換 ---
		{ ",AB\n",				{ 0x41, 0x42, 0x0a } },
		{ ",亜",				{ 0x4e9c } },
		{ ",￥",				{ 0xffe5 } },	// FULLWIDTH YEN SIGN
		{ ",\xf0\x9f\x98\xad",	{ 0x1f62d } },	// LOUDLY CRYING FACE
		// UTF-8 -> UTF-32 の不正シーケンスいる?

		// --- euc-jp からの変換 ---

		// 亜
		{ "euc-jp,\xb0\xa1",				{ 0x4e9c } },

		// "あいA"
		{ "euc-jp,\xa4\xa2\xa4\xa4" "A",	{ 0x3042, 0x3044, 0x41 } },

		// ￥(FULLWIDTH YEN SIGN)
		{ "euc-jp,\xa1\xef",				{ 0xffe5 } },

		// euc-jp -> UTF-32 の不正シーケンスいる?
		// "あ" "\xff\xff" "あ"
		//{ "euc-jp,\xa4\xa2\xff\xff\xa4\xa2",{ 0x3042, 0x303c, 0x3042 } },
	};

	int n = 0;
	for (const auto& a : table) {
		const auto& enc_input = a.first;
		const auto& expected = a.second;
		auto [ enc, input ] = Split2(enc_input, ',');
		auto where = string_format("[%d] %s", n++, enc_input.c_str());

		const char *from = NULL;
		if (!enc.empty()) {
			from = enc.c_str();
		}
		auto actual = StringToUString(input, from);
		if (expected.size() == actual.size()) {
			for (int i = 0; i < expected.size(); i++) {
				xp_eq(expected[i], actual[i], where);
			}
		} else {
			xp_eq(expected.size(), actual.size(), where);
		}
	}
}

void
test_UStringToString()
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

		const char *to = NULL;
		if (!enc.empty()) {
			to = enc.c_str();
		}
		auto actual = UStringToString(input, to);
		xp_eq(expected, actual, where);
	}
}

void
test_UString()
{
	test_StringToUString();
	test_UStringToString();
}
#endif
