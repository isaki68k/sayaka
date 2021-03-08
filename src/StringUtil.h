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
#pragma once

#include "sayaka.h"
#include <string>
#include <utility>
#include <vector>

extern std::string string_format(const char *fmt, ...) __printflike(1, 2);

// s のすべての old を new に置換した文字列を返す。
// (std::string::replace() は指定位置の文字を置き換えるタイプなので違う)
extern std::string string_replace(const std::string& s,
	const std::string& o, const std::string& n);
// s のすべての oldchar を newchar に置換する (s を書き換える)
extern void string_inreplace(std::string& s, char o, char n);

// s の末尾の連続する空白文字を削除する (s を書き換える)
extern void string_rtrim(std::string& s);

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

// 今の所テストからしか呼んでいないがどうしたもんか
extern std::pair<uint32, int> stou32(const char *s, char **endp = NULL);
extern std::pair<uint64, int> stou64(const char *s, char **endp = NULL);
extern std::pair<uint32, int> stox32(const char *s, char **endp = NULL);

// s を10進数符号なし整数とみなして、数値に変換して返す
extern uint32 stou32def(const char *s, uint32 def, char **endp = NULL);
extern uint64 stou64def(const char *s, uint64 def, char **endp = NULL);
// s を16進整数とみなして、数値に変換して返す
extern uint32 stox32def(const char *s, uint32 def, char **endp = NULL);

static inline uint32
stou32def(const std::string& s, uint32 def, char **endp = NULL)
{
	return stou32def(s.c_str(), def, endp);
}
static inline uint64
stou64def(const std::string& s, uint64 def, char **endp = NULL)
{
	return stou64def(s.c_str(), def, endp);
}
static inline uint32
stox32def(const std::string& s, uint32 def, char **endp = NULL)
{
	return stox32def(s.c_str(), def, endp);
}
