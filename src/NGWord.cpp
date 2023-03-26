/*
 * Copyright (C) 2014-2023 Tetsuya Isaki
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
}

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
//   source => クライアント名(正規表現)
// それ以外なら ngword をそのまま正規表現として比較に使用。
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

	// 遅延2
	if (StartWith(ngword, "%DELAY2,")) {
		auto tmp = Split(ngword, ",", 5);
		if (tmp.size() < 4 || tmp.size() > 5) {
			goto regular;
		}

		auto wday      = my_strptime(tmp[1], "%a");
		auto startmin  = my_strptime(tmp[2], "%R");
		auto delayhour = stou32def(tmp[3], 0);
		std::string ngtext;
		if (tmp.size() > 4) {
			ngtext = tmp[4];
		}

		return new NGWordDelay2(ngid, ngword, nguser,
			wday, startmin, delayhour, ngtext);
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
 regular:
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
		const auto& id_str = user.value("id_str", "");
		if (nguser_id == id_str) {
			return true;
		}
	}
	if (StartWith(nguser, '@')) {
		if (__predict_false(user.contains("screen_name") == false)) {
			return false;
		}
		const auto& screen_name = user["screen_name"].get<std::string>();
		// Twitter のユーザ名は比較の際は大文字小文字の区別はない
		// (Human68k のファイル名のような感じ)
		if (strcasecmp(nguser.c_str() + 1, screen_name.c_str()) == 0) {
			return true;
		}
	}

	return false;
}

// status の本文を正規表現 re と比較する。マッチすれば true を返す。
// status が RT かどうかには関知しない。
bool
NGWord::MatchText(const Json& status) const
{
	// 本文を取得。
	const Json *textj = GetFullText(status);
	if (__predict_false(textj == NULL)) {
		return false;
	}

	const auto& text = (*textj).get<std::string>();
	if (regex.Search(text) == false) {
		return false;
	}
	return true;
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
	 case Type::Delay2:
		return "Delay2";
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

	// 正規表現オブジェクトを作成
	regex.Assign(ngtext);
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

		if (MatchText(s) == false) {
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
// NG ワード (%DELAY2)
// XXX 遅延は実装してなくて単に非表示になるだけ
//

// コンストラクタ
NGWordDelay2::NGWordDelay2(int id_,
	const std::string& ngword_, const std::string& nguser_,
	int wday_, int start_, int delay_, const std::string& ngtext_)
	: inherited(Delay2, id_, ngword_, nguser_)
{
	startwday = wday_;
	startmin  = start_;
	delayhour = delay_;

	// 正規表現オブジェクトを作成
	regex.Assign(ngtext);
}

// デストラクタ
NGWordDelay2::~NGWordDelay2()
{
}

// status がこの NG ワードに一致するか調べる。
// 一致すれば matched_user にユーザ情報を格納して true を返す。
// 一致しなければ matched_user は触らず false を返す。
bool
NGWordDelay2::Match(const Json& status, const Json **matched_user) const
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

		if (MatchText(s) == false) {
			// 本文指定があって、本文が一致しない
			return false;
		}
	}

	// 一致したら発言時刻と現在時刻を比較
	time_t dt = get_datetime(status);
	// 日曜0時からの経過分にする
	struct tm *tm = localtime(&dt);
	int min = tm->tm_wday * 24 * 60 +
		tm->tm_hour * 60 +
		tm->tm_min;

	// 開始時刻も経過分にする
	int start = startwday * 24 * 60 + startmin;
	// 終了時刻(解禁時刻)
	int open  = start + delayhour * 60;

	int min2 = min + 7 * 24 * 60;
	if ((start <= min  && min  < open) ||
	    (start <= min2 && min2 < open)   )
	{
		// 一致した(NG)
		*matched_user = &status["user"];
		return true;
	}
	return false;
}

std::string
NGWordDelay2::Dump() const
{
	return inherited::Dump() +
		string_format(" wday=%d start=%d hour=%d ngtext=|%s|",
			startwday, startmin, delayhour, ngtext.c_str());
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

	// RT元の retweet_count (status.retweet_count) ではなく
	// RT先の retweet_count (status.retweeted_status.retweet_count) のほう。
	const Json& rt = status["retweeted_status"];
	const Json& retweet_count = rt["retweet_count"];
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

	// 正規表現オブジェクトを作成
	regex.Assign(ngsource);
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
	if (regex.Search(source) == false) {
		return false;
	}

	*matched_user = &s["user"];
	return true;
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
	// 正規表現オブジェクトを作成
	regex.Assign(ngword);
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
	const Json *user = NULL;

	if (status.contains("retweeted_status")) {
		// RT あり
		const Json& rt_status = status["retweeted_status"];
		user = MatchStatus(status, &rt_status);
	} else {
		// RT なし
		user = MatchStatus(status, NULL);
	}

	// 本文が一致せず、QT があれば QT 側も調べる。
	if (user == NULL && status.contains("quoted_status")) {
		const Json& qt_status = status["quoted_status"];

		if (qt_status.contains("retweeted_status")) {
			// 引用先が RT あり
			const Json& rt_status = qt_status["retweeted_status"];
			user = MatchStatus(status, &rt_status);
		} else {
			// 引用先が RT なし
			user = MatchStatus(status, &qt_status);
		}
	}

	if (user == NULL) {
		return false;
	}
	*matched_user = user;
	return true;
}

// status (と status2) から比較する。
// status は常に発言元 status、NULL になり得ないので実体参照。
// status2 はあれば RT か QT 先 status、なければ NULL。
// 戻り値は、一致すれば表示用のユーザ情報、一致しなければ NULL。
const Json *
NGWordRegular::MatchStatus(const Json& status, const Json *status2) const
{
	const Json *user = NULL;

	if (HasUser() == false) {
		// ユーザ指定がなければ、比較へ進む。
		// その際の user は発言元ユーザでいい。
		user = &status["user"];

	} else if (MatchUser(status)) {
		// ユーザ指定があって status と一致すれば、user は status のユーザ。
		user = &status["user"];

	} else if (status2 != NULL && MatchUser(*status2)) {
		// ユーザ指定があって status2 と一致すれば、user は status2 のユーザ。
		const Json& s = *status2;
		user = &s["user"];

	} else {
		// ユーザ指定があってどちらとも一致しなければ、ここで終了。
		return NULL;
	}
	assert(user);

	if (status2 == NULL) {
		// status2 がなければ status の本文のみを調べる。
		// RT も QT もない地の文の時だけこっち。
		if (MatchText(status)) {
			return user;
		}
	} else {
		// status2 があれば status2 の本文と screen_name を調べる。
		if (MatchText(*status2) || MatchName(*status2)) {
			return user;
		}
	}

	return NULL;
}

// status の screen_name を正規表現 re と比較する。
// マッチすれば true を返す。
// status が RT かどうかには関知しない。
bool
NGWordRegular::MatchName(const Json& status) const
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
	if (regex.Search(screen_name) == false) {
		return false;
	}
	return true;
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


// status から本文(の入っている Json *)を取り出す。
// 本文フィールドは full_text が extended_tweet 以下にある場合と
// (status の) 直下にある場合がなぜかあるようで、違いはよく分からない。
// またどちらもなければ仕方ないので (status 直下の) text を使う。
// text は長いと末尾が "…" に置き換えられたテキスト。
// いずれもなければ NULL を返す。
// (ヘッダの都合でここに置いてあるが sayaka からも使う)
const Json *
GetFullText(const Json& status)
{
	if (status.contains("extended_tweet")) {
		const Json& exttweet = status["extended_tweet"];
		if (exttweet.contains("full_text")) {
			return &exttweet["full_text"];
		}
	}
	if (status.contains("full_text")) {
		return &status["full_text"];
	}
	if (status.contains("text")) {
		return &status["text"];
	}

	return NULL;
}
