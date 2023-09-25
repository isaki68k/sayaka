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

#include "subr.h"
#include "StringUtil.h"

//
// 雑多なサブルーチン
//

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
time_t
GetUnixTime()
{
	return time(NULL);
}

std::string
formattime(const Json& obj)
{
	char buf[64];

	// 現在時刻
	time_t now = GetUnixTime();
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
		unixtime = (time_t)(stou64def(timestamp_ms, 0) / 1000);
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
	int mday = stou32def(w[2], 1);
	auto timestr = w[3];
	int year = stou32def(w[5], 1900);

	static const std::string monnames = "JanFebMarAprMayJunJulAugSepOctNovDec";
	int mon0 = monnames.find(monname);
	mon0 = (mon0 / 3);

	auto t = Split(timestr, ":");
	int hour = stou32def(t[0], 0);
	int min  = stou32def(t[1], 0);
	int sec  = stou32def(t[2], 0);

	struct tm tm;
	memset(&tm, 0, sizeof(tm));
	tm.tm_year = year - 1900;
	tm.tm_mon  = mon0;
	tm.tm_mday = mday;
	tm.tm_hour = hour;
	tm.tm_min  = min;
	tm.tm_sec  = sec;
	// タイムゾーン欄 (+0000) は常に 0 っぽいので、他は対応してない
	// XXX C++20 になったらまた考える
	return timegm(&tm);
}

// ISO 形式の時刻文字列を Unix time に変換する。
time_t
DecodeISOTime(const std::string& str)
{
	const char *s = &str[0];
	char *end;
	struct tm tm;
	int zh;
	int zm;

	memset(&tm, 0, sizeof(tm));

	int year = stou32def(s, -1, &end);
	if (year < 0 || *end != '-') return 0;
	tm.tm_year = year - 1900;
	s = end + 1;

	int mon = stou32def(s, -1, &end);
	if (mon < 0 || mon > 99 || *end != '-') return 0;
	tm.tm_mon = mon - 1;
	s = end + 1;

	tm.tm_mday = stou32def(s, -1, &end);
	if (tm.tm_mday < 0 || tm.tm_mday > 99 || *end != 'T') return 0;
	s = end + 1;

	tm.tm_hour = stou32def(s, -1, &end);
	if (tm.tm_hour < 0 || tm.tm_hour > 99 || *end != ':') return 0;
	s = end + 1;

	tm.tm_min = stou32def(s, -1, &end);
	if (tm.tm_min < 0 || tm.tm_min > 99 || *end != ':') return 0;
	s = end + 1;

	tm.tm_sec = stou32def(s, -1, &end);
	if (tm.tm_sec < 0 || tm.tm_sec > 99) return 0;

	// 秒の後ろは [.\d+][<timezone>]
	if (*end == '.') {
		s = end + 1;
		int frac = stou32def(s, -1, &end);
		if (frac < 0) return 0;
	}
	// <timezone> は Z か (+|-)(\d\d:?\d\d)
	if (*end == 'Z') {
		end++;
		zh = 0;
		zm = 0;
	} else if (*end == '+' || *end == '-') {
		int sign = (*end == '+') ? 1 : -1;
		s = end + 1;
		int z = stou32def(s, -1, &end);
		if (z < 0) return 0;
		if (end == s + 2) {
			zh = z;
			if (*end != ':') {
				return 0;
			}
			s = end + 1;
			zm = stou32def(s, -1, &end);
			if (zm < 0) return 0;
			if (end != s + 2) return 0;
		} else if (end == s + 4) {
			zh = z / 100;
			zm = z % 100;
		} else {
			return 0;
		}
		zh *= sign;
		zm *= sign;
	} else {
		return 0;
	}

	if (*end != '\0') {
		return 0;
	}

	// タイムゾーンを置いといて Unixtime に変換。
	time_t time = timegm(&tm);

	// タイムゾーン分を補正。
	time -= zh * 60 * 60 + zm * 60;

	return time;
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
		int h = stou32def(hhmm[0], -1);
		int m = stou32def(hhmm[1], -1);
		if (h < 0 || m < 0 || h > 99 || m > 59) {
			return -1;
		}
		return (h * 60) + m;
	}

	return -1;
}

// strerror(errno) は Debug() 等のマクロ内から呼ぶことが多いのに
// clang だと errno が再帰展開になるとかで怒られるので、回避のため。
const char *
strerrno()
{
	return strerror(errno);
}
