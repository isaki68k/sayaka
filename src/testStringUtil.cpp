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
#include "StringUtil.h"
#include <tuple>

void
test_string_replace()
{
	printf("%s\n", __func__);

	std::vector<std::array<std::string, 4>> table = {
		// input	old		new		expected
		{ "",		"o",	"n",	"" },
		{ "abc",	"a",	"nn",	"nnbc" },
		{ "abc",	"b",	"nn",	"annc" },
		{ "abc",	"c",	"nn",	"abnn" },
		{ "ababc",	"ab",	"n",	"nnc" },
		{ "cabab",	"ab",	"n",	"cnn" },
		{ "abab",	"ab",	"n",	"nn" },
		{ "abcbcd",	"bc",	"",		"ad" },
	};
	for (const auto& a : table) {
		auto input = a[0];
		const auto& oldstr = a[1];
		const auto& newstr = a[2];
		const auto& exp = a[3];

		auto actual = string_replace(input, oldstr, newstr);
		xp_eq(exp, actual, input + ",/" + oldstr + "/" + newstr + "/");
	}
}

void
test_string_inreplace()
{
	printf("%s\n", __func__);

	std::vector<std::tuple<std::string, char, char, std::string>> table = {
		{ "abaca",	'a',	'x',	"xbxcx" },
		{ "",		'a',	'x',	"" },
		{ "abaca",	'a',	'a',	"abaca" },	// old/new が同じ
	};
	for (const auto& a : table) {
		std::string input = std::get<0>(a);
		char oldchar = std::get<1>(a);
		char newchar = std::get<2>(a);
		const auto& expected = std::get<3>(a);
		std::string where = input + "," + oldchar + "," + newchar;

		string_inreplace(input, oldchar, newchar);
		xp_eq(expected, input, where);
	}
}

void
test_string_rtrim()
{
	printf("%s\n", __func__);

	std::vector<std::array<std::string, 2>> table = {
		// input		expected
		{ "ab c",		"ab c" },
		{ "ab c \n",	"ab c" },
		{ "a\t \r \n",	"a" },
		{ "\r\n",		"" },
		{ "",			"" },
	};
	for (const auto& a : table) {
		auto input = a[0];
		const auto& exp = a[1];

		string_rtrim(input);
		xp_eq(exp, input, input);
	}
}

void
test_Split()
{
	printf("%s\n", __func__);

	std::vector<std::vector<std::string>> table = {
		// input	sep		expected...
		{ "",		":",	},
		{ "ab",		":",	"ab" },
		{ "ab:",	":",	"ab", "" },
		{ "ab:cd",	":",	"ab", "cd" },
		{ "a:b:c",	":",	"a", "b", "c" },
		// XXX セパレータが連続する場合に空要素とするか無視するかはある。
		// セパレータが空白なら空要素を取り出さないほうが自然だし。
		{ "a::b:",	":",	"a", "", "b", "" },
	};
	for (auto& a : table) {
		const auto input = a[0];
		const auto sep = a[1];
		a.erase(a.begin());
		a.erase(a.begin());
		const auto& expected = a;

		auto actual = Split(input, sep);
		if (expected.size() == actual.size()) {
			// 中身を順に比較
			for (int i = 0; i < actual.size(); i++) {
				xp_eq(expected[i], actual[i], input);
			}
		} else {
			// 個数が違う
			xp_eq(expected.size(), actual.size(), input);
		}
	}
}

void
test_Split_limit()
{
	printf("%s\n", __func__);

	struct entry {
		std::string input;
		std::string sep;
		int limit;
		std::vector<std::string> exp;
	};
	std::vector<entry> table = {
		{ "ab:cd",	":",	0,	{ "ab:cd" } },	// 0は仕方ないので1扱い
		{ "ab:cd",	":",	1,	{ "ab:cd" } },
		{ "ab:cd",	":",	2,	{ "ab", "cd" } },
		{ "ab:cd",	":",	3,	{ "ab", "cd" } },
		{ "ab:cd",	":",	4,	{ "ab", "cd" } },
		{ "a:b:c:",	":",	1,	{ "a:b:c:" } },
		{ "a:b:c:",	":",	2,	{ "a", "b:c:" } },
		{ "a:b:c:",	":",	3,	{ "a", "b", "c:" } },
		{ "a:b:c:",	":",	4,	{ "a", "b", "c", "" } },
		{ "ab",		"::",	2,	{ "ab" } },
	};
	for (const auto& a : table) {
		const auto& input = a.input;
		const auto& sep = a.sep;
		int limit = a.limit;
		const auto& exp = a.exp;
		std::string where = input + "," + sep;

		auto act = Split(input, sep, limit);
		if (exp.size() == act.size()) {
			for (int i = 0; i < exp.size(); i++) {
				xp_eq(exp[i], act[i], where);
			}
		} else {
			xp_eq(exp.size(), act.size(), where);
		}
	}
}

