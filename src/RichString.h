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
