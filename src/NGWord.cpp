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

#include "Diag.h"
#include "FileUtil.h"
#include "NGWord.h"
#include "StringUtil.h"
#include "subr.h"
#include <regex>

//
// NGワードリスト
//

// コンストラクタ
NGWordList::NGWordList()
{
}

// デストラクタ
NGWordList::~NGWordList()
{
	while (empty() == false) {
		NGWord *ng = back();
		pop_back();
		delete ng;
	}
}

// ファイル名をセットする
bool
NGWordList::SetFileName(const std::string& filename_)
{
	Filename = filename_;
	return true;
}

// NG ワードをファイルから読み込む。
// 読み込めれば true を返す。
//
// NG ワードファイルは JSON で
// "ngword_list": [
//   { "ngword" : NGワード, "user" : ユーザ情報 },
//   { "ngword" : NGワード, "user" : ユーザ情報 },
//   ...
// ]
// という構造
bool
NGWordList::ReadFile()
{
	// ファイルがないのは構わない
	if (FileUtil::Exists(Filename) == false) {
		return true;
	}
	// ファイルが空でも構わない
	auto filetext = FileReadAllText(Filename);
	if (filetext.empty()) {
		return true;
	}

	const Json file = Json::parse(filetext);
	if (file.contains("ngword_list") == false) {
		return false;
	}

	const Json& ngword_list = file["ngword_list"];
	for (const Json& ngword_json : ngword_list) {
		NGWord *ng = Parse(ngword_json);
		if (ng == NULL) {
			return false;
		}
		push_back(ng);
	}
	return true;
}

// NG ワードをファイルに保存する
bool
NGWordList::WriteFile()
{
	printf("%s not implemented\n", __method__);
	return false;
}

// NG ワード1つを追加する
// 追加した NGWord へのポインタを返す。追加出来なければ NULL を返す
NGWord *
NGWordList::Add(const std::string& word, const std::string& user)
{
	// もっとも新しい ID を探す (int が一周することはないだろう)
	int new_id = 0;
	for (const NGWord *p : *this) {
		const NGWord& ng = *p;

		int id = ng.GetId();
		new_id = std::max(new_id, id);
	}
	new_id++;

	Json obj;
	obj["id"] = new_id;
	obj["ngword"] = word;
	obj["user"] = user;
	emplace_back(Parse(obj));

	return back();
};

// 入力ファイル上の NG ワード(JSON形式) 1つを NGWord クラスにして返す。
//   ngword => NGワード ("ngword") (ファイルから読んだまま変更しない)
//   nguser => ユーザ情報 ("user") (ファイルから読んだまま変更しない)
// 以下種類別に
// "%LIVE" なら
//   wday  => 曜日
//   start => 開始時間を0時からの分で
//   end   => 終了時間を0時からの分で。日をまたぐ場合は 24:00
//   end2  => 日をまたぐ場合の終了時間を分で。またがないなら -1
// "%DELAY" なら
//   ngtext => キーワード
//	 delay  => 遅延させる時間 [時間]
// "%RT" なら
//   rtnum  => RT数閾値
// "%SOURCE" なら
//   source => クライアント名(regex)
// それ以外なら ngword をそのまま regex として比較に使用。
/*static*/ NGWord *
NGWordList::Parse(const Json& src)
{
	// 歴史的経緯によりユーザ情報は、
	// JSON ファイル上でのキーは "user" だが
	// NGWord クラスの変数名は nguser なことに注意。
	int ngid = src["id"].get<int>();
	const auto& ngword = src["ngword"].get<std::string>();
	const auto& nguser = src["user"].get<std::string>();

	// 生実況 NG
	if (StartWith(ngword, "%LIVE,")) {
		auto tmp = Split(ngword, ",", 5);
		// 曜日と時刻2つを取り出す
		auto wday  = my_strptime(tmp[1], "%a");
		auto start = my_strptime(tmp[2], "%R");
		auto end1  = my_strptime(tmp[3], "%R");
		auto end2  = -1;
		if (end1 > 1440) {
			end2 = end1 - 1440;
			end1 = 1440;
		}

		return new NGWordLive(ngid, ngword, nguser, wday, start, end1, end2);
	}

	// 遅延
	if (StartWith(ngword, "%DELAY,")) {
		auto tmp = Split(ngword, ",", 3);
		auto hourstr = tmp[1];
		auto hour = 0;
		if (EndWith(hourstr, 'd')) {
			hour = stou32def(hourstr, 0) * 24;
		} else {
			hour = stou32def(hourstr, 0);
		}
		auto ngtext = tmp[2];

		return new NGWordDelay(ngid, ngword, nguser, hour, ngtext);
	}

	// RT NG
	if (StartWith(ngword, "%RT,")) {
		auto tmp = Split(ngword, ",", 2);
		auto rtnum = stou32def(tmp[1], 0);

		return new NGWordLessRT(ngid, ngword, nguser, rtnum);
	}

	// クライアント名
	if (StartWith(ngword, "%SOURCE,")) {
		return new NGWordSource(ngid, ngword, nguser);
	}

	// 通常ワード
	return new NGWordRegular(ngid, ngword, nguser);
}

