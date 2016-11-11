/*
 * Copyright (C) 2014-2016 Tetsuya Isaki
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

using ULib;

public class NGStatus
{
	public bool match;
	public string screen_name;
	public string name;
	public string time;
	public string ngword;
}

public class NGWord
{
	public string Filename;
	public Array<ULib.Json> ngwords;

	public NGWord(string filename)
	{
		Filename = filename;
		ngwords = new Array<ULib.Json>();
	}

	// NG ワードをファイルから読み込む
	public void read_file()
	{
		// ファイルがないのは構わない
		if (FileUtils.test(Filename, FileTest.EXISTS) == false) {
			return;
		}

		try {
			var file = Json.FromString(FileReadAllText(Filename));
			if (file.Has("ngword_list")) {
				// 簡単にチェック
				var ngword_list = file.GetJson("ngword_list");
				if (ngword_list.IsArray == false) {
					stderr.printf(@"NGWord.read_file: Error: "
						+ "ngword file broken\n");
					Process.exit(1);
				}
				ngwords = ngword_list.AsArray;
			}
		} catch (Error e) {
			stderr.printf("NGWord.read_file: Warning: ngword ignored: "
				+ @"$(e.message)\n");
		}
	}

	// NG ワードをファイルに保存する
	public void write_file()
	{
		// 再構成
		var ngword_list = new Json.Array(ngwords);
		var rootdict = new Dictionary<string, ULib.Json>();
		rootdict.set("ngword_list", ngword_list);
		var root = new Json.Object(rootdict);

		try {
			FileWriteAllText(Filename, root.to_string());
		} catch (Error e) {
			stderr.printf(@"NGWord.write_file: Error: $(e.message) f=$(Filename)\n");
		}
	}

	// NG ワードと照合し、結果を NGStatus で返す。
	// 一致したら match = true で、他のすべてのパラメータを埋めて返す。
	// 一致しなければ match = false で、他のパラメータは不定で返す。
	public NGStatus match(ULib.Json status)
	{
		var ngstat = new NGStatus();

		ULib.Json user = null;	// マッチしたユーザ
		for (int i = 0; i < ngwords.length; i++) {
			var ng = ngwords.index(i);

			var ng_user = ng.GetString("user");
			if (status.Has("retweeted_status")) {
				var s = status.GetJson("retweeted_status");

				if (ng_user == "") {
					// ユーザ指定がなければ、RT先本文を比較
					if (match_main(ng, s)) {
						user = s.GetJson("user");
					}
				} else {
					// ユーザ指定があって、RT元かRT先のユーザと一致すれば
					// RT先本文を比較。ただしユーザ情報はマッチしたほう。
					if (match_user(ng_user, status)) {
						if (match_main_rt(ng, s)) {
							user = status.GetJson("user");
						}
					} else if (match_user(ng_user, s)) {
						if (match_main(ng, s)) {
							user = s.GetJson("user");
						}
					}
				}
			} else {
				// RT でないステータス
				// ユーザ指定がないか、あって一致すれば、本文を比較
				if (ng_user == "" || match_user(ng_user, status)) {
					if (match_main(ng, status)) {
						user = status.GetJson("user");
					}
				}
			}

			if (user != null) {
				ngstat.match = true;
				ngstat.screen_name = user.GetString("screen_name");
				ngstat.name = user.GetString("name");
				ngstat.time = formattime(status);
				ngstat.ngword = ng.GetString("ngword");
				return ngstat;
			}
		}

		ngstat.match = false;
		return ngstat;
	}

	// ツイート status がユーザ ng_user のものか調べる。
	// ng_user は "id:<numeric_id>" か "@<screen_name>" 形式。
	private bool match_user(string ng_user, ULib.Json status)
	{
		var u = status.GetJson("user");

		if (ng_user.has_prefix("id:")) {
			var ng_user_id = ng_user.substring(3);
			if (ng_user_id == u.GetString("id_str")) {
				return true;
			}
		}
		if (ng_user.has_prefix("@")) {
			var ng_screen_name = ng_user.substring(1);
			if (ng_screen_name == u.GetString("screen_name")) {
				return true;
			}
		}

		return false;
	}

	// status の本文その他を NGワード ng と照合する。
	// マッチしたかどうかを返す。
	public bool match_main(ULib.Json ng, ULib.Json status)
	{
		var ngword = ng.GetString("ngword");

		// 生実況 NG
		if (ngword.has_prefix("%LIVE,")) {
			var tmp = ngword.split(",", 5);
			// 曜日と時刻2つを取り出す
			var wday  = my_strptime(tmp[1], "%a");
			var start = my_strptime(tmp[2], "%R");
			var end1  = my_strptime(tmp[3], "%R");
			var end2  = -1;
			if (end1 > 1440) {
				end2 = end1 - 1440;
				end1 = 1440;
			}

			// 発言時刻
			var dt = get_datetime(status).to_local();
			var tmwday = dt.get_day_of_week();
			if (tmwday == 7) {
				tmwday = 0;
			}
			var tmmin = dt.get_hour() * 60 + dt.get_minute();

			// 指定曜日の時間の範囲内ならアウト
			if (tmwday == wday && start <= tmmin && tmmin < end1) {
				return true;
			}
			// 終了時刻が24時を越える場合は、越えたところも比較
			if (end2 != -1) {
				wday = (wday + 1) % 7;
				if (tmwday == wday && 0 <= tmmin && tmmin < end2) {
					return true;
				}
			}
			return false;
		}

		// クライアント名
		if (ngword.has_prefix("%SOURCE,")) {
			var tmp = ngword.split(",", 2);
			if (tmp.length > 1 && tmp[1] != null) {
				var match = tmp[1];
				if (match in status.GetString("source")) {
					return true;
				}
			}
			return false;
		}

		// 単純ワード比較
		try {
			string text;
			if (status.Has("full_text")) {
				text = status.GetString("full_text");
			} else {
				text = status.GetString("text");
			}
			var regex = new Regex(ngword, RegexCompileFlags.DOTALL);
			if (regex.match(text)) {
				return true;
			}
		} catch (RegexError e) {
			stderr.printf("Regex failed: %s\n", e.message);
		}

		return false;
	}

	// status の本文その他を NG ワード ng と照合する。
	// リツイートメッセージ用。
	private bool match_main_rt(ULib.Json ng, ULib.Json status)
	{
		// まず通常比較
		if (match_main(ng, status)) {
			return true;
		}

		// 名前も比較
		var user = status.GetJson("user");
		var ngword = ng.GetString("ngword");
		Regex regex;
		try {
			regex = new Regex(ngword, RegexCompileFlags.CASELESS);
		} catch (RegexError e) {
			stderr.printf("Regex failed: %s\n", e.message);
			return false;
		}
		if (regex.match(user.GetString("screen_name"))) {
			return true;
		}
		if (regex.match(user.GetString("name"))) {
			return true;
		}

		return false;
	}

	// NGワードを追加する
	public void cmd_add(string word, string user)
	{
		read_file();

		// もっとも新しい ID を探す (int が一周することはないだろう)
		var new_id = 0;
		for (int i = 0; i < ngwords.length; i++) {
			var ng = ngwords.index(i);
			var id = ng.GetInt("id");

			if (id > new_id) {
				new_id = id;
			}
		}
		new_id++;

		var dict = new Dictionary<string, ULib.Json>();
		dict.set("id", new Json.Number(@"$(new_id)"));
		dict.set("ngword", new Json.String(word));
		dict.set("user", new Json.String(user ?? ""));
		var obj = new Json.Object(dict);

		ngwords.append_val(obj);
		stdout.printf(@"id $(new_id) added\n");

		write_file();
	}

	// NGワードを削除する
	public void cmd_del(string ngword_id)
	{
		read_file();

		var opt_id = int.parse(ngword_id);
		var removed = false;

		for (int i = 0; i < ngwords.length; i++) {
			var ng = ngwords.index(i);
			var id = ng.GetInt("id");

			if (opt_id == id) {
				ngwords.remove_index(i);
				removed = true;
				break;
			}
		}

		if (removed == false) {
			stdout.printf(@"id $(opt_id) not found\n");
			return;
		}

		stdout.printf(@"id $(opt_id) removed\n");
		write_file();
	}

	// NGワード一覧を表示する
	public void cmd_list()
	{
		read_file();

		for (int i = 0; i < ngwords.length; i++) {
			var ng = ngwords.index(i);
			var id = ng.GetInt("id");
			var word = ng.GetString("ngword");
			var user = ng.GetString("user");

			stdout.printf("%d\t%s", id, word);
			if (user != "") {
				stdout.printf("\t%s", user);
			}
			stdout.printf("\n");
		}
	}
}
