#pragma once

#include <string>
#include <utility>
#include <vector>

extern std::string string_format(const char *fmt, ...) __printflike(1, 2);

// s の old を new に置換した文字列を返す。
// (std::string::replace() は指定位置の文字を置き換えるタイプなので違う)
extern std::string string_replace(const std::string& s,
	const std::string& o, const std::string& n);

// s を sep で分割する
extern std::vector<std::string> Split(const std::string& s,
	const std::string& sep);
// s を sep で最大 limit 個まで分割する。
extern std::vector<std::string> Split(const std::string& s,
	const std::string& sep, int limit);

// s を最初に現れた c で2分割する (s に c が含まれなければ前詰め)
extern std::pair<std::string, std::string> Split2(
	const std::string& s, const std::string& c);
extern std::pair<std::string, std::string> Split2(
	const std::string& s, char c);

// s を最初に現れた c で2分割する (s に c が含まれなければ後ろ詰め)
extern std::pair<std::string, std::string> Split2FirstOption(
	const std::string& s, const std::string& c);
extern std::pair<std::string, std::string> Split2FirstOption(
	const std::string& s, char c);

// 文字列 s をなんちゃって Url エンコードする
extern std::string UrlEncode(const std::string& s);

// s の前後の空白文字を削除した文字列を返す
extern std::string Chomp(const std::string& s);

// s を小文字にした文字列を返す
extern std::string StringToLower(const std::string& s);

// s が prefix から始まれば true を返す
extern bool StartWith(const std::string& s, const std::string& prefix);
extern bool StartWith(const std::string& s, char prefix);
// s が suffix で終わっていれば true を返す
extern bool EndWith(const std::string& s, const std::string& suffix);
extern bool EndWith(const std::string& s, char suffix);


#if defined(SELFTEST)
extern void test_StringUtil();
#endif
