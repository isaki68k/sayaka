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

#include "Regex.h"
#include <regex>

// <regex> をヘッダに持ち出さないため、ここで内部クラスを定義する。
class RegexInner
{
 public:
	std::regex re {};
};

// コンストラクタ
Regex::Regex()
{
	inner = new RegexInner();
}

// デストラクタ
Regex::~Regex()
{
	delete inner;
}

// 正規表現をセットする
bool
Regex::Assign(const std::string& regex)
{
	bool rv = true;
	try {
		// どのケースも大文字小文字は無視でよい
		inner->re.assign(regex, std::regex_constants::icase);
	} catch (...) {
		rv = false;
	}
	return rv;
}

// text がこの正規表現と一致するか調べる
bool
Regex::Search(const std::string& text) const
{
	bool rv = false;
	try {
		rv = std::regex_search(text, inner->re);
	} catch (...) {
		rv = false;
	}
	return rv;
}
