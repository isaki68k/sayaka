#include "FileUtil.h"
#include "NGWord.h"
#include "StringUtil.h"
#include "subr.h"

// コンストラクタ
NGWord::NGWord()
{
}

// コンストラクタ
NGWord::NGWord(const std::string& filename_)
{
	SetFileName(filename_);
}

// ファイル名をセットする
bool
NGWord::SetFileName(const std::string& filename_)
{
	Filename = filename_;
	return true;
}

// NG ワードをファイルから読み込む。
// 読み込めれば true を返す。
// エラーなら LastErr にエラーメッセージを格納して false を返す。
bool
NGWord::ReadFile()
{
	// ファイルがないのは構わない
	if (FileUtil::Exists(Filename) == false) {
		return true;
	}
	auto filetext = FileReadAllText(Filename);
	if (filetext.empty()) {
		return true;
	}

	auto file = Json::parse(filetext);
	if (file.contains("ngword_list")) {
		// 簡単にチェック
		ngwords = file["ngword_list"];
		if (ngwords.is_array()) {
			return true;
		}
	}
	LastErr = "NGWord.ReadFile: Error: ngword file broken";
	return false;
}

// NG ワードをファイルに保存する
bool
NGWord::WriteFile()
{
	// 再構成
	Json root;
	root["ngword_list"] = ngwords;

	bool r = FileWriteAllText(Filename, root.dump());
	if (r == false) {
		LastErr = "NGWord.WriteFile failed";
		return false;
	}

	return true;
}

// NG ワードをファイルから読み込んで、前処理する。
// WriteFile() で書き戻さないこと。
bool
NGWord::ParseFile()
{
	if (!ReadFile()) {
		return false;
	}

	std::vector<Json> ngwords2;
	for (const auto& ng : ngwords) {
		auto ng2 = Parse(ng);
		ngwords2.emplace_back(ng2);
	}
	ngwords = ngwords2;

	return true;
}

// NG ワードを前処理して返す。
//	"ngword" => ngword (ファイルから読んだまま変更しない)
//	"nguser" => user (ファイルから読んだまま変更しない)
//	"type" => 種別
//	以下 type に応じて必要なパラメータ
// type == "%LIVE" なら
//	"wday" => 曜日
//	"start" => 開始時間を0時からの分で
//	"end" => 終了時間を0時からの分で。日をまたぐ場合は 24:00
//	"end2" => 日をまたぐ場合の終了時間を分で。またがないなら -1
// type == "%DELAY" なら
//	"ngtext" => キーワード
//	"delay" => 遅延させる時間 [時間]
// type == "%RT" なら "rtnum" (RT数閾値)
// type == "%SOURCE" なら "source"(クライアント名regex)
// type == "normal" なら "ngword" をそのまま比較に使用
Json
NGWord::Parse(const Json& ng)
{
	Json ng2;

	std::string ngword = ng["ngword"];
	ng2["ngword"] = ngword;
	// 歴史的経緯によりユーザ情報は、
	// ファイル上の JSON ではキーは "user" だが
	// メモリ上の JSON では "nguser" なことに注意。
	ng2["nguser"] = ng["user"];

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
		ng2["type"]  = tmp[0];
		ng2["wday"]  = wday;
		ng2["start"] = start;
		ng2["end1"]  = end1;
		ng2["end2"]  = end2;
		return ng2;
	}

	// 遅延
	if (StartWith(ngword, "%DELAY,")) {
		auto tmp = Split(ngword, ",", 3);
		auto hourstr = tmp[1];
		auto hour = 0;
		if (EndWith(hourstr, 'd')) {
			hour = std::stoi(hourstr) * 24;
		} else {
			hour = std::stoi(hourstr);
		}
		ng2["type"] = tmp[0];
		ng2["delay"] = hour;
		ng2["ngtext"] = tmp[2];
		return ng2;
	}

	// RT NG
	if (StartWith(ngword, "%RT,")) {
		auto tmp = Split(ngword, ",", 2);
		ng2["type"] = tmp[0];
		ng2["rtnum"] = std::stoi(tmp[1]);
		return ng2;
	}

	// クライアント名
	if (StartWith(ngword, "%SOURCE,")) {
		auto tmp = Split(ngword, ",", 2);
		ng2["type"] = tmp[0];
		ng2["source"] = tmp[1];
		return ng2;
	}

	// 通常 NG ワード
	ng2["type"] = "normal";
	return ng2;
}