// NG ワードとリスト照合する。
// 一致したら ngstat を埋めて true を返す。
// 一致しなければ false を返す (ngstat は不定)。
bool
NGWordList::Match(NGStatus *ngstatp, const Json& status) const
{
	NGStatus& ngstat = *ngstatp;

	const Json *user = NULL;	// マッチしたユーザ
	for (const NGWord *p : *this) {
		const NGWord& ng = *p;

		if (ng.Match(status, &user) == true) {
			const Json& u = *user;

			ngstat.match = true;
			ngstat.screen_name = u.value("screen_name", "");
			ngstat.name = u.value("name", "");
			ngstat.time = formattime(status);
			ngstat.ngword = ng.GetWord();
			return true;
		}
#if 0
		// QT 元ユーザ名がマッチするなら QT 先も RT チェック
		if (user == NULL && status.contains("quoted_status")) {
			if (ng.HasUser() == false || ng.MatchUser(status)) {
				const Json& qt_status = status["quoted_status"];
				if (ng.MatchMain(qt_status)) {
					user = &status["user"];
				}
			}
		}

		if (user) {
			const Json& u = *user;

			ngstat.match = true;
			ngstat.screen_name = u.value("screen_name", "");
			ngstat.name = u.value("name", "");
			ngstat.time = formattime(status);
			ngstat.ngword = ng.ngword;
			return true;
		}
#endif
	}

	return false;
}

//
// NG ワード 1項目(基本クラス)
//

// コンストラクタ
NGWord::NGWord(NGWord::Type type_, int id_,
	const std::string& ngword_, const std::string& nguser_)
{
	type = type_;
	id = id_;
	ngword = ngword_;
	nguser = nguser_;
}

// デストラクタ
NGWord::~NGWord()
{
}

// ツイート status がこのユーザのものか調べる。
bool
NGWord::MatchUser(const Json& status) const
{
	const Json& user = status["user"];

	if (StartWith(nguser, "id:")) {
		std::string_view nguser_id = std::string_view(nguser).substr(3);
		std::string_view id_str = user.value("id_str", "");
		if (nguser_id == id_str) {
			return true;
		}
	}
	if (StartWith(nguser, '@')) {
		std::string_view ngname = std::string_view(nguser).substr(1);
		std::string_view screen_name = user.value("screen_name", "");
		// XXX 大文字小文字は区別しないにしないといけない気がする
		if (ngname == screen_name) {
			return true;
		}
	}

	return false;
}

// status の本文を正規表現 word と比較する。マッチすれば true を返す。
// status が RT かどうかには関知しない。
bool
NGWord::MatchText(const Json& status, const std::string& word) const
{
	const Json *textp = NULL;

	// extended_tweet->full_text、なければ text、どちらもなければ false?
	do {
		if (status.contains("extended_tweet")) {
			const Json& extended = status["extended_tweet"];
			if (extended.contains("full_text")) {
				textp = &extended["full_text"];
				break;
			}
		}
		if (status.contains("text")) {
			textp = &status["text"];
			break;
		}
		return false;
	} while (0);

	const auto& text = (*textp).get<std::string>();
	try {
		std::regex re(word);
		if (regex_search(text, re)) {
			return true;
		}
	} catch (...) {
		// 正規表現周りで失敗したらそのまま、マッチしなかった、でよい
	}
	return false;
}