void
test_Split2()
{
	printf("%s\n", __func__);

	std::vector<std::array<std::string, 6>> table = {
		// input	sep,	Split2()		Split2FirstOption()
		{ "ab:cd",	":",	"ab",	"cd",	"ab",	"cd" },
		{ "ab::cd",	":",	"ab",	":cd",	"ab",	":cd" },
		{ "ab::cd",	"::",	"ab",	"cd",	"ab",	"cd" },
		{ "ab:c:",	":",	"ab",	"c:",	"ab",	"c:" },
		{ "ab",		":",	"ab",	"",		"",		"ab" },
		{ "ab",		"::",	"ab",	"",		"",		"ab" },
	};

	for (const auto& a : table) {
		auto input = a[0];
		auto sep   = a[1];
		auto exps1 = a[2];
		auto exps2 = a[3];
		auto expf1 = a[4];
		auto expf2 = a[5];

		std::pair<std::string, std::string> act;
		std::string where = input + "," + sep;

		// Split2(std::string)
		act = Split2(input, sep);
		xp_eq(exps1, act.first,  where);
		xp_eq(exps2, act.second, where);

		// Split2(char)
		if (sep.length() == 1) {
			act = Split2(input, sep[0]);
			xp_eq(exps1, act.first,  where);
			xp_eq(exps2, act.second, where);
		}

		// Split2FirstOption(std::string)
		act = Split2FirstOption(input, sep);
		xp_eq(expf1, act.first,  where);
		xp_eq(expf2, act.second, where);

		// Split2FirstOption(char)
		if (sep.length() == 1) {
			act = Split2FirstOption(input, sep[0]);
			xp_eq(expf1, act.first,  where);
			xp_eq(expf2, act.second, where);
		}
	}
}

void
test_UrlEncode()
{
	printf("%s\n", __func__);

	std::vector<std::array<std::string, 2>> table = {
		{ "",	"" },
		{ "\x1\x2\x3\x4\x5\x6\x7",		"%01%02%03%04%05%06%07" },
		{ "\x8\x9\xa\xb\xc\xd\xe\xf",	"%08%09%0A%0B%0C%0D%0E%0F" },
		{ "\x10\x11\x12\x13\x14\x15\x16\x17",	"%10%11%12%13%14%15%16%17" },
		{ "\x18\x19\x1a\x1b\x1c\x1d\x1e\x1f",	"%18%19%1A%1B%1C%1D%1E%1F" },
		{ " !\"#$%&'()*+,-./",	"%20%21%22%23%24%25%26%27%28%29%2A%2B%2C-.%2F"},
		{ "0123456789:;<=>?",	"0123456789%3A%3B%3C%3D%3E%3F" },
		{ "@ABCDEFGHIJKLMNO",	"%40ABCDEFGHIJKLMNO" },
		{ "PQRSTUVWXYZ[\\]^_",	"PQRSTUVWXYZ%5B%5C%5D%5E_" },
		{ "`abcdefghijklmno",	"%60abcdefghijklmno" },
		{ "pqrstuvwxyz{|}~",	"pqrstuvwxyz%7B%7C%7D~"}, // XXX %7E" },
		{ "\x80\xff",			"%80%FF" },
	};
	for (const auto& a : table) {
		const auto& src = a[0];
		const auto& exp = a[1];

		xp_eq(exp, UrlEncode(src), exp);
	}
}

void
test_Chomp()
{
	printf("%s\n", __func__);

	std::vector<std::pair<std::string, std::string>> table = {
		{ "",			"" },
		{ "abc",		"abc" },
		{ " abc",		"abc" },
		{ "  abc",		"abc" },
		{ "a ",			"a" },
		{ "a  ",		"a" },
		{ "  ab  ",		"ab" },
		{ "\n ab\t \n",	"ab" },
	};
	for (const auto& a : table) {
		auto input = a.first;
		auto expected = a.second;

		auto actual = Chomp(input);
		xp_eq(expected, actual, input);
	}
}

