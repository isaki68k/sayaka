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
#include "subr.h"
#include "JsonInc.h"

// 現在時刻を返す、はずだがここではテスト用に固定時刻を返す
time_t
GetUnixTime()
{
	// 固定の時刻を返す (2009/11/18 18:54:12)
	return 1258538052;
}

static void
test_format_time()
{
	printf("%s\n", __func__);

	// テスト中は Now() が固定時刻を返す
	std::vector<std::pair<time_t, std::string>> table = {
		// 入力時刻						期待値
		{ 1258538052,					"18:54:12" },		// 同時刻
		{ 1258538052 - 1,				"18:54:11" },		// 同日過去
		{ 1258538052 + 1,				"18:54:13" },		// 未来
		{ 1258469999,					"11/17 23:59:59" },	// 前日
		{ 1258538052 - 86400 * 190,		"05/12 18:54:12" },	// 半年以上前
		{ 1258538052 - 86400 * 322,		"2008/12/31 18:54" },	// 去年
	};
	for (const auto& a : table) {
		const auto& inp = std::get<0>(a);
		const auto& exp = std::get<1>(a);

		auto actual = format_time(inp);
		xp_eq(exp, actual, exp);
	}
}

static void
test_twitter_get_time()
{
	printf("%s\n", __func__);

	std::vector<std::pair<std::string, time_t>> table = {
		{ R"( "timestamp_ms":"1258538052000" )",				1258538052 },
		{ R"( "created_at":"Wed Nov 18 09:54:12 +0000 2009" )", 1258538052 },
	};
	for (const auto& a : table) {
		auto& src = a.first;
		time_t exp = a.second;

		Json json = Json::parse("{" + src + "}");
		auto actual = twitter_get_time(json);
		xp_eq(exp, actual, src);
	}
}

static void
test_DecodeISOTime()
{
	printf("%s\n", __func__);

	std::vector<std::pair<std::string, time_t>> table = {
		{ "2009-11-18T09:54:12Z",		1258538052 },
		{ "2009-11-18T18:54:12+0900",	1258538052 },
		{ "2009-11-18T18:54:12+09:00",	1258538052 },	// コロンもあり
		{ "2009-11-18T08:24:12-0130",	1258538052 },	// TZが負で、分あり
		{ "2009-11-18T09:54:12.01234Z",	1258538052 },	// 小数部何桁でも可

		{ "2009-11-18T00:00:00",		0 },	// timezone がない
		{ "2009-11-18T00:00:00.Z",		0 },	// 小数部がない
	};
	for (const auto& a : table) {
		auto& src = a.first;
		time_t exp = a.second;

		auto actual = DecodeISOTime(src);
		xp_eq(exp, actual, src);
	}
}

static void
test_my_strptime()
{
	printf("%s\n", __func__);

	std::vector<std::tuple<std::string, std::string, int>> table = {
		{ "%a",	"Sun",	0 },
		{ "%a", "mon",	1 },
		{ "%a", "tue",	2 },
		{ "%a", "WED",	3 },
		{ "%a", "THU",	4 },
		{ "%a", "fri",	5 },
		{ "%a", "sAT",	6 },
		{ "%a", "",		-1 },

		{ "%R", "00:00",	0 },
		{ "%R", "00:01",	1 },
		{ "%R", "01:02",	62 },
		{ "%R", "23:59",	1439 },
		{ "%R", "24:01",	1441 },
		{ "%R",	"00:01:02",	-1 },
		{ "%R",	"00",		-1 },
		{ "%R", "-1:-1",	-1 },
		{ "%R",	"02:",		-1 },
		{ "%R",	":",		-1 },
		{ "%R", "0:2",		2 },	// 悩ましいが弾くほどでもないか
	};
	for (const auto& a : table) {
		const auto& fmt = std::get<0>(a);
		const auto& buf = std::get<1>(a);
		int exp = std::get<2>(a);

		auto actual = my_strptime(buf, fmt);
		xp_eq(exp, actual, fmt + "," + buf);
	}
}

static void
test_CRC32()
{
	printf("%s\n", __func__);

	std::vector<std::tuple<std::string, uint32>> table = {
		{ "hoge", 0x4b775151 },
	};
	for (const auto& a : table) {
		const auto& input = std::get<0>(a);
		uint32 exp = std::get<1>(a);

		auto actual = CRC32(input);
		xp_eq_x32_(__FILE__, __LINE__, __func__, exp, actual, input);
	}
}

void
test_subr()
{
	test_format_time();
	test_twitter_get_time();
	test_DecodeISOTime();
	test_my_strptime();
	test_CRC32();
}