std::string
NGWord::Dump() const
{
	return string_format("id=%d word=|%s| user=|%s| type=%s",
		GetId(), GetWord().c_str(), GetUser().c_str(), Type2str(type).c_str());
}

/*static*/ std::string
NGWord::Type2str(Type type)
{
	switch (type) {
	 case Type::Regular:
		return "Regular";
	 case Type::Live:
		return "Live";
	 case Type::Delay:
		return "Delay";
	 case Type::LessRT:
		return "LessRT";
	 case Type::Source:
		return "Source";
	 default:
		return string_format("?(%d)", (int)type);
	}
}

//
// NG ワード (%LIVE)
//

// コンストラクタ
NGWordLive::NGWordLive(int id_,
	const std::string& ngword_, const std::string& nguser_,
	int wday_, int start_, int end1_, int end2_)
	: inherited(Live, id_, ngword_, nguser_)
{
	wday  = wday_;
	start = start_;
	end1  = end1_;
	end2  = end2_;
}

// デストラクタ
NGWordLive::~NGWordLive()
{
}

// status がこの NG ワードに一致するか調べる。
// 一致すれば matched_user にユーザ情報を格納して true を返す。
// 一致しなければ matched_user は触らず false を返す。
bool
NGWordLive::Match(const Json& status, const Json **matched_user) const
{
	// userなし, plain -> status.time
	// userなし, RT    -> 元status.time
	// userあり, plain -> if (user)   status.time
	// userあり, RT    -> if (元user) 元status.time

	// RT の有無に関わらず、元 status だけ見る。
	if (HasUser() == false || MatchUser(status)) {
		// 発言時刻
		time_t dt = get_datetime(status);
		struct tm tm;
		localtime_r(&dt, &tm);
		auto tmmin = tm.tm_hour * 60 + tm.tm_min;

		// 指定曜日の時間の範囲内ならアウト
		if (tm.tm_wday == wday && start <= tmmin && tmmin < end1) {
			*matched_user = &status["user"];
			return true;
		}
		// 終了時刻が24時を超える場合は、越えたところも比較
		if (end2 >= 0) {
			int wday2 = (wday + 1) % 7;
			if (tm.tm_wday == wday2 && 0 <= tmmin && tmmin < end2) {
				*matched_user = &status["user"];
				return true;
			}
		}
	}
	return false;
}

std::string
NGWordLive::Dump() const
{
	return inherited::Dump() +
		string_format(" wday=%d start=%d end1=%d end2=%d",
			wday, start, end1, end2);
}

//
// NG ワード (%DELAY)
// XXX 遅延は実装してなくて単に非表示になるだけ
//

// コンストラクタ
NGWordDelay::NGWordDelay(int id_,
	const std::string& ngword_, const std::string& nguser_,
	int hour, const std::string& ngtext_)
	: inherited(Delay, id_, ngword_, nguser_)
{
	// UI は時間単位で指定だが比較する時は time_t なのでここで秒にする
	delay_sec = hour * 3600;
	ngtext = ngtext_;
}

// デストラクタ
NGWordDelay::~NGWordDelay()
{
}

// status がこの NG ワードに一致するか調べる。
// 一致すれば matched_user にユーザ情報を格納して true を返す。
// 一致しなければ matched_user は触らず false を返す。
bool
NGWordDelay::Match(const Json& status, const Json **matched_user) const
{
	// RT先とRT元が入り混じっているので注意。
	// userなし, plain -> status.text, status.time
	// userなし, RT    -> RT先status.text, RT元status.time
	// userあり, plain -> if (元user) { status.text, status.time }
	// userあり, RT    -> if (元user) { RT先status.text, RT元status.time }

	// ユーザは RT の有無に関わらず、元 status だけ見る。
	// ユーザ指定があって一致しないケースを先に弾く。
	if (HasUser() && MatchUser(status) == false) {
		return false;
	}

	// 本文指定があれば本文を比較
	if (ngtext.empty() == false) {
		// 本文は、RT なら RT 先本文。
		const Json& s = status.contains("retweeted_status")
			? status["retweeted_status"]
			: status;

		if (MatchText(s, ngtext) == false) {
			// 本文指定があって、本文が一致しない
			return false;
		}
	}

	// 一致したら発言時刻と現在時刻を比較
	// ここの発言時刻は RT の有無にかかわらず元 status の時刻。
	time_t dt = get_datetime(status);
	// delay_sec [秒] 以内なら表示しない(=NG)ので、越えていれば不一致。
	time_t now = time(NULL);
	if (now > dt + delay_sec) {
		return false;
	}

	// 一致した(NG)
	*matched_user = &status["user"];
	return true;
}