void
test_StringToLower()
{
	printf("%s\n", __func__);

	std::vector<std::pair<std::string, std::string>> table = {
		{ "",			"" },
		{ "ABC12[]",	"abc12[]" },
		{ "abc12{}",	"abc12{}" },
	};
	for (const auto& a : table) {
		auto input = a.first;
		auto expected = a.second;

		auto actual = StringToLower(input);
		xp_eq(expected, actual, input);
	}
}

void
test_StartWith()
{
	std::vector<std::tuple<std::string, std::string, bool>> table = {
		{ "abc",	"",		false },
		{ "abc",	"a",	true },
		{ "abc",	"abc",	true },
		{ "abc",	"abcd",	false },
		{ "abc",	"bc",	false },
		{ "",		"",		false },
		{ "",		"a",	false },
		{ "abc",	"ABC",	false },
		{ "abc",	"x",	false },
	};
	for (const auto& a : table) {
		const std::string& s = std::get<0>(a);
		const std::string& x = std::get<1>(a);
		const bool exp = std::get<2>(a);

		bool act = StartWith(s, x);
		xp_eq(exp, act, s + "," + x);

		// 1文字なら char 版もテストする
		if (x.size() == 1) {
			act = StartWith(s, x[0]);
			xp_eq(exp, act, s + ",'" + x + "'");
		}
	}
}

void
test_EndWith()
{
	std::vector<std::tuple<std::string, std::string, bool>> table = {
		{ "abc",	"",		false },
		{ "abc",	"c",	true },
		{ "abc",	"bc",	true },
		{ "abc",	"abc",	true },
		{ "abc",	"xabc",	false },
		{ "abc",	"ab",	false },
		{ "",		"",		false },
		{ "",		"a",	false },
		{ "abc",	"ABC",	false },
		{ "abc",	"x",	false },
	};
	for (const auto& a : table) {
		const std::string& s = std::get<0>(a);
		const std::string& x = std::get<1>(a);
		const bool exp = std::get<2>(a);

		bool act = EndWith(s, x);
		xp_eq(exp, act, s + "," + x);

		// 1文字なら char 版もテストする
		if (x.size() == 1) {
			act = EndWith(s, x[0]);
			xp_eq(exp, act, s + ",'" + x + "'");
		}
	}
}

void
test_stou32()
{
	printf("%s\n", __func__);

	std::vector<std::tuple<std::string, uint32, int, int>> table = {
		// input		val			error	endoffset
		{ "0",			0,			0,		1 },
		{ "9",			9,			0,		1 },
		{ "12",			12,			0,		2 },
		{ "429496729",	429496729,	0,		9 },	// MAXの一桁少ないやつ
		{ "429496730",	429496730,	0,		9 },
		{ "4294967289",	4294967289, 0,		10 },	// MAX近く
		{ "4294967295",	4294967295, 0,		10 },	// MAX
		{ "4294967296",	0,			ERANGE,	-1 },	// 範囲外
		{ "42949672950",0,			ERANGE,	-1 },	// MAX より一桁多い
		{ "4294967295a",4294967295,	0,		10 },	// 正常
		{ "",			0,			EINVAL,	-1 },	// 空
		{ "-1",			0,			EINVAL,	-1 },	// 負数
		{ "-2147483648",0,			EINVAL,	-1 },	// 負数(INT_MIN)
		{ "-2147483649",0,			EINVAL,	-1 },	// 負数(INT_MIN外)
		{ "-4294967295",0,			EINVAL,	-1 },	// 負数(-UINT_MAX)
		{ "1.9",		1,			0,		1 },	// 整数以外は無視
		{ "00000000009",9,			0,		11 },	// 先頭のゼロを8進数にしない
	};
	for (const auto& a : table) {
		const auto& src = std::get<0>(a);
		const auto expval = std::get<1>(a);
		const auto experr = std::get<2>(a);
		const auto expend = std::get<3>(a);

		char *actend = const_cast<char *>(src.data()) - 1;
		auto [actval, acterr] = stou32(src.c_str(), &actend);
		xp_eq(expval, actval, src);
		xp_eq(experr, acterr, src);
		xp_eq(expend, actend - src.data(), src);
	}

	// NULL
	{
		char *actend;
		auto [actval, acterr] = stou32(NULL, &actend);
		xp_eq(0, actval);
		xp_eq(EINVAL, acterr);
		// actend どうするか
	}
}

