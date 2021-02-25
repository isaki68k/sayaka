/*
 * Copyright (C) 2015 Y.Sugahara (moveccr)
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

#include "sayaka.h"
#include "StringUtil.h"
#include <cstdarg>
#include <cstring>
#include <errno.h>

// sprintf() の std::string を返すような版。
std::string
string_format(const char *fmt, ...)
{
	va_list ap;
	char *buf;

	va_start(ap, fmt);
	vasprintf(&buf, fmt, ap);
	va_end(ap);
	std::string rv(buf);
	free(buf);

	return rv;
}

// s のすべての oldstr を newstr に置換した文字列を返す
std::string
string_replace(const std::string& s,
	const std::string& oldstr, const std::string& newstr)
{
	std::string rv;

	for (auto i = 0; i < s.size(); ) {
		if (strncmp(s.c_str() + i, oldstr.c_str(), oldstr.size()) == 0) {
			rv += newstr;
			i += oldstr.size();
		} else {
			rv += s[i++];
		}
	}
	return rv;
}

// s のすべての oldchar を newchar に置換する (s を書き換える)
void
string_inreplace(std::string& s, char oldchar, char newchar)
{
	for (auto pos = 0;;) {
		pos = s.find(oldchar, pos);
		if (pos == std::string::npos)
			break;
		s[pos++] = newchar;
	}
}

// s の末尾の連続する空白文字を削除する (s を書き換える)
void
string_rtrim(std::string& s)
{
	for (;;) {
		char c = s.back();
		if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
			s.pop_back();
		} else {
			break;
		}
	}
}

// s を sep で分割する。
std::vector<std::string>
Split(const std::string& s, const std::string& sep)
{
	std::vector<std::string> v;

	// s が空だと find() は使えないが、どのみちあまり意味はない
	if (s.empty()) {
		return v;
	}

	auto p = 0;
	while (p != std::string::npos) {
		std::string item;
		auto e = s.find(sep, p);
		if (e == std::string::npos) {
			item = s.substr(p);
			p = e;
		} else {
			item = s.substr(p, e - p);
			p = e + 1;
		}
		v.emplace_back(item);
	}

	return v;
}

// s を sep で最大 limit 個まで分割する。
std::vector<std::string>
Split(const std::string& s, const std::string& sep, int limit)
{
	std::vector<std::string> v;

	// s が空だと find() は使えないが、どのみちあまり意味はない
	if (s.empty()) {
		return v;
	}

	std::string item;
	auto p = 0;
	while (p != std::string::npos && --limit > 0) {
		auto e = s.find(sep, p);
		if (e == std::string::npos) {
			item = s.substr(p);
			p = e;
		} else {
			item = s.substr(p, e - p);
			p = e + 1;
		}
		v.emplace_back(item);
	}

	// 残り
	if (p != std::string::npos) {
		item = s.substr(p);
		v.emplace_back(item);
	}

	return v;
}

// 文字列 s を最初に現れた文字列 c で2分割する。
// 返される両文字列に c は含まない。
// c が見つからない時は rv.first に s、rv.second に "" を返す。
// 例えば Split2("a:", ":") と Split2("a", ":") はどちらも [ "a", "" ] を返す。
std::pair<std::string, std::string>
Split2(const std::string& s, const std::string& c)
{
	std::pair<std::string, std::string> rv;

	auto p = s.find(c);
	if (p == std::string::npos) {
		rv.first  = s;
		rv.second = "";
	} else {
		rv.first  = s.substr(0, p);
		rv.second = s.substr(p + c.length());
	}
	return rv;
}

// 文字列 s を最初に現れた文字 c で2分割する。
std::pair<std::string, std::string>
Split2(const std::string& s, char c)
{
	std::pair<std::string, std::string> rv;

	auto p = s.find(c);
	if (p == std::string::npos) {
		rv.first  = s;
		rv.second = "";
	} else {
		rv.first  = s.substr(0, p);
		rv.second = s.substr(p + 1);
	}
	return rv;
}

// 文字列 s を最初に現れた文字列 c で2分割する。
// 返される両文字列に c は含まない。
// c が見つからない時は rv.first に ""、rv.second に s を返す。つまり
// Split2FirstOption("a:", ":") は [ "a", "" ] だが
// Split2FirstOption("a", ":") は [ "", "a" ] を返す。
std::pair<std::string, std::string>
Split2FirstOption(const std::string& s, const std::string& c)
{
	std::pair<std::string, std::string> rv;

	auto p = s.find(c);
	if (p == std::string::npos) {
		rv.first  = "";
		rv.second = s;
	} else {
		rv.first  = s.substr(0, p);
		rv.second = s.substr(p + c.length());
	}
	return rv;
}

// 文字列 s を最初に現れた文字列 c で2分割する。
std::pair<std::string, std::string>
Split2FirstOption(const std::string& s, char c)
{
	std::pair<std::string, std::string> rv;

	auto p = s.find(c);
	if (p == std::string::npos) {
		rv.first  = "";
		rv.second = s;
	} else {
		rv.first  = s.substr(0, p);
		rv.second = s.substr(p + 1);
	}
	return rv;
}

// 文字列 s をなんちゃって Url エンコードする
std::string
UrlEncode(const std::string& s)
{
	std::string sb;

	// XXX '~' はエンコードしたほうがいいので後で取り除くこと
	for (const auto& c : s) {
		if (('0' <= c && c <= '9') ||
		    ('A' <= c && c <= 'Z') ||
		    ('a' <= c && c <= 'z') ||
		    (c == '-' || c == '_' || c == '.' || c == '~'))
		{
			sb += c;
		} else {
			sb += string_format("%%%02X", (uint8)c);
		}
	}
	return sb;
}

// 前後の空白文字を削除した文字列を返す。
std::string
Chomp(const std::string& str)
{
	int pos = 0;
	while (isspace((int)str[pos])) {
		pos++;
	}
	std::string rv = str.substr(pos);

	while (isspace((int)rv.back())) {
		rv.pop_back();
	}
	return rv;
}

// ASCII 大文字を小文字にした新たな文字列を返す。
std::string
StringToLower(const std::string& str)
{
	std::string rv = str;
	for (auto& c : rv) {
		c = tolower((int)c);
	}
	return rv;
}

// s が prefix から始まれば true を返す
// C++20 になったら string.startwith() にすること
bool
StartWith(const std::string& s, const std::string& prefix)
{
	if (s.empty() || prefix.empty())
		return false;
	return strncmp(s.c_str(), prefix.c_str(), prefix.size()) == 0;
}

// s が prefix から始まれば true を返す
bool
StartWith(const std::string& s, char prefix)
{
	if (s.empty())
		return false;
	return (s.front() == prefix);
}

// s が suffix で終わっていれば true を返す
// C++20 になったら string.endwith() にすること
bool
EndWith(const std::string& s, const std::string& suffix)
{
	if (s.empty() || suffix.empty())
		return false;
	auto strsize = s.size();
	auto sufsize = suffix.size();
	if (sufsize > strsize)
		return false;
	return strcmp(s.c_str() + strsize - sufsize, suffix.c_str()) == 0;
}

// s が prefix から始まれば true を返す
bool
EndWith(const std::string& s, char suffix)
{
	if (s.empty())
		return false;
	return (s.back() == suffix);
}

// 文字列 s を10進数符号なし32bit整数とみなして数値に変換して返す。
// 数値以外の文字が来たところで変換を終了する。
// 変換できれば pair.first にその値、pair.second は 0 を返す。
// 変換できなければ pair.first は 0、pair.second にはエラー原因を返す。
// EINVAL なら文字列が数値でない、
// ERANGE なら数値が uint32 で表現できない。
// endp が NULL でない場合、変換できれば数値の次の文字の位置を返す。
// 変換できない場合は endp は変更しない。
std::pair<uint32, int>
stou32(const char *s, char **endp)
{
	uint32_t val;
	unsigned char c;
	int i;
	int error;

	val = 0;
	error = EINVAL;	// 1桁もなければエラー

	if (__predict_false(s == NULL)) {
		return { val, error };
	}

	for (i = 0; i < 9; i++) {
		c = (unsigned char)(s[i] - '0');
		if (c > 9) {
			// 数値以外が来たのでここで終わり。桁溢れはおきていない
 done:
			if (endp != NULL) {
				*endp = const_cast<char *>(&s[i]);
			}
			return { val, error };
		}

		val = val * 10 + c;
		error = 0;	// ここで EINVAL をクリア
	}

	// 10桁目
	c = (unsigned char)(s[i] - '0');
	if (c > 9) {
		// 数値以外が来たのでここで終わり。桁溢れはおきていない
		goto done;
	}
	i++;

	// UINT_MAX は 4294967295。

	if (__predict_false(val > UINT_MAX / 10)) {
		val = 0;
		error = ERANGE;
		goto done;
	}

	val *= 10;
	if (__predict_false(val == (UINT_MAX / 10) * 10) && c > 5) {
		val = 0;
		error = ERANGE;
		goto done;
	}

	val += c;

	// 11桁目があればエラー
	c = s[i];
	if ('0' <= s[i] && s[i] <= '9') {
		val = 0;
		error = ERANGE;
		goto done;
	}
	// 11桁目が数値以外なら正常終了
	goto done;
}

#if defined(SELFTEST)
#include "test.h"
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

	std::vector<std::tuple<std::string, int, uint32, int>> table = {
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
}
#endif