// NG ワードと照合する。
// 一致したら ngstat を埋めて true を返す。
// 一致しなければ false を返す (ngstat は不定)。
bool
NGWord::Match(NGStatus *ngstatp, const Json& status) const
{
	NGStatus& ngstat = *ngstatp;

	Json user;	// マッチしたユーザ
	for (const auto& ng : ngwords) {
		std::string nguser = ng["nguser"];

		if (status.contains("retweeted_status")) {
			Json s = status["retweeted_status"];

			if (nguser.empty()) {
				// ユーザ指定がなければ、RT先本文を比較
				if (MatchMain(ng, s)) {
					user = s["user"];
				}
			} else {
				// ユーザ指定があって、RT元かRT先のユーザと一致すれば
				// RT先本文を比較。ただしユーザ情報はマッチしたほう。
				if (MatchUser(nguser, status)) {
					if (MatchMainRT(ng, s)) {
						user = status["user"];
					}
				} else if (MatchUser(nguser, s)) {
					if (MatchMain(ng, s)) {
						user = s["user"];
					}
				}
			}
		} else {
			// RT でないケース
			// ユーザ指定がないか、あって一致すれば、本文を比較
			if (nguser.empty() || MatchUser(nguser, status)) {
				if (MatchMain(ng, status)) {
					user = status["user"];
				}
			}
		}

		// QT 元ユーザ名がマッチするなら QT 先も RT チェック
		if (user.is_null() && status.contains("quoted_status")) {
			if (nguser.empty() || MatchUser(nguser, status)) {
				Json qt_status = status["quoted_status"];
				if (MatchMain(ng, qt_status)) {
					user = status["user"];
				}
			}
		}

		if (!user.is_null()) {
			ngstat.match = true;
			ngstat.screen_name = user["screen_name"];
			ngstat.name = user["name"];
			ngstat.time = formattime(status);
			ngstat.ngword = ng["ngword"];
			return true;
		}
	}

	return false;
}

// ツイート status がユーザ ng_user のものか調べる。
// ng_user は "id:<numeric_id>" か "@<screen_name>" 形式。
bool
NGWord::MatchUser(const std::string& ng_user, const Json& status) const
{
	auto u = status["user"];

	if (StartWith(ng_user, "id:")) {
		auto ng_user_id = ng_user.substr(3);
		if (ng_user_id == u["id_str"]) {
			return true;
		}
	}
	if (StartWith(ng_user, '@')) {
		auto ng_screen_name = ng_user.substr(1);
		if (ng_screen_name == u["screen_name"]) {
			return true;
		}
	}

	return false;
}

// status の本文その他を NGワード ng と照合する。
// マッチしたかどうかを返す。
bool
NGWord::MatchMain(const Json& ng, const Json& status) const
{
	const Json& ngword = ng["ngword"];
	std::string ngtype = ng["type"];

	if (ngtype == "%LIVE") {	// 生実況 NG
		int wday  = ng["wday"];
		int start = ng["start"];
		int end1  = ng["end1"];
		int end2  = ng["end2"];

		// 発言時刻
		time_t dt = get_datetime(status);
		struct tm tm;
		localtime_r(&dt, &tm);
		auto tmmin = tm.tm_hour * 60 + tm.tm_min;

		// 指定曜日の時間の範囲内ならアウト
		if (tm.tm_wday == wday && start <= tmmin && tmmin < end1) {
			return true;
		}
		// 終了時刻が24時を超える場合は、越えたところも比較
		if (end2 != -1) {
			wday = (wday + 1) % 7;
			if (tm.tm_wday == wday && 0 <= tmmin && tmmin < end2) {
				return true;
			}
		}

	} else if (ngtype == "%DELAY") {	// 表示遅延
		// まずは通常の文字列比較
		if (MatchNormal(ng["ngtext"], status)) {
			// 一致したら発言時刻と現在時刻を比較

			// 発言時刻
			time_t dt = get_datetime(status);
			// delay[時間]以内なら表示しない(=NG)
			time_t now = time(NULL);
			int delay = ng["delay"];
			if (now < dt + (delay * 3600)) {
				return true;
			}
		}

	// } else if (ngtype == "%RT") {	// 未実装

	} else if (ngtype == "%SOURCE") {	// クライアント名
		std::string stsource = status["source"];
		std::string ngsource = ng["source"];
		if (stsource.find(ngsource)) {
			return true;
		}

	} else {	// 通常 NG ワード
		if (MatchNormal(ngword, status)) {
			return true;
		}
	}

	return false;
}

// NGワード(regex) ngword が status 中の本文にマッチするか調べる。
// マッチすれば true、しなければ false を返す。
bool
NGWord::MatchNormal(const std::string& ngword, const Json& status) const
{
	if (status.contains("text") == false) {
		return false;
	}
	std::string text = status["text"];
	if (status.contains("extended_tweet")) {
		const Json& extended_tweet = status["extended_tweet"];
		if (extended_tweet.contains("full_text")) {
			text = extended_tweet["full_text"];
		}
	}
	// XXX 正規表現未対応
	if (text.find(ngword)) {
		return true;
	}

	return false;
}

// status の本文その他を NG ワード ng と照合する。
// リツイートメッセージ用。
bool
NGWord::MatchMainRT(const Json& ng, const Json& status) const
{
	// まず通常比較
	if (MatchMain(ng, status)) {
		return true;
	}

	// 名前も比較
	const std::string& ngtype = ng["type"];
	if (ngtype == "normal") {
		const Json& user = status["user"];
		const std::string& ngword = ng["ngword"];
		// 正規表現未対応
		return false;
	}

	return false;
}

