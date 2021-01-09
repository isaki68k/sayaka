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
	Json Parse(const Json& ng);

	// ツイート status がユーザ ng_user のものか調べる。
	bool MatchUser(const std::string& ng_user, const Json& status) const;

	// status の本文その他を NG ワード ng と照合する。
	bool MatchMain(const Json& ng, const Json& status) const;

	// NGワード(regex) ngword が status 中の本文にマッチするか調べる。
	bool MatchNormal(const std::string& ngword, const Json& status) const;

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
