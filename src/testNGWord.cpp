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

#include "test.h"
#include "FileUtil.h"
#include "NGWord.h"
#include "StringUtil.h"
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
		// testname 	ngword		expected
		{ "text_only",	"hell",		true },
		{ "full_only",	"hell",		true },
		{ "full_text",	"hell",		true },
		{ "text_only",	"hellox",	false },
		{ "full_only",	"hellox",	false },
		{ "full_text",	"hellox",	false },

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

		auto actual = ng->MatchText(statuses[testname]);
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

		// QT 本文のみ
		{ "qt1",	"nomatch",		"",			false },
		{ "qt1",	"hello",		"",			true },
		{ "qt1",	"foo",			"",			true },
		{ "qt1",	"seven",		"",			true },
		{ "qt1",	"nomatch",		"@ange",	false },
		{ "qt1",	"hello",		"@ange",	true },
		{ "qt1",	"foo",			"@ange",	true },
		{ "qt1",	"seven",		"@ange",	true },
		{ "qt1",	"nomatch",		"@other",	false },
		{ "qt1",	"hello",		"@other",	false },
		{ "qt1",	"foo",			"@other",	false },
		{ "qt1",	"seven",		"@other",	false },
		{ "qt1",	"nomatch",		"@seven",	false },
		{ "qt1",	"hello",		"@seven",	false },
		{ "qt1",	"foo",			"@seven",	true },
		// XXX これはどうする?
		{ "qt1",	"seven",		"@seven",	true },

		// QT 先が RT
		{ "qt2",	"nomatch",		"",			false },
		{ "qt2",	"hello",		"",			true },
		{ "qt2",	"foo",			"",			true },
		{ "qt2",	"seven",		"",			true },
		{ "qt2",	"nomatch",		"@ange",	false },
		{ "qt2",	"hello",		"@ange",	true },
		{ "qt2",	"foo",			"@ange",	true },
		{ "qt2",	"seven",		"@ange",	true },
		{ "qt2",	"nomatch",		"@other",	false },
		{ "qt2",	"hello",		"@other",	false },
		{ "qt2",	"foo",			"@other",	false },
		{ "qt2",	"seven",		"@other",	false },
		{ "qt2",	"nomatch",		"@seven",	false },
		{ "qt2",	"hello",		"@seven",	false },
		{ "qt2",	"foo",			"@seven",	true },
		// XXX これはどうする?
		{ "qt2",	"seven",		"@seven",	true },
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
		{ "qt1", {		// QT (RTなし)
			{ "text", "abc hello..." },
			{ "extended_tweet", { { "full_text", "abc hello world" } } },
			{ "created_at", "Sun Jan 10 12:20:00 +0000 2021" },
			{ "source", "test client v0" },
			{ "user", { { "id_str", "100" }, { "screen_name", "ange" } } },
			{ "quoted_status", {
				{ "text", "foo bar" },
				{ "created_at", "Sun Jan 10 12:20:00 +0000 2021" },
				{ "source", "other client v0" },
				{ "user", { { "id_str", "101" }, { "screen_name", "seven" } } },
			} },
		} },
		{ "qt2", {		// QT (RTあり)
			{ "text", "abc hello..." },
			{ "extended_tweet", { { "full_text", "abc hello world" } } },
			{ "created_at", "Sun Jan 10 12:20:00 +0000 2021" },
			{ "source", "test client v0" },
			{ "user", { { "id_str", "100" }, { "screen_name", "ange" } } },
			{ "quoted_status", {
				{ "text", "RT: foo bar" },
				{ "created_at", "Sun Jan 10 12:20:00 +0000 2021" },
				{ "source", "test client v0" },
				{ "user", { { "id_str", "100" }, { "screen_name", "ange" } } },
				{ "retweet_count", 3 },
				{ "retweeted_status", {
					{ "text", "foo bar" },
					{ "created_at", "Sun Jan 10 12:20:00 +0000 2021" },
					{ "user", {
						{ "id_str", "101" },
						{ "screen_name", "seven" }
					} },
					{ "retweet_count", 3 },
					{ "source", "other client v0" },
				} },
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