std::string
NGWordDelay::Dump() const
{
	return inherited::Dump() +
		string_format(" delay_sec=%d ngtext=|%s|", delay_sec, ngtext.c_str());
}

//
// NG ワード(RT数)
//

// コンストラクタ
NGWordLessRT::NGWordLessRT(int id_,
	const std::string& ngword_, const std::string& nguser_, int threshold_)
	: inherited(LessRT, id_, ngword_, nguser_)
{
	threshold = threshold_;
}

// デストラクタ
NGWordLessRT::~NGWordLessRT()
{
}

// status がこの NG ワードに一致するか調べる。
// 一致すれば matched_user にユーザ情報を格納して true を返す。
// 一致しなければ matched_user は触らず false を返す。
bool
NGWordLessRT::Match(const Json& status, const Json **matched_user) const
{
	// userなし, plain -> -
	// userなし, RT    -> RT先status.count
	// userあり, plain -> -
	// userあり, RT    -> if (元user) RT先status.count

	// RT を持ってなければそもそも関係ない
	if (status.contains("retweeted_status") == false) {
		return false;
	}

	// ユーザ指定があって一致しないケースを先に弾く。
	if (HasUser() && MatchUser(status) == false) {
		return false;
	}

	// RT元の retweet_count (status.retweet_count) と
	// RT先の retweet_count (status.retweeted_status.retweet_count) は
	// 同じ値のようだ。
	const Json& retweet_count = status["retweet_count"];
	int rtcnt = retweet_count.get<int>();
	if (rtcnt > threshold) {
		return false;
	}

	*matched_user = &status["user"];
	return true;
}

std::string
NGWordLessRT::Dump() const
{
	return inherited::Dump() + string_format(" threshold=%d", threshold);
}

//
// NG ワード(SOURCE)
//

// コンストラクタ
NGWordSource::NGWordSource(int id_,
	const std::string& ngword_, const std::string& nguser_)
	: inherited(Source, id_, ngword_, nguser_)
{
	ngsource = ngword.substr(strlen("%SOURCE,"));
}

// デストラクタ
NGWordSource::~NGWordSource()
{
}

// status がこの NG ワードに一致するか調べる。
// 一致すれば matched_user にユーザ情報を格納して true を返す。
// 一致しなければ matched_user は触らず false を返す。
bool
NGWordSource::Match(const Json& status, const Json **matched_user) const
{
	// userなし, plain -> status.source
	// userなし, RT    -> RT先status.source
	// userあり, plain -> if (user) status.source
	// userあり, RT    -> if (RT先user) RT先status.source

	const Json& s = status.contains("retweeted_status")
		? status["retweeted_status"]
		: status;

	// ユーザ指定があって一致しないケースを先に弾く。
	if (HasUser() && MatchUser(s) == false) {
		return false;
	}

	// なければ仕方ないか
	if (s.contains("source") == false) {
		return false;
	}

	const Json& source_json = s["source"];
	const auto& source = source_json.get<std::string>();
	try {
		std::regex re(ngsource);
		if (regex_search(source, re)) {
			*matched_user = &s["user"];
			return true;
		}
	} catch (...) {
		// 正規表現周りで失敗したらそのまま、マッチしなかった、でよい
	}
	return false;
}

std::string
NGWordSource::Dump() const
{
	return inherited::Dump() +
		string_format(" ngsource=|%s|", ngsource.c_str());
}

//
// NG ワード(通常)
//

// コンストラクタ
NGWordRegular::NGWordRegular(int id_,
	const std::string& ngword_, const std::string& nguser_)
	: inherited(Regular, id_, ngword_, nguser_)
{
}

