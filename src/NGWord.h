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
#include "Regex.h"

class NGStatus;
class NGWord;

// NGワードリスト
class NGWordList
	: public std::vector<NGWord *>
{
 public:
	NGWordList();
	NGWordList(const std::string& filename_)
		: NGWordList()
	{
		SetFileName(filename_);
	}
	~NGWordList();

	// ファイル名をセットする
	bool SetFileName(const std::string& filename);

	// NG ワードリストをファイルから読み込む
	bool ReadFile();

	// NG ワードリストをファイルに保存する
	bool WriteFile();

	// NG ワードを追加する
	NGWord *Add(const std::string& word, const std::string& user);

	// src から NG ワードインスタンスを生成する
	static NGWord *Parse(const Json& src);

	// NG ワードリストと照合する
	bool Match(NGStatus *ngstat, const Json& status) const;

	// コマンド
	bool CmdAdd(const std::string& word, const std::string& user);
	bool CmdDel(const std::string& ngword_id);
	bool CmdList();

	std::string Filename {};
};

class NGStatus
{
 public:
	bool match {};
	std::string screen_name {};
	std::string name {};
	std::string time {};
	std::string ngword {};
};

// NGワード1項目の基本クラス
class NGWord
{
 public:
	enum Type
	{
		Regular = 0,
		Live,
		Delay,
		LessRT,
		Source,
		MAX,
	};

 protected:
	// 必ず継承で作る
	NGWord(Type type_, int id_,
		const std::string& ngword_, const std::string& nguser_);
 public:
	virtual ~NGWord();

	int GetId() const { return id; }
	const std::string& GetWord() const { return ngword; }
	const std::string& GetUser() const { return nguser; }

	// ユーザ指定があれば true を返す
	bool HasUser() const { return (nguser.empty() == false); }

	// ツイート status がこのユーザのものなら true を返す
	bool MatchUser(const Json& status) const;

	// status と照合する。
	virtual bool Match(const Json& status, const Json **matched_user) const = 0;

	// この NG ワードの内部状態を文字列にして返す
	// (ベースクラス自身も実体を持っている)
	virtual std::string Dump() const;

	// type を文字列にして返す
	static std::string Type2str(Type type);

 public:
	// 本文を正規表現 re と照合する
	bool MatchText(const Json& status) const;

 protected:
	// 種別
	Type type {};
	// 元データ
	int id {};
	std::string ngword {};
	std::string nguser {};
	// ワーク
	Regex regex {};
};

class NGWordLive : public NGWord
{
	using inherited = NGWord;
 public:
	NGWordLive(int id_,
		const std::string& ngword_, const std::string& nguser_,
		int wday_, int start_, int end1_, int end2_);
	virtual ~NGWordLive() override;

	bool Match(const Json& status, const Json **matched_user) const override;
	std::string Dump() const override;

 private:
	int wday {};
	int start {};
	int end1 {};
	int end2 {};
};

class NGWordDelay : public NGWord
{
	using inherited = NGWord;
 public:
	NGWordDelay(int id_,
		const std::string& ngword_, const std::string& nguser_,
		int hour_, const std::string& ngtext);
	virtual ~NGWordDelay() override;

	bool Match(const Json& status, const Json **matched_user) const override;
	std::string Dump() const override;

 private:
	int delay_sec {};
	std::string ngtext {};
};

class NGWordLessRT : public NGWord
{
	using inherited = NGWord;
 public:
	NGWordLessRT(int id_,
		const std::string& ngword_, const std::string& nguser_, int threshold_);
	virtual ~NGWordLessRT() override;

	bool Match(const Json& status, const Json **matched_user) const override;
	std::string Dump() const override;

 private:
	int threshold {};
};

class NGWordSource : public NGWord
{
	using inherited = NGWord;
 public:
	NGWordSource(int id_,
		const std::string& ngword_, const std::string& nguser_);
	virtual ~NGWordSource() override;

	bool Match(const Json& status, const Json **matched_user) const override;
	std::string Dump() const override;

 private:
	std::string ngsource {};
};

class NGWordRegular : public NGWord
{
	using inherited = NGWord;
 public:
	NGWordRegular(int id_,
		const std::string& ngword_, const std::string& nguser_);
	virtual ~NGWordRegular() override;

	bool Match(const Json& status, const Json **matched_user) const override;

 private:
	const Json *MatchStatus(const Json& status, const Json *status2) const;
	bool MatchName(const Json& status) const;
};
