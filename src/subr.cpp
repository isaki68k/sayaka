/*
 * Copyright (C) 2016-2021 Tetsuya Isaki
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

#include "StringUtil.h"
#include "subr.h"

// 雑多なサブルーチン

// 名前表示用に整形
std::string
formatname(const std::string& text)
{
	std::string rv = unescape(text);
	rv = string_replace(rv, "\r\n", " ");
	rv = string_replace(rv, "\r", " ");
	rv = string_replace(rv, "\n", " ");
	return rv;
}

// ID 表示用に整形
std::string
formatid(const std::string& text)
{
	return "@" + text;
}

// HTML のエスケープを元に戻す
std::string
unescape(const std::string& text)
{
	std::string rv = text;
	rv = string_replace(rv, "&lt;", "<");
	rv = string_replace(rv, "&gt;", ">");
	rv = string_replace(rv, "&amp;", "&");
	return rv;
}

// HTML タグを取り除いた文字列を返す
std::string
strip_tags(const std::string& text)
{
	std::string sb;
	bool intag = false;
	for (const auto& c : text) {
		if (intag) {
			if (c == '>') {
				intag = false;
			}
		} else {
			if (c == '<') {
				intag = true;
			} else {
				sb += c;
			}
		}
	}
	return sb;
}

// 現在時刻を返す
static time_t
Now()
{
#if defined(SELFTEST)
	// 固定の時刻を返す (2009/11/18 18:54:12)
	return 1258538052;
#else
	// 現在時刻を返す
	return time(NULL);
#endif
}

std::string
formattime(const Json& obj)
{
	char buf[64];

	// 現在時刻
	time_t now = Now();
	struct tm ntm;
	localtime_r(&now, &ntm);

	// obj の日時を取得
	time_t dt = get_datetime(obj);
	struct tm dtm;
	localtime_r(&dt, &dtm);

	const char *fmt;
	if (dtm.tm_year == ntm.tm_year && dtm.tm_yday == ntm.tm_yday) {
		// 今日なら時刻のみ
		fmt = "%T";
	} else if (dtm.tm_year == ntm.tm_year) {
		// 昨日以前で今年中なら年を省略 (mm/dd HH:MM:SS)
		// XXX 半年以内ならくらいのほうがいいのか?
		fmt = "%m/%d %T";
	} else {
		// 去年以前なら yyyy/mm/dd HH:MM (秒はもういいだろう…)
		fmt = "%Y/%m/%d %R";
	}
	strftime(buf, sizeof(buf), fmt, &dtm);
	return std::string(buf);
}

// status の日付時刻を返す。
// timestamp_ms があれば使い、なければ created_at を使う。
// 今のところ、timestamp_ms はたぶん新しめのツイート/イベント通知には
// 付いてるはずだが、リツイートされた側は created_at しかない模様。
time_t
get_datetime(const Json& status)
{
	time_t unixtime;

	if (status.contains("timestamp_ms")) {
		// 数値のようにみえる文字列で格納されている
		const auto& timestamp_ms = status.value("timestamp_ms", "0");
		unixtime = (time_t)(std::stol(timestamp_ms) / 1000);
	} else {
		const auto& created_at = status.value("created_at", "");
		unixtime = conv_twtime_to_unixtime(created_at);
	}
	return unixtime;
}

// Twitter 書式の日付時刻から Unixtime を返す。
// "Wed Nov 18 18:54:12 +0000 2009"
time_t
conv_twtime_to_unixtime(const std::string& instr)
{
	if (__predict_false(instr.empty())) {
		return 0;
	}

	auto w = Split(instr, " ");
	auto& monname = w[1];
	int mday = std::stoi(w[2]);
	auto timestr = w[3];
	int year = std::stoi(w[5]);

	static const std::string monnames = "JanFebMarAprMayJunJulAugSepOctNovDec";
	int mon0 = monnames.find(monname);
	mon0 = (mon0 / 3);

	auto t = Split(timestr, ":");
	int hour = std::stoi(t[0]);
	int min  = std::stoi(t[1]);
	int sec  = std::stoi(t[2]);

	struct tm tm;
	memset(&tm, 0, sizeof(tm));
	tm.tm_year = year - 1900;
	tm.tm_mon  = mon0;
	tm.tm_mday = mday;
	tm.tm_hour = hour;
	tm.tm_min  = min;
	tm.tm_sec  = sec;
	// タイムゾーン欄 (+0000) は常に 0 っぽいので、他は対応してない
	return mktime_z(NULL, &tm);
}

// strptime() っぽい俺様版。
// "%a" と "%R" だけ対応し、戻り値は int。
int
my_strptime(const std::string& buf, const std::string& fmt)
{
	if (fmt == "%a") {
		static const std::array<std::string, 7> wdays = {
			"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat",
		};
		for (int i = 0; i < wdays.size(); i++) {
			if (strcasecmp(buf.c_str(), wdays[i].c_str()) == 0) {
				return i;
			}
		}
		return -1;
	}

	if (fmt == "%R") {
		auto hhmm = Split(buf, ":");
		if (hhmm.size() != 2) {
			return -1;
		}
		auto hh = hhmm[0];
		auto mm = hhmm[1];
		if (hh.size() < 1 || hh.size() > 2) {
			return -1;
		}
		if (mm.size() < 1 || mm.size() > 2) {
			return -1;
		}
		auto h = std::stoi(hh);
		auto m = std::stoi(mm);
		if (h < 0 || m < 0) {
			return -1;
		}
		return (h * 60) + m;
	}

	return -1;
}

#if defined(SELFTEST)
#include "test.h"

void
test_formattime()
{
	printf("%s\n", __func__);

	// テスト中は Now() が固定時刻を返す
	std::vector<std::array<std::string, 2>> table = {
		// 入力時刻							期待値
		{ "Wed Nov 18 11:54:12 +0000 2009",	"20:54:12" },			// 同日
		{ "Tue Nov 17 09:54:12 +0000 2009", "11/17 18:54:12" },		// 年内
		{ "Tue Nov 18 11:54:12 +0000 2008", "2008/11/18 20:54" },	// それ以前
	};
	for (const auto& a : table) {
		const auto& inp = a[0];
		const auto& exp = a[1];

		Json json = Json::parse("{\"created_at\":\"" + inp + "\"}");
		auto actual = formattime(json);
		xp_eq(exp, actual, inp);
	}
}

void
test_get_datetime()
{
	printf("%s\n", __func__);

	std::vector<std::pair<std::string, time_t>> table = {
		{ R"( "timestamp_ms":"1234999" )",	1234 },
		{ R"( "created_at":"Wed Nov 18 09:54:12 +0000 2009" )", 1258538052 },
	};
	for (const auto& a : table) {
		auto& src = a.first;
		time_t exp = a.second;

		Json json = Json::parse("{" + src + "}");
		auto actual = get_datetime(json);
		xp_eq(exp, actual, src);
	}
}

void
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

void
test_subr()
{
	test_formattime();
	test_get_datetime();
	test_my_strptime();
}
#endif