// デストラクタ
NGWordRegular::~NGWordRegular()
{
}

// status がこの NG ワードに一致するか調べる。
// 一致すれば matched_user にユーザ情報を格納して true を返す。
// 一致しなければ matched_user は触らず false を返す。
bool
NGWordRegular::Match(const Json& status, const Json **matched_user) const
{
	// userなし, plain -> status.text
	// userなし, RT    -> MatchRT(RT先status)
	// userあり, plain -> if (user) status.text
	// userあり, RT    -> if (RT元user || RT先user) MatchRT(RT先status)
	// ここで matchRT(rt) := (rt.text, rt.screen_name)

	if (status.contains("retweeted_status") == false) {
		// RT でない場合

		// ユーザ指定があって一致しないケースを先に弾く。
		if (HasUser() && MatchUser(status) == false) {
			return false;
		}

		// 本文を調べる
		if (MatchText(status, ngword)) {
			*matched_user = &status["user"];
			return true;
		}

	} else if (HasUser() == false) {
		// RT があって、ユーザ指定がない場合、
		// RT 先本文と RT 先 screen_name を調べる。

		const Json& s = status["retweeted_status"];

		if (MatchText(s, ngword) || MatchName(s, ngword)) {
			// おそらくRT元ユーザ名を示すほうが分かりやすいはず
			*matched_user = &status["user"];
			return true;
		}

	} else {
		// RT があって、ユーザ指定もある場合、
		// ユーザの比較は RT元ユーザ、RT先ユーザ両方行う。
		// また RT 先本文と RT 先 screen_name を調べる。

		const Json& s = status["retweeted_status"];
		const Json *u = NULL;

		// ユーザ指定がある場合、マッチしたほうを示すほうがよかろう
		if (MatchUser(status)) {
			u = &status["user"];
		} else if (MatchUser(s)) {
			u = &s["user"];
		} else {
			return false;
		}

		// 比較するのは RT 先だが、誰でマッチしたかとは別
		if (MatchText(s, ngword) || MatchName(s, ngword)) {
			*matched_user = u;
			return true;
		}
	}
	return false;
}

// status の screen_name を正規表現 word と比較する。
// マッチすれば true を返す。
// status が RT かどうかには関知しない。
bool
NGWordRegular::MatchName(const Json& status, const std::string& word) const
{
	if (__predict_false(status.contains("user") == false)) {
		return false;
	}
	const Json& user = status["user"];

	if (__predict_false(user.contains("screen_name") == false)) {
		return false;
	}
	const Json& screen_name_json = user["screen_name"];
	const auto& screen_name = screen_name_json.get<std::string>();
	try {
		std::regex re(word);
		if (regex_search(screen_name, re)) {
			return true;
		}
	} catch (...) {
		// 正規表現周りで失敗したらそのまま、マッチしなかった、でよい
	}
	return false;
}


//
// コマンド
//

// NG ワードを追加する
bool
NGWordList::CmdAdd(const std::string& word, const std::string& user)
{
	if (!ReadFile()) {
		return false;
	}

	if (!Add(word, user)) {
		return false;
	}

	if (!WriteFile()) {
		return false;
	}
	return true;
}

// NG ワードを削除する
bool
NGWordList::CmdDel(const std::string& ngword_id)
{
	// 未実装
	printf("%s not implemented\n", __PRETTY_FUNCTION__);
	return false;
}

// NG ワード一覧を表示する
bool
NGWordList::CmdList()
{
	if (!ReadFile()) {
		return false;
	}

	for (const NGWord *p : *this) {
		const NGWord& ng = *p;
		auto id = ng.GetId();
		const std::string& word = ng.GetWord();
		const std::string& user = ng.GetUser();

		printf("%d\t%s", id, word.c_str());
		if (!user.empty()) {
			printf("\t%s", user.c_str());
		}
		printf("\n");
	}

	return true;
}


#if defined(SELFTEST)
#include "test.h"
#include <tuple>

