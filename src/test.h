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

#pragma once

#include "sayaka.h"
#include <unistd.h>
#include <array>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

// テスト用に、自動的に後始末するテンポラリファイル
// tempnam(3) や mktemp(3) 使うと unsecure だと怒られるので
// 面倒だけど mkdtemp(3) を使う。
class autotemp
{
 public:
	autotemp(const std::string& name) {
		strcpy(tempname, "/tmp/sayakatest.XXXXXX");
		cdirname = mkdtemp(tempname);
		filename = std::string(cdirname) + "/" + name;
	}
	~autotemp() {
		unlink(filename.c_str());
		rmdir(cdirname);
	}

	// std::string として評価されるとファイル名を返す
	operator std::string() const {
		return filename;
	}

	const char *c_str() const {
		return filename.c_str();
	}

 private:
	char tempname[32];
	char *cdirname;
	std::string filename;
};

extern int test_count;
extern int test_fail;

// 可変長マクロは、GCC 拡張なら xp_eq(exp, act, ...) のように書けるが
// C++ では xp_eq(exp, act) と xp_eq(exp, act, msg) のように 2つか3つの
// ようなのを受け取るのが難しい。ただ、どうせここを雑にしといても関数定義に
// マッチしなければエラーになるので、気にしないことにする。
#define xp_eq(...) 		xp_eq_(__FILE__, __LINE__, __func__, __VA_ARGS__)

extern void xp_eq_(const char *file, int line, const char *func,
	int exp, int act, const std::string& msg = "");
extern void xp_eq_u_(const char *file, int line, const char *func,
	uint64 exp, uint64 act, const std::string& msg = "");
extern void xp_eq_(const char *file, int line, const char *func,
	const std::string& exp, const std::string& act, const std::string& msg="");

#define xp_fail(msg) xp_fail_(__FILE__, __LINE__, __func__, msg)
extern void xp_fail_(const char *file, int line, const char *func,
	const std::string& msg);


extern void test_ChunkedInputStream();
extern void test_Diag();
extern void test_Dictionary();
extern void test_FileUtil();
extern void test_ImageReductor();
extern void test_MemoryInputStream();
extern void test_NGWord();
extern void test_OAuth();
extern void test_ParsedUri();
extern void test_RichString();
extern void test_SixelConverter();
extern void test_StringUtil();
extern void test_UString();
extern void test_eaw_code();
extern void test_subr();
extern void test_term();