// NG ワードを追加する
bool
NGWord::CmdAdd(const std::string& word, const std::string& user)
{
	if (!ReadFile()) {
		return false;
	}

	// もっとも新しい ID を探す (int が一周することはないだろう)
	auto new_id = 0;
	
	for (const auto& ng : ngwords) {
		int id = ng["id"].get<int>();

		if (id > new_id) {
			new_id = id;
		}
	}
	new_id++;

	Json obj;
	obj["id"] = new_id;
	obj["ngword"] = word;
	obj["user"] = user;
	ngwords.emplace_back(obj);
	printf("id %d added\n", new_id);

	if (!WriteFile()) {
		return false;
	}
	return true;
}

// NG ワードを削除する
bool
NGWord::CmdDel(const std::string& ngword_id)
{
	// 未実装
	printf("%s not implemented\n", __PRETTY_FUNCTION__);
	return false;
}

// NG ワード一覧を表示する
bool
NGWord::CmdList()
{
	if (!ReadFile()) {
		return false;
	}

	for (const auto& ng : ngwords) {
		auto id = ng["id"].get<int>();
		auto word = ng["ngword"].get<std::string>();
		auto user = ng["user"].get<std::string>();

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

void
test_NGWord_ReadFile()
{
	printf("%s\n", __func__);

	autotemp filename("a.json");
	bool r;

	{
		// ファイルがない場合
		NGWord ng(filename);
		r = ng.ReadFile();
		xp_eq(true, r);
	}
	{
		// ファイルがあって空の場合
		NGWord ng(filename);
		FileWriteAllText(filename, "");
		r = ng.ReadFile();
		xp_eq(true, r);
	}
	{
		// ["ngword_list"] がない場合
		NGWord ng(filename);
		FileWriteAllText(filename, "{ \"a\": true }");
		r = ng.ReadFile();
		xp_eq(false, r);
	}
	{
		// ["ngword_list"] があって空の場合
		NGWord ng(filename);
		FileWriteAllText(filename, "{ \"ngword_list\": [] }");
		r = ng.ReadFile();
		xp_eq(true, r);
		xp_eq(0, ng.ngwords.size());
	}
}

void
test_Parse()
{
	printf("%s\n", __func__);

	std::vector<std::pair<std::string, std::string>> table = {
		// src	期待値 JSON のうち可変部分ダンプ
		{ "a",	R"("type":"normal")" },
		{ "%LIVE,Mon,00:01,23:59,a,a",
		  R"( "type":"%LIVE","wday":1,"start":1,"end1":1439,"end2":-1 )" },
		{ "%LIVE,Tue,00:00,24:01,a,a",
		  R"( "type":"%LIVE","wday":2,"start":0,"end1":1440,"end2":1 )" },
		{ "%DELAY,1,a,a",	R"( "type":"%DELAY","delay":1,"ngtext":"a,a" )" },
		{ "%DELAY,2d,a,a",	R"( "type":"%DELAY","delay":48,"ngtext":"a,a" )" },
		{ "%RT,1",			R"( "type":"%RT","rtnum":1 )" },
		{ "%SOURCE,a,a",	R"( "type":"%SOURCE","source":"a,a" )" },
		// XXX 異常系をもうちょっとやったほうがいい
	};
	NGWord ngword("");
	for (const auto& a : table) {
		const auto& src = a.first;
		const auto& expstr = a.second;

		// 期待するJson
		Json exp = Json::parse("{" + expstr + "}");
		exp["nguser"] = "u";
		exp["ngword"] = src;
		// 入力
		Json ng;
		ng["nguser"] = "u";
		ng["ngword"] = src;
		// 検査 (仕方ないので一つずつやる)
		auto act = ngword.Parse(ng);
		xp_eq(exp["nguser"], act["nguser"].get<std::string>(), src);
		xp_eq(exp["ngword"], act["ngword"].get<std::string>(), src);
		auto type = exp["type"];
		xp_eq(type, act["type"].get<std::string>(), src);
		if (type == "%LIVE") {
			xp_eq(exp["wday"], act["wday"].get<int>(), src);
			xp_eq(exp["start"], act["start"].get<int>(), src);
			xp_eq(exp["end1"], act["end1"].get<int>(), src);
			xp_eq(exp["end2"], act["end2"].get<int>(), src);
		} else if (type == "%DELAY") {
			xp_eq(exp["delay"], act["delay"].get<int>(), src);
			xp_eq(exp["ngtext"], act["ngtext"].get<std::string>(), src);
		} else if (type == "%RT") {
			xp_eq(exp["rtnum"], act["rtnum"].get<int>(), src);
		} else if (type == "%SOURCE") {
			xp_eq(exp["source"], act["source"].get<std::string>(), src);
		} else if (type == "normal") {
			// 追加パラメータなし
		} else {
			xp_fail(src);
		}
	}
}

void
test_NGWord()
{
	test_NGWord_ReadFile();
	test_Parse();
}
#endif