void
test_NGWordList_ReadFile()
{
	printf("%s\n", __func__);

	autotemp filename("a.json");
	bool r;

	{
		// ファイルがない場合
		NGWordList list(filename);
		r = list.ReadFile();
		xp_eq(true, r);
	}
	{
		// ファイルがあって空の場合
		NGWordList list(filename);
		FileWriteAllText(filename, "");
		r = list.ReadFile();
		xp_eq(true, r);
	}
	{
		// ["ngword_list"] がない場合
		NGWordList list(filename);
		FileWriteAllText(filename, "{ \"a\": true }");
		r = list.ReadFile();
		xp_eq(false, r);
	}
	{
		// ["ngword_list"] があって空の場合
		NGWordList list(filename);
		FileWriteAllText(filename, "{ \"ngword_list\": [] }");
		r = list.ReadFile();
		xp_eq(true, r);
		xp_eq(0, list.size());
	}
}

void
test_NGWordList_Parse()
{
	printf("%s\n", __func__);

	std::vector<std::pair<std::string, std::string>> table = {
		// src	ダンプのうち可変部分
		{ "a",	" type=Regular" },

		{ "%LIVE,Mon,00:01,23:59,a,a",
		  " type=Live wday=1 start=1 end1=1439 end2=-1" },

		{ "%LIVE,Tue,00:00,24:01,a,a",
		  " type=Live wday=2 start=0 end1=1440 end2=1" },

		{ "%DELAY,1,a,a",	" type=Delay delay_sec=3600 ngtext=|a,a|" },
		{ "%DELAY,2d,a,a",	" type=Delay delay_sec=172800 ngtext=|a,a|" },
		{ "%RT,1",			" type=LessRT threshold=1" },
		{ "%SOURCE,a,a",	" type=Source ngsource=|a,a|" },

		// XXX 異常系をもうちょっとやったほうがいい
	};
	for (const auto& a : table) {
		const auto& src = a.first;
		const auto& expstr = a.second;

		// 期待する文字列
		auto exp = string_format("id=123 word=|%s| user=|@u|", src.c_str());
		exp += expstr;

		// 入力 (ファイルを模しているので "nguser" ではなく "user")
		Json ngword_json;
		ngword_json["id"] = 123;
		ngword_json["user"] = "@u";
		ngword_json["ngword"] = src;
		// 検査 (仕方ないので一つずつやる)
		NGWord *ng = NGWordList::Parse(ngword_json);
		auto act = ng->Dump();
		xp_eq(exp, act, src);
		delete ng;
	}
}

void
test_NGWord_MatchUser()
{
	printf("%s\n", __func__);

	// さすがに status に user がないケースはテストせんでいいだろ…
	std::vector<std::tuple<std::string, std::string, bool>> table = {
		// nguser	status->user								expected
		{ "id:1",	R"( "id_str":"12","screen_name":"ab" )",	false },
		{ "id:12",	R"( "id_str":"12","screen_name":"ab" )",	true },
		{ "id:123",	R"( "id_str":"12","screen_name":"ab" )",	false },
		{ "@a",		R"( "id_str":"12","screen_name":"ab" )",	false },
		{ "@ab",	R"( "id_str":"12","screen_name":"ab" )",	true },
		{ "@abc",	R"( "id_str":"12","screen_name":"ab" )",	false },
		{ "@AB",	R"( "id_str":"12","screen_name":"ab" )",	false },
	};
	for (const auto& a : table) {
		const std::string& nguser = std::get<0>(a);
		const std::string& expr = std::get<1>(a);
		const bool expected = std::get<2>(a);

		Json ngword_json;
		ngword_json["id"] = 123;
		ngword_json["user"] = nguser;
		ngword_json["ngword"] = "a";
		NGWord *ng = NGWordList::Parse(ngword_json);

		Json user = Json::parse("{" + expr + "}");
		Json status { { "user", user } };
		auto actual = ng->MatchUser(status);
		xp_eq(expected, actual, nguser + "," + expr);
	}
}

