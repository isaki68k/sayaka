#pragma once

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
	};
	~autotemp() {
		unlink(filename.c_str());
		rmdir(cdirname);
	};

	// std::string として評価されるとファイル名を返す
	operator std::string() const {
		return filename;
	};

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
#define xp_eq(...) xp_eq_(__FILE__, __LINE__, __func__, __VA_ARGS__)

extern void xp_eq_(const char *file, int line, const char *func,
	int exp, int act, const std::string& msg = "");
extern void xp_eq_(const char *file, int line, const char *func,
	const std::string& exp, const std::string& act, const std::string& msg="");

#define xp_fail(msg) xp_fail_(__FILE__, __LINE__, __func__, msg)
extern void xp_fail_(const char *file, int line, const char *func,
	const std::string& msg);
