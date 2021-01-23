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
#include "StringUtil.h"
#include <vector>
#include <iconv.h>

// Unicode コードポイントの配列。
// UString とは言っているが文字列ではなく vector 派生なことに留意。
// 主に std::vector に append() がないのでこんなことになっている…。
class UString : public std::vector<unichar>
{
	using inherited = std::vector<unichar>;
	using size_type = std::size_t;
 public:
	// 出力文字コードの初期化
	static bool Init(const std::string& codeset);

	// コンストラクタ
	explicit UString()				// デフォルトコンストラクタ
		: inherited() { }
	UString(const UString& s)		// コピーコンストラクタ
		: inherited(s) { }
	UString(UString&& s) noexcept	// ムーブコンストラクタ
		: inherited(s) { }
	UString(std::initializer_list<unichar> il)	// 初期化子リストを受け取る
		: inherited(il) { }

	UString(const std::string& s) {	// std::string からの変換
		Append(s);
	}

	// 代入演算子
	UString& operator=(const UString& s) {
		inherited::operator=(s);
		return *this;
	}
	UString& operator=(const std::string& s) {
		clear();
		return Append(s);
	}

	// UString s を末尾に追加
	UString& Append(const UString& s) {
		insert(end(), s.begin(), s.end());
		return *this;
	}
	UString& operator+=(const UString& s) {
		return Append(s);
	}

	// unichar u を末尾に追加
	UString& Append(unichar u) {
		emplace_back(u);
		return *this;
	}
	UString& operator+=(unichar u) {
		return Append(u);
	}

	// 文字列 s (ASCII) を1文字ずつ末尾に追加
	// (ASCII またはエスケープシーケンスのみの場合に使用できる)
	UString& Append(const std::string& s) {
		for (auto c : s) {
			emplace_back((unsigned char)c);
		}
		return *this;
	}

	// UTF-8 文字列 str を UString に変換する
	static UString FromUTF8(const std::string& str);

	// Init() で設定した文字コードの std::string に変換
	std::string ToString() const;

	// uni が Init() で設定した文字コードに変換できるか
	static bool IsUCharConvertible(unichar uni);

	// src からの UTF-8 1文字を unichar に変換する
	static std::pair<unichar, int> UCharFromUTF8(const char *src);

	// unichar 1文字を UTF-8 に変換して dst に書き出す
	static int UCharToUTF8(char *dst, unichar code);

	std::string dump() const;

 private:
	// UTF-8 文字列を Init() で設定した出力文字コードに変換する
	static std::string UTF8ToOutCode(const std::string& utf8);

	// 出力文字コードにするのに iconv 変換が必要かどうか
	static bool use_iconv;
};

// + 演算子
// (std::string::operator+ 参照)
static inline UString operator+(const UString& lhs, const UString& rhs) {
	return UString(lhs).Append(rhs);		// (1)
}
static inline UString operator+(UString&& lhs, const UString& rhs) {
	return std::move(lhs.Append(rhs));		// (2)
}
/*
static inline UString operator+(const UString& lhs, UString&& rhs) {
	return std::move(rhs.insert(0, lhs));	// (3)
}*/
static inline UString operator+(UString&& lhs, UString&& rhs) {
	return std::move(lhs.Append(rhs));		// (4)
}
static inline UString operator+(const char *lhs, const UString& rhs) {
	return UString(lhs) + rhs;				// (5)
}
/*
static inline UString operator+(const char *lhs, UString&& rhs) {
	return std::move(rhs.insert(0, lhs));	// (6)
}*/
static inline UString operator+(char lhs, const UString& rhs) {
	return UString().Append(lhs) + rhs;		// (7)
}
/*
static inline UString operator+(char lhs, UString&& rhs) {
	return std::move(rhs.insert(0, 1, lhs)); // (8)
}*/
static inline UString operator+(const UString& lhs, const char *rhs) {
	return lhs + UString(rhs);				// (9)
}
static inline UString operator+(UString&& lhs, const char *rhs) {
	return std::move(lhs.Append(rhs));		// (10)
}
static inline UString operator+(const UString& lhs, char rhs) {
	return lhs + UString().Append(rhs);		// (11)
}
static inline UString operator+(UString&& lhs, char rhs) {
	return std::move(lhs.Append(rhs));		// (12)
}

#if defined(SELFTEST)
extern void test_UString();
#endif