void
test_NGWord_MatchText()
{
	printf("%s\n", __func__);

	std::vector<std::tuple<std::string, std::string, bool>> table = {
		// testname ngword			expected
		{ "text_only",	"hello",	true },
		{ "full_only",	"hello",	true },
		{ "full_text",	"hello",	true },

		{ "text_only",	"\\.\\.\\.",	false },
		{ "full_only",	"\\.\\.\\.",	false },
		{ "full_text",	"\\.\\.\\.",	false },
	};
	Json statuses {
		{ "text_only", {	// text のみ、今時あるのかは知らん
			{ "text", "hello" },
		} },
		{ "full_only", {	// full_text のみ、あるのかは知らん
			{ "extended_tweet", {
				{ "full_text", "hello" },
			} },
		} },
		{ "full_text", {	// full_text と text 両方。通常はこれ
			{ "text", "hel..." },
			{ "extended_tweet", {
				{ "full_text", "hello" },
			} },
		} },
	};
	for (const auto& a : table) {
		const auto& testname = std::get<0>(a);
		const auto& ngword = std::get<1>(a);
		bool expected = std::get<2>(a);

		// テストを選択
		if (statuses.contains(testname) == false) {
			xp_fail("invalid testname: " + testname);
			continue;
		}

		// ng を作成
		NGWordList nglist;
		NGWord *ng = nglist.Add(ngword, "");

		auto actual = ng->MatchText(statuses[testname], ng->GetWord());
		xp_eq(expected, actual, testname + "," + ngword);
	}
}

