#pragma once

#include "sayaka.h"
#include "StringUtil.h"
#include <vector>

// Unicode コードポイントの配列。
// UString とは言っているが文字列ではなく vector 派生なことに留意。
// 主に std::vector に append() がないのでこんなことになっている…。
class UString : public std::vector<unichar>
{
	using inherited = std::vector<unichar>;
	using size_type = std::size_t;
 public:
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

	// 文字列 s (UTF-8) を UString に変換して末尾に追加
	UString& Append(const std::string& s);
	UString& operator+=(const std::string& s) {
		return Append(s);
	}

	// 文字列 s (ASCII) を1文字ずつ末尾に追加
	// (ASCII またはエスケープシーケンスのみの場合に使用できる)
	UString& AppendChars(const std::string& s) {
		for (auto c : s) {
			emplace_back((unsigned char)c);
		}
		return *this;
	}
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


// 文字列との相互変換
extern UString StringToUString(const std::string& str);
extern std::string UStringToString(const UString& ustr);

#if defined(SELFTEST)
extern void test_UString();
#endif
