/*
 * Copyright (C) 2014-2021 Tetsuya Isaki
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
#include "Json.h"

class NGStatus
{
 public:
	bool match {};
	std::string screen_name {};
	std::string name {};
	std::string time {};
	std::string ngword {};
};

class NGWord
{
 public:
	NGWord();
	NGWord(const std::string& filename_);

	// ファイル名をセットする
	bool SetFileName(const std::string& filename);

	// NG ワードをファイルから読み込む
	bool ReadFile();

	// NG ワードをファイルに保存する
	bool WriteFile();

	// NG ワードをファイルから読み込んで、前処理する。
	bool ParseFile();

	// NG ワードと照合する。
	bool Match(NGStatus *ngstat, const Json& status) const;

	// コマンド
	bool CmdAdd(const std::string& word, const std::string& user);
	bool CmdDel(const std::string& ngword_id);
	bool CmdList();

 public:
	// NG ワード1つを前処理して返す
	static Json Parse(const Json& ng);

	// ツイート status がユーザ ng_user のものか調べる。
	static bool MatchUser(const std::string& ng_user, const Json& status);

	// status の本文その他を NG ワード ng と照合する。
	static bool MatchMain(const Json& ng, const Json& status);

	// 正規表現のNGワード ngword が status 中の本文にマッチするか調べる。
	static bool MatchRegular(const std::string& ngword, const Json& status);

	// status の本文その他を NG ワード ng と照合する。
	bool MatchMainRT(const Json& ng, const Json& status) const;

	std::string Filename {};

	Json ngwords {};

	// 直近のエラーメッセージ
	std::string LastErr {};
};

#if defined(SELFTEST)
extern void test_NGWord();
#endif