void
test_stou64()
{
	printf("%s\n", __func__);

	std::vector<std::tuple<std::string, uint64, int, int>> table = {
	 // input					val		error	endoffset
	 { "0",						0,		0,		1 },
	 { "9",						9,		0,		1 },
	 { "12",					12,		0,		2 },
	 { "4294967289",			4294967289,	0,	10 },	// 32MAX近く
	 { "4294967295",			4294967295,	0,	10 },	// 32MAX
	 { "4294967296",			4294967296,	0,	10 },	// 32bitの範囲外
	 { "1844674407370955161",	1844674407370955161,  0, 19 }, // 1桁小
	 { "18446744073709551615",	18446744073709551615ULL, 0, 20 }, // U64MAX
	 { "18446744073709551616",	0,		ERANGE,	-1 },	// 範囲外
	 { "18446744073709551615a",	18446744073709551615ULL, 0, 20 }, // 正常
	 { "",						0,		EINVAL,	-1 },	// 空
	 { "-1",					0,		EINVAL,	-1 },	// 負数
	 { "-2147483648",			0,		EINVAL,	-1 },	// 負数(INT_MIN)
	 { "-2147483649",			0,		EINVAL,	-1 },	// 負数(INT_MIN外)
	 { "-4294967295",			0,		EINVAL,	-1 },	// 負数(-UINT_MAX)
	 { "-9223372036854775808",	0,		EINVAL,	-1 },	// 負数(INT64_MIN)
	 { "-9223372036854775809",	0,		EINVAL,	-1 },	// 負数(INT64_MIN外)
	 { "-18446744073709551615",	0,		EINVAL, -1 },	// 負数(-UINT64_MAX)
	 { "1.9",					1,		0,		1 },	// 整数以外は無視
	 { "000000000000000000009",	9,		0,		21 },	// 8進数にしない
	};
	for (const auto& a : table) {
		const auto& src = std::get<0>(a);
		const auto expval = std::get<1>(a);
		const auto experr = std::get<2>(a);
		const auto expend = std::get<3>(a);

		char *actend = const_cast<char *>(src.data()) - 1;
		auto [actval, acterr] = stou64(src.c_str(), &actend);
		xp_eq(expval, actval, src);
		xp_eq(experr, acterr, src);
		xp_eq(expend, actend - src.data(), src);
	}

	// NULL
	{
		char *actend;
		auto [actval, acterr] = stou64(NULL, &actend);
		xp_eq(0, actval);
		xp_eq(EINVAL, acterr);
		// actend どうするか
	}
}

void
test_stox32()
{
	printf("%s\n", __func__);

	std::vector<std::tuple<std::string, uint32, int, int>> table = {
		// input		val			error	endoffset
		{ "0",			0,			0,		1 },
		{ "9",			0x9,		0,		1 },
		{ "F",			0xf,		0,		1 },
		{ "f",			0xf,		0,		1 },
		{ "1f",			0x1f,		0,		2 },
		{ "fffffff",	0x0fffffff,	0,		7 },	// 一桁少ない
		{ "ffffffff",	0xffffffff,	0,		8 },	// UINT32_MAX
		{ "fffffffff",	0,			ERANGE,	-1 },	// 一桁多い
		{ "ffffffffg",	0xffffffff,	0,		8 },	// これは正常...
		{ "",			0,			EINVAL,	-1 },	// 空
		{ "-1",			0,			EINVAL,	-1 },	// 負数
		{ "0xff",		0,			0,		1 },	// 正常な 0 で終わる...
		{ "1.9",		1,			0,		1 },	// 整数以外は無視
		{ "00000000009",9,			0,		11 },	// 先頭のゼロを8進数にしない
	};
	for (const auto& a : table) {
		const auto& src = std::get<0>(a);
		const auto expval = std::get<1>(a);
		const auto experr = std::get<2>(a);
		const auto expend = std::get<3>(a);

		char *actend = const_cast<char *>(src.data()) - 1;
		auto [actval, acterr] = stox32(src.c_str(), &actend);
		xp_eq(expval, actval, src);
		xp_eq(experr, acterr, src);
		xp_eq(expend, actend - src.data(), src);
	}

	// NULL
	{
		char *actend;
		auto [actval, acterr] = stox32(NULL, &actend);
		xp_eq(0, actval);
		xp_eq(EINVAL, acterr);
		// actend どうするか
	}
}

void
test_StringUtil()
{
	test_string_replace();
	test_string_inreplace();
	test_string_rtrim();
	test_Split();
	test_Split_limit();
	test_Split2();
	test_UrlEncode();
	test_Chomp();
	test_StringToLower();
	test_StartWith();
	test_EndWith();
	test_stou32();
	test_stou64();
	test_stox32();
}
