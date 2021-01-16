#pragma once

#include "sayaka.h"
#include "UString.h"
#include <string>
#include <vector>

// いろいろ込みの文字情報
class RichChar
{
 public:
	// コードポイント (というか UTF-32)
	unichar code {};

	// この文字の text 先頭からの文字数。
	int charoffset {};

	// このコードポイントの文字の text 先頭からのバイトオフセット
	int byteoffset {};

	// この文字の直前に挿入するエスケープシーケンス
	UString altesc {};

	// 代替 URL
	// 元の URL 文字列範囲は code = -1 で消す。
	std::string alturl {};
};

// いろいろ込みのテキスト
class RichString : public std::vector<RichChar>
{
	// 文字ごとの情報は親クラスの配列で持っている
	using inherited = std::vector<RichChar>;

 public:
	RichString();
	RichString(const std::string& text_);

	// ダンプ文字列を返す (デバッグ用)
	std::string dump() const;

 private:
	// 元の文字列 (変更しない)
	std::string text {};

	// src から info を作成する。
	bool MakeInfo(std::vector<RichChar> *info, const std::string& src) const;
};

#if defined(SELFTEST)
extern void test_RichString();
#endif