void
test_NGWordList_Match()
{
	printf("%s\n", __func__);

	// RT元とRT先が関係するテスト
	std::vector<std::tuple<std::string, std::string, std::string, bool>> table =
	{	// testname	ngword			@user		expected

		// --- Live
		// NGワードはローカル時刻、status は UTC。
		// XXX JST 前提なので、他タイムゾーンではテストがこける…
		{ "std",	"%LIVE,Sun,21:00,22:00",	"",	true },
		{ "std",	"%LIVE,Sun,12:00,21:00",	"",	false },
		// 日またぎ、Sun 21:20 は Sat 45:20…
		{ "std",	"%LIVE,Sat,23:00,45:00",	"",	false },
		{ "std",	"%LIVE,Sat,23:00,45:30",	"",	true },

		// --- Delay は今の所省略

		// --- LessRT
		{ "rt0",	"%RT,2",		"",			false },
		{ "rt0",	"%RT,3",		"",			false },
		{ "rt0",	"%RT,2",		"@other",	false },
		{ "rt0",	"%RT,3",		"@other",	false },
		{ "rt0",	"%RT,2",		"@ange",	false },
		{ "rt0",	"%RT,3",		"@ange",	false },
		{ "rt1",	"%RT,2",		"",			false },
		{ "rt1",	"%RT,3",		"",			false },
		{ "rt1",	"%RT,2",		"@other",	false },
		{ "rt1",	"%RT,3",		"@other",	false },
		{ "rt1",	"%RT,2",		"@ange",	false },
		{ "rt1",	"%RT,3",		"@ange",	false },
		{ "rt2",	"%RT,2",		"",			false },
		{ "rt2",	"%RT,3",		"",			true },
		{ "rt2",	"%RT,2",		"@other",	false },
		{ "rt2",	"%RT,3",		"@other",	false },
		{ "rt2",	"%RT,2",		"@ange",	false },
		{ "rt2",	"%RT,3",		"@ange",	true },
		// RT先ユーザがマッチしても RT 数ルールは適用しない
		{ "rt2",	"%RT,2",		"@seven",	false },
		{ "rt2",	"%RT,3",		"@seven",	false },

		// --- Source
		{ "std",	"%SOURCE,client",	"",		true },
		{ "std",	"%SOURCE,clientx",	"",		false },
		{ "std",	"%SOURCE, v\\d",	"",		true },		// 正規表現

		// --- Regular
		// 通常ワード
		{ "std",	"abc",				"",		true },
		{ "std",	"ABC",				"",		false },
		// 正規表現
		{ "std",	"a(b|d)c",			"",		true },
		{ "std",	"ad?c",				"",		false },

		// 本文のみ検索
		{ "rt0",	"nomatch",		"",			false },
		{ "rt0",	"hello",		"",			true },
		{ "rt0",	"ange",			"",			false },
		{ "rt0",	"nomatch",		"@other",	false },
		{ "rt0",	"hello",		"@other",	false },
		{ "rt0",	"ange",			"@other",	false },
		{ "rt0",	"nomatch",		"@ange",	false },
		{ "rt0",	"hello",		"@ange",	true },
		{ "rt0",	"ange",			"@ange",	false },
		// 通常キーワードは本文のほかにRT先のユーザ名も比較する
		{ "rt2",	"nomatch",		"",			false },
		{ "rt2",	"hello",		"",			true },
		{ "rt2",	"ange",			"",			false },
		{ "rt2",	"seven",		"",			true },
		{ "rt2",	"nomatch",		"@other",	false },
		{ "rt2",	"hello",		"@other",	false },
		{ "rt2",	"ange",			"@other",	false },
		{ "rt2",	"seven",		"@other",	false },
		{ "rt2",	"nomatch",		"@ange",	false },
		{ "rt2",	"hello",		"@ange",	true },
		{ "rt2",	"ange",			"@ange",	false },
		{ "rt2",	"seven",		"@ange",	true },
		{ "rt2",	"nomatch",		"@seven",	false },
		{ "rt2",	"hello",		"@seven",	true },
		{ "rt2",	"ange",			"@seven",	false },
		// XXX これはどうするか?
		{ "rt2",	"seven",		"@seven",	true },
	};
	Json statuses {
		{ "std", {		// 基本形式
			{ "text", "abc hello..." },
			{ "extended_tweet", { { "full_text", "abc hello world" } } },
			{ "created_at", "Sun Jan 10 12:20:00 +0000 2021" },
			{ "source", "test client v0" },
			{ "user", { { "id_str", "100" }, { "screen_name", "ange" } } },
		} },
		{ "rt0", {		// RTされていない通常ツイート
			{ "text", "abc hello..." },
			{ "extended_tweet", { { "full_text", "abc hello world" } } },
			{ "created_at", "Sun Jan 10 12:20:00 +0000 2021" },
			{ "source", "test client v0" },
			{ "user", { { "id_str", "100" }, { "screen_name", "ange" } } },
		} },
		{ "rt1", {		// これ自身がRTされているだけの通常ツイート
			{ "text", "abc hello..." },
			{ "extended_tweet", { { "full_text", "abc hello world" } } },
			{ "created_at", "Sun Jan 10 12:20:00 +0000 2021" },
			{ "source", "test client v0" },
			{ "user", { { "id_str", "100" }, { "screen_name", "ange" } } },
			{ "retweet_count", 3 },
		} },
		{ "rt2", {		// RTしたツイート(3リツイートされている)
			{ "text", "RT: abc hello..." },
			{ "extended_tweet", { { "full_text", "abc hello world" } } },
			{ "created_at", "Sun Jan 10 12:20:00 +0000 2021" },
			{ "source", "test client v0" },
			{ "user", { { "id_str", "100" }, { "screen_name", "ange" } } },
			{ "retweet_count", 3 },
			{ "retweeted_status", {
				{ "text", "abc hello..." },
				{ "extended_tweet", { { "full_text", "abc hello world" } } },
				{ "created_at", "Sun Jan 10 12:20:00 +0000 2021" },
				{ "source", "other client v0" },
				{ "user", { { "id_str", "101" }, { "screen_name", "seven" } } },
				{ "retweet_count", 3 },
			} },
		} },
	};

	for (const auto& a : table) {
		const auto& testname = std::get<0>(a);
		const auto& word = std::get<1>(a);
		const auto& user = std::get<2>(a);
		bool expected = std::get<3>(a);

		// テストを選択
		if (statuses.contains(testname) == false) {
			xp_fail("invalid testname: " + testname);
		}
		const Json& status = statuses[testname];

		// ng を作成
		NGWordList nglist;
		nglist.Add(word, user);

		NGStatus ngstat;
		bool actual = nglist.Match(&ngstat, status);
		xp_eq(expected, actual,
			std::string(testname) + "," + word + "," + user + "," +
				(expected ? "true" : "false"));
	}
}

void
test_NGWord()
{
	test_NGWordList_ReadFile();
	test_NGWordList_Parse();
	test_NGWord_MatchUser();
	test_NGWord_MatchText();
	test_NGWordList_Match();
}
#endif
