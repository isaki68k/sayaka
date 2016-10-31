/*
 * sayaka - twitter client
 */
/*
 * Copyright (C) 2016 Tetsuya Isaki
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

// 別に ULib でも何でもないけど、sayaka.vala 側で関数名のみで呼び出したいため
namespace ULib
{
	// 名前表示用に整形
	public static string formatname(string text)
	{
		return unescape(text)
			.replace("\r\n", " ")
			.replace("\r", " ")
			.replace("\n", " ");
	}

	// ID 表示用に整形
	public static string formatid(string text)
	{
		return "@" + text;
	}

	public static string unescape(string text)
	{
		return text
			.replace("&lt;", "<")
			.replace("&gt;", ">")
			.replace("&amp;", "&");
	}

	// $object の日付時刻を表示用に整形して返す。
	// timestamp_ms があれば使い、なければ created_at を使う。
	// 今のところ、timestamp_ms はたぶん新しめのツイート/イベント通知には
	// 付いてるはずだが、リツイートされた側は created_at しかない模様。
	public static string formattime(ULib.Json obj)
	{
		// vala の DateTime はセットする時に UTC かローカルタイムかを
		// 決めたらそれ以降変えられないようなので(?)、
		// 先に now_local() を作っといて、そのローカルタイムから
		// utc_offset を取得しておく…。嘘だと思うけど…。
		DateTime now = new DateTime.now_local();
		int utc_offset = (int)((int64)now.get_utc_offset() / 1000 / 1000);

		// object の日時を取得
		var dt = get_datetime(obj);

		// なぜかワルシャワ時間に対応 :-)
		string time_zone = null;
		if (obj.Has("user")) {
			var user = obj.GetJson("user");
			var zone = user.GetString("time_zone");
			if (zone == "Warsaw") {
				utc_offset = user.GetInt("utc_offset");
				time_zone = zone;
			}
		}

		// dt は UTC で作ったらローカルタイムに出来ないっぽいので
		// ここで時差分を追加してやる? 嘘だろ…。
		dt = dt.add_hours(utc_offset / 3600);

		var sb = new StringBuilder();

		if (dt.format("%F") == now.format("%F")) {
			// 今日なら時刻のみ(HH:MM:SS)
			sb.append(dt.format("%T"));
		} else if (dt.format("%Y") == now.format("%Y")) {
			// 昨日以前で今年中なら年省略(mm/dd HH:MM:SS)
			// XXX 半年以内ならくらいのほうがいいのか?
			sb.append(dt.format("%m/%d %T"));
		} else {
			// 去年以前なら yyyy/mm/dd HH:MM (秒はもういいだろ…)
			sb.append(dt.format("%Y/%m/%d %R"));
		}

		// タイムゾーンがあれば追加
		if (time_zone != null) {
			sb.append("(");
			sb.append(time_zone);
			sb.append(")");
		}

		return sb.str;
	}

	// status の日付時刻を返す。
	// timestamp_ms があれば使い、なければ created_at を使う。
	// 今のところ、timestamp_ms はたぶん新しめのツイート/イベント通知には
	// 付いてるはずだが、リツイートされた側は created_at しかない模様。
	public static DateTime get_datetime(ULib.Json status)
	{
		DateTime dt;

		if (status.Has("timestamp_ms")) {
			// 数値のようにみえる文字列で格納されている
			var timestamp_ms = status.GetString("timestamp_ms");
			var unixtime = int64.parse(timestamp_ms) / 1000;
			dt = new DateTime.from_unix_utc(unixtime);
		} else {
			var created_at = status.GetString("created_at");
			dt = conv_twtime_to_datetime(created_at);
		}
		return dt;
	}

	// twitter 書式の日付時刻から DateTime を作って返す。
	// "Wed Nov 18 18:54:12 +0000 2009"
	public static DateTime conv_twtime_to_datetime(string instr)
	{
		string[] w = instr.split(" ");
		string monname = w[1];
		int mday = int.parse(w[2]);
		string timestr = w[3];
		int year = int.parse(w[5]);

		var mon = "JanFebMarAprMayJunJulAugSepOctNovDec".index_of(monname);
		mon = (mon / 3) + 1;

		string[] t = timestr.split(":");
		int hour = int.parse(t[0]);
		int min  = int.parse(t[1]);
		int sec  = int.parse(t[2]);

		return new DateTime.utc(year, mon, mday, hour, min, (double)sec);
	}

	// strptime() によく似た俺様版。
	// "%a" と "%R" だけ対応。戻り値は int。
	public static int my_strptime(string buf, string fmt)
	{
		if (fmt == "%a") {
			string[] wdays = {
				"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat",
			};

			for (int i = 0; i < wdays.length; i++) {
				var wday = wdays[i];
				if (buf.ascii_casecmp(wday) == 0) {
					return i;
				}
			}
			return -1;
		}

		if (fmt == "%R") {
			var hhmm = buf.split(":");
			if (hhmm.length != 2) {
				return -1;
			}
			var hh = int.parse(hhmm[0]);
			var mm = int.parse(hhmm[1]);
			return (hh * 60) + mm;
		}

		return -1;
	}
}
