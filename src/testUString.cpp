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
#include "UString.h"
#include "term.h"
#include <map>

class TestUString : public UString
{
 public:
	static iconv_t *GetHandle() { return &cd; }
};

// 実運用系では cd を解放しないので、テストでは手動で解放しておく
static void
test_iconv_close()
{
#if defined(HAVE_ICONV)
	iconv_t& cd = *TestUString::GetHandle();
	if (cd != (iconv_t)-1) {
		iconv_close(cd);
		cd = (iconv_t)-1;
	}
#endif
}

// テスト表示用に文字列をクォートする
static std::string
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

// test_Init() 以外にも後でポカ避けにも使う
static std::map<std::string, bool> table_Init = {
	// encoding		expected
#if defined(HAVE_ICONV)
	{ "",				true },
	{ "euc-jp",			true },
	{ "iso-2022-jp",	true },
#else
	{ "",				true },		// UTF-8 へは変換可能
	{ "euc-jp",			false },
	{ "iso-2022-jp",	false },
#endif
};

static void
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

static void
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

static void
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

		// 'LOUDLY CRYING FACE' のみ
		{ { 0x1f62d },					"euc-jp,\xa2\xae" },
#else
		// iconv サポートがない時にこの変換は起きないはずなのでこれでいい
		// (e4 ba 9c は U+4e9c の UTF-8 表現)
		{ { 0x4e9c },					"euc-jp,\xe4\xba\x9c" },
#endif

		// --- jis への変換 ---
		// 日本語で終わる文字列ではASCIIに戻すエスケープは出力されないっぽい。
#if defined(HAVE_ICONV)
		// 亜
		{ { 0x4e9c },					"iso-2022-jp,\x1b$B0!" },

		// "あい"
		{ { 0x3042, 0x3044 },			"iso-2022-jp,\x1b$B$\"$$" },

		// "あいA"
		{ { 0x3042, 0x3044, 0x41 },		"iso-2022-jp,\x1b$B$\"$$\x1b(BA" },

		// ￥ (FULLWIDTH YEN SIGN)
		{ { 0xffe5 },					"iso-2022-jp,\x1b$B!o" },

		//  "あ" 'LOUDLY CRYING FACE' "あ"
		{ { 0x3042, 0x1f62d, 0x3042 },	"iso-2022-jp,\x1b$B$\"\".$\"" },

		// 'LOUDLY CRYING FACE' のみ
		{ { 0x1f62d },					"iso-2022-jp,\x1b$B\"." },
#else
		// iconv サポートがない時にこの変換は起きないはずなのでこれでいい
		// (e4 ba 9c は U+4e9c の UTF-8 表現)
		{ { 0x4e9c },					"iso-2022-jp,\xe4\xba\x9c" },
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

static void
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

		// 変換先が jis の場合
		UString::Init("iso-2022-jp");
		act = UString::IsUCharConvertible(uni);
		xp_eq(exp, act, where);
		test_iconv_close();

		// 変換先がない場合は呼ばれないはずなので false でいい
		UString::Init("");
		act = UString::IsUCharConvertible(uni);
		xp_eq(false, act, where);
		test_iconv_close();
	}
}

static std::vector<std::pair<unichar, std::vector<uint8>>> table_UCharToUTF8 = {
	// code		expected_bytes
	{ 0x0041,	{ 0x41 } },						// 'A'
	{ 0x07b0,	{ 0xde, 0xb0 } },				// THAANA SUKUN
	{ 0xffe5,	{ 0xef, 0xbf, 0xa5 } },			// FULLWIDTH YEN SIGN
	{ 0x10280,	{ 0xf0, 0x90, 0x8a, 0x80 } },	// LYCIAN LETTER A
};
static void
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

static void
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
