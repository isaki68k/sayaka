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
	int r;

	va_start(ap, fmt);
	r = vasprintf(&buf, fmt, ap);
	va_end(ap);
	if (__predict_false(r < 0)) {
		return "";
	}
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
	static const char rawchar[] =
		"-.0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ_abcdefghijklmnopqrstuvwxyz";

	for (const auto& c : s) {
		if (strchr(rawchar, c) != NULL) {
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

// 文字列 s を N 進数符号なし T型 整数とみなして数値に変換して返す。
// 数値以外の文字が来たところで変換を終了する。
// 変換できれば pair.first にその値、pair.second は 0 を返す。
// 変換できなければ pair.first は 0、pair.second にはエラー原因を返す。
// EINVAL なら文字列が数値でない、
// ERANGE なら数値が T型 で表現できない。
// endp が NULL でない場合、変換できれば数値の次の文字の位置を返す。
// 変換できない場合は endp は変更しない。
template <typename T, int N>
std::pair<T, int>
stouT(const char *s, char **endp)
{
	T val;
	int c;
	int error;

	val = 0;
	error = EINVAL;	// 1桁もなければエラー

	if (__predict_false(s == NULL)) {
		return { val, error };
	}

	for (; *s; s++) {
		c = *s;

		// テンプレート分岐で c を求める
		if (N == 10) {
			if (c < '0' || c > '9') {
				break;
			}
			c -= '0';
		} else if (N == 16) {
			if ('0' <= c && c <= '9') {
				c -= '0';
			} else if ('A' <= c && c <= 'F') {
				c = c - 'A' + 10;
			} else if ('a' <= c && c <= 'f') {
				c = c - 'a' + 10;
			} else {
				break;
			}
		} else {
			// ここにはこないはず
		}

		// オーバーフローチェックは演算ごとに必要
		T n1 = val * N;
		T n2 = n1 + c;
		if (n1 < val || n2 < val) {
			// overflow
			error = ERANGE;
			break;
		}
		val = n2;
		error = 0;
	}

	if (__predict_true(error == 0)) {
		if (endp) {
			*endp = const_cast<char *>(s);
		}
	} else {
		val = 0;
	}

	return { val, error };
}

// 文字列 s を10進数符号なし64bit整数とみなして数値に変換して返す。
// 数値以外の文字が来たところで変換を終了する。
// 変換できれば pair.first にその値、pair.second は 0 を返す。
// 変換できなければ pair.first は 0、pair.second にはエラー原因を返す。
// EINVAL なら文字列が数値でない、
// ERANGE なら数値が uint64 で表現できない。
// endp が NULL でない場合、変換できれば数値の次の文字の位置を返す。
// 変換できない場合は endp は変更しない。
std::pair<uint64, int>
stou64(const char *s, char **endp)
{
	return stouT<uint64, 10>(s, endp);
}

std::pair<uint32, int>
stou32(const char *s, char **endp)
{
	return stouT<uint32, 10>(s, endp);
}

std::pair<uint32, int>
stox32(const char *s, char **endp)
{
	return stouT<uint32, 16>(s, endp);
}

// エラーの代わりにデフォルト値を返す版
uint32
stou32def(const char *s, uint32 defval, char **endp)
{
	auto [val, error] = stou32(s, endp);
	if (error) {
		val = defval;
	}
	return val;
}

uint64
stou64def(const char *s, uint64 defval, char **endp)
{
	auto [val, error] = stou64(s, endp);
	if (error) {
		val = defval;
	}
	return val;
}

uint32
stox32def(const char *s, uint32 defval, char **endp)
{
	auto [val, error] = stox32(s, endp);
	if (error) {
		val = defval;
	}
	return val;
}
