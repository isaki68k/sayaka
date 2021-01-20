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
#include <cstring>
#include <iconv.h>

// 文字列 s を(UString に変換して)末尾に追加
UString&
UString::Append(const std::string& s)
{
	UString us = StringToUString(s);
	return Append(us);
}


// 文字列 str (UTF-8) を UString (Unicode コードポイント配列) に変換する。
// Unicode コードポイントといいつつ UTF-32 なのだが (実際は別物)。
// 変換できなければ空配列を返す。
UString
StringToUString(const std::string& str)
{
	iconv_t cd;
	UString ustr;

	cd = iconv_open(UTF32_HE, "utf-8");
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

// UString (Unicode コードポイント配列) を string (UTF-8 文字列)に変換する。
// Unicode コードポイントといいつつ実際は UTF-32 なのだが。
// 変換できなければ "" を返す。
std::string
UStringToString(const UString& ustr)
{
	iconv_t cd;
	std::string str;

	cd = iconv_open("utf-8", UTF32_HE);
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
	iconv_close(cd);
	if (__predict_false(r != 0)) {
		if (r == (size_t)-1) {
			return str;
		}
		if (r > 0) {
			// 戻り値は invalid conversion の数
			// どうすべ
			errno = 0;
			return str;
		}
	}

	const char *s = (const char *)dstbuf.data();
	const char *e = (const char *)dst;
	while (s < e) {
		str += *s++;
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
		// input				expected
		{ "AB\n",				{ 0x41, 0x42, 0x0a } },
		{ "亜",					{ 0x4e9c } },
		{ "￥",					{ 0xffe5 } },	// FULLWIDTH YEN SIGN
		{ "\xf0\x9f\x98\xad",	{ 0x1f62d } },	// LOUDLY CRYING FACE
	};

	// StringToUString()
	for (const auto& a : table) {
		const auto& input = a.first;
		const auto& expected = a.second;

		auto actual = StringToUString(input);
		if (expected.size() == actual.size()) {
			for (int i = 0; i < expected.size(); i++) {
				xp_eq(expected[i], actual[i], input);
			}
		} else {
			xp_eq(expected.size(), actual.size(), input);
		}
	}

	// UStringToString()
	for (const auto& a : table) {
		const auto& expected = a.first;
		const auto& input = a.second;

		auto actual = UStringToString(input);
		xp_eq(expected, actual, expected);
	}
}

void
test_UString()
{
	test_StringToUString();
}
#endif
