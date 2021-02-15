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

// デバッグ用診断ツール

#include "Diag.h"
#include <cstdio>
#include <cstdarg>

// コンストラクタ
Diag::Diag()
{
}

// コンストラクタ
Diag::Diag(const std::string& name_)
{
	SetClassname(name_);
}

// クラス名を後から設定する
void
Diag::SetClassname(const std::string& name_)
{
	classname = name_;
	if (!classname.empty()) {
		classname += " ";
	}
}

// デバッグレベルを lv に設定する
void
Diag::SetLevel(int lv)
{
	debuglevel = lv;
}

// メッセージ出力 (改行はこちらで付加する)
void
Diag::Print(const char *fmt, ...) const
{
	va_list ap;

	fputs(classname.c_str(), stderr);

	va_start(ap, (fmt));
	vfprintf(stderr, (fmt), ap);
	va_end(ap);

	fputs("\n", stderr);
}


// Diag とは関係ないけど、とりあえずここに寄生しておく。

// __PRETTY_FUNCTION__ と __FUNCTION__ からクラス名::関数名だけを得る
std::string
get_classfunc_name(const char *pretty_, const char *func_)
{
	std::string_view func(func_);
	std::string_view pretty(pretty_);

	auto namepos = pretty.find(func);
	auto begin = pretty.find_last_of(" *", namepos) + 1;
	auto end   = pretty.find("(", namepos + func.size());
	auto classfunc = pretty.substr(begin, end - begin);

	return std::string(classfunc);
}

#if defined(SELFTEST)
#include "test.h"
#include "StringUtil.h"
#include <memory>

// テスト用のクラス
class Test;
class Test
{
 public:
	// コンストラクタの場合
	Test() {
		name = string_format("%s", __method__);
	}
	// デストラクタの場合
	~Test() {
		name = string_format("%s", __method__);
	}

	// ノーマルな関数
	void test1() {
		name = string_format("%s", __method__);
	}
	// 紛らわしそうなやつ、ポインタを返す、同じクラスを受け取る
	int *test2(Test *h) {
		name = string_format("%s", __method__);
		return NULL;
	}
	// 関数ポインタを受け取り、関数ポインタを返す
	using func_t = int (*)();
	func_t test3(func_t a) {
		name = string_format("%s", __method__);
		return NULL;
	}

	// 結果を格納する
	static std::string name;
};

std::string Test::name;

void
test_get_classfunc_name()
{
	printf("%s\n", __func__);

	std::unique_ptr<Test> t(new Test());
	xp_eq("Test::Test", Test::name);

	t->test1();
	xp_eq("Test::test1", Test::name);

	t->test2(NULL);
	xp_eq("Test::test2", Test::name);

	t->test3(NULL);
	xp_eq("Test::test3", Test::name);

	t.reset();
	xp_eq("Test::~Test", Test::name);
}

void
test_Diag()
{
	test_get_classfunc_name();
}
#endif
