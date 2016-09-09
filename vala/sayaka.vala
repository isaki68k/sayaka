/*
 * sayaka - twitter client
 */
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

using System.OS;
using ULib;

class Program
{
	public static int main(string[] args)
	{
		var sayaka = new SayakaMain();
		return sayaka.Main(args);
	}
}

public class MediaInfo
{
	public string target_url;
	public string display_url;
	public int width;

	public MediaInfo(string t, string d, int w = 0)
	{
		target_url = t;
		display_url = d;
		width = w;
	}
}

public class NGStatus
{
	public bool match;
	public string screen_name;
	public string name;
	public string time;
	public string ngword;
}

public class SayakaMain
{
	public const string version = "3.2.1 (2016/04/24)";

	private static Diag diag;

	public const char CAN = '\x18';
	public const char ESC = '\x1b';
	public const string CSI = "\x1b[";

	public const int DEFAULT_FONT_WIDTH  = 7;
	public const int DEFAULT_FONT_HEIGHT = 14;

	public enum Color {
		Username,
		UserId,
		Time,
		Source,

		Retweet,
		Favorite,
		Url,
		Tag,
		Verified,
		Protected,
		NG,
		Max;
	}

	public enum SayakaCmd {
		StreamMode,
		PlayMode,
		TweetMode,
		MutelistMode,
		StreamRelayMode,
		NgwordAdd,
		NgwordDel,
		NgwordList,
		NortlistMode,
		BlocklistMode,
		Version,
		Max;
	}

	public SocketFamily address_family;
	public bool opt_noimg;
	public string sixel_cmd;
	public int color_mode;
	public bool protect;
	public bool debug;
	public int screen_cols;
	public int opt_fontwidth;
	public int opt_fontheight;
	public int fontheight;
	public int fontwidth;
	public int iconsize;
	public int imagesize;
	public int indent_cols;
	public int indent_depth;
	public int max_image_count;		// この列に表示する最大の画像の数
	public int image_count;			// この列に表示している画像の数
	public int image_next_cols;		// この列の次に表示する画像の位置(文字単位)
	public int image_max_rows;		// この列で最大の画像の高さ(文字単位)
	public bool bg_white;
	public string iconv_tocode = "";
	public string[] color2esc = new string[Color.Max];
	public Twitter tw;
	public Dictionary<string, string> blocklist
		= new Dictionary<string, string>();
	public Dictionary<string, string> mutelist
		= new Dictionary<string, string>();
	public Dictionary<string, string> nortlist
		= new Dictionary<string, string>();
	public bool opt_x68k;
	public bool opt_norest;
	public Array<ULib.Json> ngwords;
	public bool opt_evs;
	public bool opt_show_ng;
	public string opt_ngword;
	public string opt_ngword_user;
	public string record_file;
	public string opt_filter;
	public string last_id;			// 直前に表示したツイート
	public int last_id_count;		// 連続回数
	public int last_id_max;			// 連続回数の上限
	public bool in_sixel;			// SIXEL 出力中なら true
	public string ciphers;

	public string basedir;
	public string cachedir;
	public string tokenfile;
	public string ngwordfile;
	public string colormapdir;

	static SayakaMain sayakaMain;

	public SayakaMain()
	{
		sayakaMain = this;
	}

	public int Main(string[] args)
	{
		SayakaCmd cmd = SayakaCmd.StreamMode;

		basedir = Environment.get_home_dir() + "/.sayaka/";
		cachedir    = basedir + "cache";
		tokenfile   = basedir + "token.json";
		ngwordfile  = basedir + "ngword.json";
		colormapdir = basedir;

		address_family = SocketFamily.INVALID;	// UNSPEC がないので代用
		color_mode = 256;
		sixel_cmd = "";
		bg_white = true;
		opt_evs = false;
		opt_show_ng = false;
		opt_x68k = false;
		opt_filter = "";
		last_id = "";
		last_id_count = 0;
		last_id_max = 10;
		ciphers = null;

		for (var i = 1; i < args.length; i++) {
			switch (args[i]) {
			 case "-4":
				address_family = SocketFamily.IPV4;
				break;
			 case "-6":
				address_family = SocketFamily.IPV6;
				break;
			 case "--black":
				bg_white = false;
				break;
			 case "--blocklist":
				cmd = SayakaCmd.BlocklistMode;
				break;
			 case "--ciphers":
				ciphers = args[++i];
				break;
			 case "--color":
				color_mode = int.parse(args[++i]);
				break;
			 case "--debug":
				debug = true;
				Diag.global_trace = true;
				Diag.global_debug = true;
				Diag.global_warn = true;
				break;
			 case "--eucjp":
				iconv_tocode = "euc-jp";
				break;
			 case "--filter":
				opt_filter = args[++i];
				break;
			 case "--font":
				var metric = args[++i].split("x");
				if (metric.length != 2) {
					usage();
				}
				opt_fontwidth = int.parse(metric[0]);
				opt_fontheight = int.parse(metric[1]);
				break;
			 case "--jis":
				iconv_tocode = "iso-2022-jp";
				break;
			 case "--max-cont":
				last_id_max = int.parse(args[++i]);
				break;
			 case "--max-image-cols":
				max_image_count = int.parse(args[++i]);
				if (max_image_count < 1) {
					max_image_count = 0;
				}
				break;
			 case "--mutelist":
				cmd = SayakaCmd.MutelistMode;
				break;
			 case "--ngword-add":
				cmd = SayakaCmd.NgwordAdd;
				opt_ngword = args[++i];
				break;
			 case "--ngword-del":
				cmd = SayakaCmd.NgwordDel;
				opt_ngword = args[++i];
				break;
			 case "--ngword-list":
				cmd = SayakaCmd.NgwordList;
				break;
			 case "--noimg":
				opt_noimg = true;
				break;
			 case "--norest":
				opt_norest = true;
				break;
			 case "--nortlist":
				cmd = SayakaCmd.NortlistMode;
				break;
			 case "--play":
				cmd = SayakaCmd.PlayMode;
				break;
			 case "--post":
				cmd = SayakaCmd.TweetMode;
				break;
			 case "--protect":
				protect = true;
				break;
			 case "--record":
				record_file = args[++i];
				break;
			 case "--relay-server":
				cmd = SayakaCmd.StreamRelayMode;
				break;
			 case "--sixel-cmd":
				sixel_cmd = args[++i];
				break;
			 case "--show-ng":
				opt_show_ng = true;
				break;
			 case "--support-evs":
				opt_evs = true;
				break;
			 case "--token":
			 {
				var path = args[++i];
				if (path == null) {
					usage();
				}
				if (path.has_prefix("/")) {
					tokenfile = path;
				} else {
					tokenfile = basedir + path;
				}
				break;
			 }
			 case "--user":
				opt_ngword_user = args[++i];
				break;
			 case "--version":
				cmd = SayakaCmd.Version;
				break;
			 case "--white":
				bg_white = true;
				break;
			 case "--x68k":
				opt_x68k = true;
				// "--font 8x16 --jis" を指定したのと同じ
				opt_fontwidth = 8;
				opt_fontheight = 16;
				iconv_tocode = "iso-2022-jp";
				break;
			 default:
				usage();
				break;
			}
		}

		diag  = new Diag("SayakaMain");
		diag.Trace("TRACE CHECK");

		diag.Debug(@"tokenfile = $(tokenfile)\n");

		init();

		// コマンド別処理
		switch (cmd) {
		 case SayakaCmd.StreamMode:
			init_stream();
			cmd_stream();
			break;
		 case SayakaCmd.PlayMode:
			init_stream();
			cmd_play();
			break;
		 case SayakaCmd.MutelistMode:
			cmd_mutelist();
			break;
		 case SayakaCmd.StreamRelayMode:
			cmd_userstream_relay(args[0]);
			break;
		 case SayakaCmd.NgwordAdd:
			cmd_ngword_add();
			break;
		 case SayakaCmd.NgwordDel:
			cmd_ngword_del();
			break;
		 case SayakaCmd.NgwordList:
			cmd_ngword_list();
			break;
		 case SayakaCmd.NortlistMode:
			cmd_nortlist();
			break;
		 case SayakaCmd.BlocklistMode:
			cmd_blocklist();
			break;
		 case SayakaCmd.TweetMode:
			cmd_tweet();
			break;
		 case SayakaCmd.Version:
			cmd_version();
			break;
		 default:
			usage();
			break;
		}

		return 0;
	}

	// 初期化
	public void init()
	{
		// ~/.sayaka がなければ作る
		if (FileUtils.test(basedir, FileTest.IS_DIR) == false) {
			var r = Posix.mkdir(basedir, 0755);
			if (r != 0) {
				stdout.printf(@"sayaka: init: mkdir $(basedir) failed.\n");
				Process.exit(1);
			}
			stdout.printf(@"sayaka: init: $(basedir) is created.\n");
		}

		// キャッシュディレクトリを作る
		if (FileUtils.test(cachedir, FileTest.IS_DIR) == false) {
			var r = Posix.mkdir(cachedir, 0755);
			if (r != 0) {
				stdout.printf(@"sayaka: init: mkdir $(cachedir) failed.\n");
				Process.exit(1);
			}
			stdout.printf(@"sayaka: init: $(cachedir) is created.\n");
		}

		// シグナルハンドラを設定
		Posix.@signal(SIGINT, signal_handler);
		Posix.@signal(SIGWINCH, signal_handler);
	}

	// 投稿する
	public void cmd_tweet()
	{
		// 標準入力から受け取る。UTF-8 前提。
		var sb = new StringBuilder();
		while (stdin.eof() == false) {
			var line = stdin.read_line();
			if (line != null) {
				sb.append(line);
				sb.append("\n");
			}
		}
		var text = sb.str.chomp();

		// アクセストークンを取得
		tw = new Twitter();
		get_access_token();

		// 投稿するパラメータを用意
		var options = new Dictionary<string, string>();
		options.AddOrUpdate("status", text);
		options.AddOrUpdate("trim_user", "1");

		// 投稿
		var json = tw.API2Json("POST", Twitter.APIRoot, "statuses/update",
			options);
		if (json.Has("errors")) {
			var errorlist = json.GetArray("errors");
			// エラーが複数返ってきたらどうするかね
			var code = errorlist.index(0).GetInt("code");
			var message = errorlist.index(0).GetString("message");
			stderr.printf(@"statuses/update failed: $(message)($(code))\n");
			Process.exit(1);
		}
		stdout.printf("Posted.\n");
	}

	// ユーザストリームモードのための準備
	public void init_stream()
	{
		// 色の初期化
		init_color();

		// 外部コマンド
		var cmd = new StringBuilder();
		cmd.append(sixel_cmd);
		if (sixel_cmd.has_suffix("img2sixel")) {
			cmd.append(" -S");
			if (color_mode == 2) {
				cmd.append(" -e --quality=low");
			} else if (color_mode <= 16) {
				cmd.append(@" -m $(colormapdir)/colormap$(color_mode).png");
			}
		}
		sixel_cmd = cmd.str;
		if (debug) {
			stdout.printf("sixel_cmd=");
			if (sixel_cmd == "") {
				stdout.printf("<internal>\n");
			} else {
				stdout.printf("%s\n", sixel_cmd);
			}
		}

		// 一度手動で呼び出して桁数を取得
		signal_handler(SIGWINCH);

		// NGワード取得
		read_ngword_file();
	}

	// ユーザストリーム
	public void cmd_stream()
	{
		DataInputStream userStream = null;

		// 古いキャッシュを削除
		if (debug) {
			stdout.printf("Deleting expired cache files...");
			stdout.flush();
		}
		invalidate_cache();
		if (debug) {
			stdout.printf("done\n");
		}

		// アクセストークンを取得
		tw = new Twitter();
		get_access_token();

		if (ciphers != null) {
			tw.SetCiphers(ciphers);
		}

		if (opt_norest == false) {
			// ブロックユーザ取得
			if (debug) {
				stdout.printf("Getting block users list...");
				stdout.flush();
			}
			get_block_list();
			if (debug) {
				stdout.printf("done\n");
			}

			// ミュートユーザ取得
			if (debug) {
				stdout.printf("Getting mute users list...");
				stdout.flush();
			}
			get_mute_list();
			if (debug) {
				stdout.printf("done\n");
			}

			// RT非表示ユーザ取得
			if (debug) {
				stdout.printf("Getting nort users list...");
				stdout.flush();
			}
			get_nort_list();
			if (debug) {
				stdout.printf("done\n");
			}
		}

		stdout.printf("Ready..");
		stdout.flush();

		// ストリーミング開始
		if (opt_filter != "") {
			// --filter 指定があればキーワード検索モード
			diag.Trace("PostAPI call");
			try {
				var dict = new Dictionary<string, string>();
				dict.AddOrUpdate("track", opt_filter);
				userStream = tw.PostAPI(Twitter.PublicAPIRoot,
					"statuses/filter", dict);
			} catch (Error e) {
				stderr.printf("statuses/filter: %s\n", e.message);
				Process.exit(1);
			}
		} else {
			diag.Trace("UserStreamAPI call");
			try {
				userStream = tw.UserStreamAPI("user");
			} catch (Error e) {
				stderr.printf("userstream: %s\n", e.message);
				Process.exit(1);
			}
		}

		stdout.printf("Connected.\n");

		while (true) {
			string line;
			try {
				line = userStream.read_line();
			} catch (Error e) {
				stderr.printf("userstream.read_line: %s\n", e.message);
				Process.exit(1);
			}
			if (showstatus_callback_line(line) == false) {
				break;
			}
		}
	}

	// 再生モード
	public void cmd_play()
	{
		while (true) {
			string line;

			line = stdin.read_line();
			if (showstatus_callback_line(line) == false) {
				break;
			}
		}
	}

	// 中継サーバモード
	public void cmd_userstream_relay(string progname)
	{
		DataInputStream userStream = null;

		// 実行ファイルのあるところへ chdir
		var progdir = Path.get_dirname(progname);
		Posix.chdir(progdir);

		// アクセストークンを取得
		// XXX すでにあることが前提
		tw = new Twitter();
		get_access_token();

		// ストリーミング開始
		try {
			diag.Trace("UserStreamAPI call");
			userStream = tw.UserStreamAPI("user");
		} catch (Error e) {
			stderr.printf("userstream: %s\n", e.message);
			Process.exit(1);
		}

		while (true) {
			string line;
			try {
				line = userStream.read_line();
			} catch (Error e) {
				stderr.printf("userstream.read_line: %s\n", e.message);
				Process.exit(1);
			}

			// 空行がちょくちょく送られてくるようだ
			if (line == "") {
				continue;
			}

			stdout.printf("%s\n", line);
			stdout.flush();
		}
	}

	// アクセストークンを取得する
	public void get_access_token()
	{
		try {
			// ファイルからトークンを取得
			tw.AccessToken.LoadFromFile(tokenfile);
		} catch {
			// なければトークンを取得してファイルに保存
			tw.GetAccessToken();
			if (tw.AccessToken.Token == "") {
				stderr.printf("GIVE UP\n");
				Process.exit(1);
			}
			try {
				tw.AccessToken.SaveToFile(tokenfile);
			} catch {
				stderr.printf("Token save error\n");
				Process.exit(1);
			}
		}
	}

	// 1行を受け取ってから callback に呼ぶまでの共通部分。
	// true でループ継続、false でループ終了。
	// ファイルかソケットかで全部 read_line() が使えてれば
	// こんなことにはならないんだが…。
	public bool showstatus_callback_line(string? line)
	{
		if (line == null) {
			return false;
		}

		// 空行がちょくちょく送られてくるようだ
		if (line == "") {
			diag.Debug("empty line");
			return true;
		}

		var parser = new ULib.JsonParser();
		try {
			var obj = parser.Parse(line);
			TRACE("obj=%p".printf(obj));
			TRACE("obj=%s\n".printf(obj.ToString()));
			showstatus_callback(obj);
		} catch (Error e) {
			stdout.printf("showstatus_callback_line: %s\n", e.message);
			return false;
		}
		return true;
	}

	// 1ツイートを表示するコールバック関数
	// ここではループ中からそのまま呼ばれる
	public void showstatus_callback(ULib.Json obj)
	{
		ULib.Json status = null;

		// obj が元オブジェクト (イベント or メッセージ)

		// 録画
		if (record_file != null) {
			try {
				var f = File.new_for_path(record_file);
				var outputstream = f.append_to(FileCreateFlags.NONE);
				var stream = new DataOutputStream(outputstream);
				stream.put_string(obj.ToString());
				stream.put_string("\n");
			} catch (Error e) {
				// ignore ?
			}
		}

		if (obj.Has("event")) {
			// event => イベント種別
			//			"favorite", "unfavorite", "follow", "unfollow", ...
			// timestamp_ms => イベント発生時刻(UNIXTIME)
			// created_at => イベント発生時刻

			var event = obj.GetString("event");
			switch (event) {
			 case "favorite":
				if (obj.Has("target_object")) {
					status = obj.GetJson("target_object");

					// これだけだと、$status から $object が拾えないので
					// $object をバックリンクしておく。
					status.AsObject.AddOrUpdate("object", obj);
				}
				break;
			 case "follow":
				var time = coloring(formattime(obj), Color.Time);

				var s = obj.GetJson("source");
				var src_userid = coloring(formatid(s.GetString("screen_name")),
					Color.UserId);
				var src_name   = coloring(formatname(s.GetString("name")),
					Color.Username);

				var t = obj.GetJson("target");
				var dst_userid = coloring(formatid(t.GetString("screen_name")),
					Color.UserId);
				var dst_name   = coloring(formatname(t.GetString("name")),
					Color.Username);
				var src = coloring("sayakaちゃんからお知らせ", Color.Source);

				show_icon(s);
				print_(@"$(src_name) $(src_userid) が "
				     + @"$(dst_name) $(dst_userid) をフォローしました。");
				stdout.printf("\n");
				print_(@"$(time) $(src)");
				stdout.printf("\n");
				stdout.printf("\n");
				return;

			 case "mute":
				add_mute_list(obj.GetJson("target"));
				return;

			 case "unmute":
				del_mute_list(obj.GetJson("target"));
				return;

			 default:
				return;
			}
		} else if (obj.Has("text")) {
			// 通常のツイート
			// status はツイートメッセージ
			status = obj;
		} else if (obj.Has("friends")) {
			// 最初に送られてくる friends リストはいらない
			return;
		} else {
			// それ以外の情報はとりあえず無視
			return;
		}

		// ブロックユーザ、ミュートユーザ、RT 非表示のユーザの RT も
		// ストリームには流れてきてしまうので、ここで弾く。
		var id_str = status.GetJson("user").GetString("id_str");
		if (blocklist.ContainsKey(id_str)) {
			return;
		}
		if (mutelist.ContainsKey(id_str)) {
			return;
		}
		if (status.Has("retweeted_status")) {
			// RT があって RT 元ユーザが該当すれば弾く
			if (nortlist.ContainsKey(id_str)) {
				return;
			}

			// RT 先ユーザがブロック/ミュートユーザでも弾く
			var retweeted_status = status.GetJson("retweeted_status");
			id_str = retweeted_status.GetJson("user").GetString("id_str");
			if (blocklist.ContainsKey(id_str)) {
				return;
			}
			if (mutelist.ContainsKey(id_str)) {
				return;
			}
		}

		// NGワード
		var ngstat = match_ngword(status);
		if (ngstat.match) {
			// マッチしたらここで表示
			if (opt_show_ng) {
				var userid = coloring(formatid(ngstat.screen_name), Color.NG);
				var name = coloring(formatname(ngstat.name), Color.NG);
				var time = coloring(ngstat.time, Color.NG);

				var msg = coloring(@"NG:$(ngstat.ngword)", Color.NG);

				print_(@"$(name) $(userid)\n"
				     + @"$(time) $(msg)");
				stdout.printf("\n");
				stdout.printf("\n");
			}
			return;
		}

		showstatus(status, false);
		stdout.printf("\n");
	}

	// 1ツイートを表示
	public void showstatus(ULib.Json status, bool is_quoted)
	{
		ULib.Json obj = status.GetJson("object");

		// RT なら、RT 元を $status、RT先を $s
		ULib.Json s = status;
		bool has_retweet = false;
		if (status.Has("retweeted_status")) {
			s = status.GetJson("retweeted_status");
			has_retweet = true;
		}

		// --protect オプションなら鍵ユーザのツイートを表示しない
		if (protect == true) {
			var match = false;
			var user = status.GetJson("user");
			if (user.GetBool("protected")) {
				match = true;
			} else if (has_retweet) {
				// リツイート先も調べる
				var rt = status.GetJson("retweeted_status");
				user = rt.GetJson("user");
				if (user.GetBool("protected")) {
					match = true;
				}
			}
			// どちらかで一致すれば非表示
			if (match) {
				print_(coloring("鍵垢", Color.NG) + "\n"
					+ coloring(formattime(status), Color.Time));
				stdout.printf("\n");
				return;
			}
		}

		// 簡略表示の判定。QT 側では行わない
		if (is_quoted == false) {
			// 直前のツイートの RT なら簡略表示
			if (has_retweet && last_id == s.GetString("id_str")) {
				if (last_id_count++ < last_id_max) {
					var rtmsg = format_rt_owner(status);
					var rtcnt = format_rt_cnt(s);
					var favcnt = format_fav_cnt(s);
					print_(rtmsg + rtcnt + favcnt);
					stdout.printf("\n");
					return;
				}
			}

			// 直前のツイートのふぁぼなら簡略表示
			if (obj != null && obj.GetString("event") == "favorite"
			 && last_id == status.GetString("id_str")) {
				if (last_id_count++ < last_id_max) {
					var favmsg = format_fav_owner(obj);
					var rtcnt = format_rt_cnt(s);
					var favcnt = format_fav_cnt(s);
					print_(favmsg + rtcnt + favcnt);
					stdout.printf("\n");
					return;
				}
			}

			// 表示確定
			last_id = s.GetString("id_str");
			last_id_count = 0;
		}

		var s_user = s.GetJson("user");
		var userid = coloring(formatid(s_user.GetString("screen_name")),
			Color.UserId);
		var name = coloring(formatname(s_user.GetString("name")),
			Color.Username);
		var src = coloring(unescape(PHP.strip_tags(
			s.GetString("source") + "から")),
			Color.Source);
		var time = coloring(formattime(s), Color.Time);
		var verified = s_user.GetBool("verified")
			? coloring(" ●", Color.Verified)
			: "";
		var protected = s_user.GetBool("protected")
			? coloring(" ■", Color.Protected)
			: "";

		var mediainfo = new Array<MediaInfo>();
		var msg = formatmsg(s, mediainfo);

		show_icon(s_user);
		print_(name + " " + userid + verified + protected);
		stdout.printf("\n");
		print_(msg);
		stdout.printf("\n");

		// picture
		image_count = 0;
		image_next_cols = 0;
		image_max_rows = 0;
		for (var i = 0; i < mediainfo.length; i++) {
			var m = mediainfo.index(i);
			stdout.printf(@"$(CSI)$(indent_cols)C");
			show_photo(m.target_url, m.width, i);
			stdout.printf("\r");
		}

		// コメント付きRT の引用部分
		if (s.Has("quoted_status")) {
			// この中はインデントを一つ下げる
			stdout.printf("\n");
			indent_depth++;
			showstatus(s.GetJson("quoted_status"), true);
			indent_depth--;
		}

		// このステータスの既 RT、既ふぁぼ数
		var rtmsg = format_rt_cnt(s);
		var favmsg = format_fav_cnt(s);
		print_("%s %s%s%s".printf(time, src, rtmsg, favmsg));
		stdout.printf("\n");

		// リツイート元
		if (has_retweet) {
			print_(format_rt_owner(status));
			stdout.printf("\n");
		}

		// ふぁぼ元
		if (obj != null && obj.GetString("event") == "favorite") {
			print_(format_fav_owner(obj));
			stdout.printf("\n");
		}
	}

	// リツイート元通知を整形して返す
	public string format_rt_owner(ULib.Json status)
	{
		var user = status.GetJson("user");
		var rt_time   = formattime(status);
		var rt_userid = formatid(user.GetString("screen_name"));
		var rt_name   = formatname(user.GetString("name"));
		var str = coloring(@"$(rt_time) $(rt_name) $(rt_userid) がリツイート",
			Color.Retweet);
		return str;
	}

	// ふぁぼ元通知を整形して返す
	public string format_fav_owner(ULib.Json obj)
	{
		var user = obj.GetJson("source");
		var fav_time   = formattime(obj);
		var fav_userid = formatid(user.GetString("screen_name"));
		var fav_name   = formatname(user.GetString("name"));
		var str = coloring(@"$(fav_time) $(fav_name) $(fav_userid) がふぁぼ",
			Color.Favorite);
		return str;
	}

	// リツイート数を整形して返す
	public string format_rt_cnt(ULib.Json s)
	{
		var rtcnt = s.GetInt("retweet_count");
		return (rtcnt > 0)
			? coloring(" %dRT".printf(rtcnt), Color.Retweet)
			: "";
	}

	// ふぁぼ数を整形して返す
	public string format_fav_cnt(ULib.Json s)
	{
		var favcnt = s.GetInt("favorite_count");
		return (favcnt > 0)
			? coloring(" %dFav".printf(favcnt), Color.Favorite)
			: "";
	}

	public void print_(string msg)
	{
		string rv;
		rv = make_indent(msg);

		// 置換は formatmsg() 中で行っている

		// 出力文字コードの変換
		if (iconv_tocode != "") {
			if (iconv_tocode == "iso-2022-jp") {
				var sb = new StringBuilder();
				unichar c;
				for (var i = 0; rv.get_next_char(ref i, out c); ) {
					if (0xff61 <= c && c < 0xffa0) {
						sb.append(@"$(ESC)(I");
						sb.append_unichar(c - 0xff60 + 0x20);
						sb.append(@"$(ESC)(B");
					} else {
						sb.append_unichar(c);
					}
				}
				rv = sb.str;
			}

			try {
				string rv2;
				rv2 = convert(rv, -1, iconv_tocode, "utf-8");
				rv = rv2;
			} catch {
				// nop
			}
		}

		stdout.printf("%s", rv);
	}

	// インデントをつける
	public string make_indent(string text)
	{
		// 桁数が分からない場合は何もしない
		if (screen_cols == 0) {
			return text;
		}

		// インデント階層
		var left = indent_cols * (indent_depth + 1);
		string indent = CSI + @"$(left)C";

		// 文字列を分解。
		var textarray = new List<unichar>();
		unichar uni;
		for (var i = 0; text.get_next_char(ref i, out uni); ) {
			if ((  0xe000 <= uni && uni <=   0xf8ff)	// BMP
			 || ( 0xf0000 <= uni && uni <=  0xffffd)	// 第15面
			 || (0x100000 <= uni && uni <= 0x10fffd)) 	// 第16面
			{
				// Private Use Area (外字) をコードポイント形式(?)にする
				var text2 = "<U+%X>".printf(uni);
				for (var j = 0; text2.get_next_char(ref j, out uni); ) {
					textarray.append(uni);
				}
				continue;
			}
			if (uni == 0xfe0e || uni == 0xfe0f) {
				// ここで EVS 文字を抜く。
				// 絵文字セレクタらしいけど、mlterm + sayaka14 フォント
				// だと U+FE0E とかの文字が前の文字に上書き出力されて
				// ぐちゃぐちゃになってしまうので、mlterm が対応するまでは
				// こっちでパッチ対応。
				if (opt_evs == false) {
					continue;
				}
			}

			textarray.append(uni);
		}

		// 1文字ずつ文字幅を数えながら出力用に整形していく
		bool inescape = false;
		StringBuilder newtext = new StringBuilder();
		newtext.append(indent);
		var x = left;
		for (var i = 0; i < textarray.length(); i++) {
			uni = textarray.nth_data(i);
			if (inescape) {
				newtext.append_unichar(uni);
				if (uni == 'm') {
					inescape = false;
				}
			} else {
				if (uni == ESC) {
					newtext.append_unichar(uni);
					inescape = true;
				} else if (uni == '\n') {
					newtext.append_unichar(uni);
					newtext.append(indent);
					x = left;
				} else if (uni.iswide_cjk()
				        || (0x1f000 <= uni && uni <= 0x1ffff))	// 絵文字
				{
					if (x > screen_cols - 2) {
						newtext.append("\n");
						newtext.append(indent);
						x = left;
					}
					newtext.append_unichar(uni);
					x += 2;
				} else {
					newtext.append_unichar(uni);
					x++;
				}
				if (x > screen_cols - 1) {
					newtext.append("\n");
					newtext.append(indent);
					x = left;
				}
			}
		}
		return newtext.str;
	}

	// 名前表示用に整形
	public string formatname(string text)
	{
		return unescape(text)
			.replace("\r\n", " ")
			.replace("\r", " ")
			.replace("\n", " ");
	}

	// ID 表示用に整形
	public string formatid(string text)
	{
		return "@" + text;
	}

	public string unescape(string text)
	{
		return text
			.replace("&lt;", "<")
			.replace("&gt;", ">")
			.replace("&amp;", "&");
	}

	// 色定数
	public const string BOLD		= "1";
	public const string UNDERSCORE	= "4";
	public const string STRIKE		= "9";
	public const string BLACK		= "30";
	public const string RED			= "31";
	public const string GREEN		= "32";
	public const string BROWN		= "33";
	public const string BLUE		= "34";
	public const string MAGENTA		= "35";
	public const string CYAN		= "36";
	public const string WHITE		= "37";
	public const string GRAY		= "90";
	public const string YELLOW		= "93";

	public void init_color()
	{
		string blue;
		string green;
		string username;
		string fav;

		// 黒背景か白背景かで色合いを変えたほうが読みやすい
		if (bg_white) {
			blue = BLUE;
		} else {
			blue = CYAN;
		}

		// ユーザ名。白地の場合は出来ればもう少し暗めにしたい
		if (bg_white && color_mode > 16) {
			username = "38;5;28";
		} else {
			username = BROWN;
		}

		// リツイートは緑色。出来れば濃い目にしたい
		if (opt_x68k) {
			green = "92";
		} else if (color_mode > 16) {
			green = "38;5;28";
		} else {
			green = GREEN;
		}

		// ふぁぼは黄色。白地の場合は出来れば濃い目にしたいが
		// こちらは太字なのでユーザ名ほどオレンジにしなくてもよさげ。
		if (bg_white && color_mode > 16) {
			fav = "38;5;184";
		} else {
			fav = BROWN;
		}

		color2esc[Color.Username]	= username;
		color2esc[Color.UserId]		= blue;
		color2esc[Color.Time]		= GRAY;
		color2esc[Color.Source]		= GRAY;

		color2esc[Color.Retweet]	= @"$(BOLD);$(green)";
		color2esc[Color.Favorite]	= BOLD + ";" + fav;
		color2esc[Color.Url]		= @"$(UNDERSCORE);$(blue)";
		color2esc[Color.Tag]		= blue;
		color2esc[Color.Verified]	= CYAN;
		color2esc[Color.Protected]	= GRAY;
		color2esc[Color.NG]			= @"$(STRIKE);$(GRAY)";
	}

	public string coloring(string text, Color col)
	{
		string rv;

		if (color2esc[col] != null) {
			rv = @"$(CSI)$(color2esc[col])m$(text)$(CSI)0m";
		} else {
			rv = @"Coloring($(text),$(col))";
		}
		return rv;
	}

	// $object の日付時刻を表示用に整形して返す。
	// timestamp_ms があれば使い、なければ created_at を使う。
	// 今のところ、timestamp_ms はたぶん新しめのツイート/イベント通知には
	// 付いてるはずだが、リツイートされた側は created_at しかない模様。
	public string formattime(ULib.Json obj)
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
	public DateTime get_datetime(ULib.Json status)
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
	public DateTime conv_twtime_to_datetime(string instr)
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

	// テキスト整形用のタグ
	public class TextTag
	{
		public int Start;
		public int End;
		public Color Type;
		public string Text;
	
		public TextTag(int start, int end, Color type, string? text = null)
		{
			Start = start;
			End = end;
			Type = type;
			Text = text;
		}

		public int length { get { return End - Start; } }

		public string ToString()
		{
			return @"($(Start), $(End), $(Type))";
		}
	}

	public string formatmsg(ULib.Json s, Array<MediaInfo> mediainfo)
	{
		// 本文
		var text = s.GetString("text");

		// 1文字ずつに分解して配列に
		var utext = new unichar[text.char_count()];
		unichar uni;
		for (var pos = 0, i = 0; text.get_next_char(ref pos, out uni); ) {
			utext[i++] = uni;
		}

		// エンティティを調べる
		var tags = new TextTag[utext.length];
		if (s.Has("entities")) {
			var entities = s.GetJson("entities");
			// ハッシュタグ情報を展開
			var hashtags = entities.GetArray("hashtags");
			for (var i = 0; i < hashtags.length; i++) {
				var t = hashtags.index(i);
				// t->indices[0] … 開始位置、1文字目からなら0
				// t->indices[1] … 終了位置。この1文字前まで
				var indices = t.GetArray("indices");
				var start = indices.index(0).AsInt;
				var end   = indices.index(1).AsInt;
				tags[start] = new TextTag(start, end, Color.Tag);
			}

			// ユーザID情報を展開
			var mentions = entities.GetArray("user_mentions");
			for (var i = 0; i < mentions.length; i++) {
				var t = mentions.index(i);
				var indices = t.GetArray("indices");
				var start = indices.index(0).AsInt;
				var end   = indices.index(1).AsInt;
				tags[start] = new TextTag(start, end, Color.UserId);
			}

			// URL を展開
			var urls = entities.GetArray("urls");
			for (var i = 0; i < urls.length; i++) {
				var t = urls.index(i);
				var indices = t.GetArray("indices");
				var start = indices.index(0).AsInt;
				var end   = indices.index(1).AsInt;

				// url         本文中の短縮 URL (twitterから)
				// display_url 差し替えて表示用の URL (twitterから)
				// expanded_url 展開後の URL (twitterから)
				var url      = t.GetString("url");
				var disp_url = t.GetString("display_url");
				var expd_url = t.GetString("expanded_url");

				// 本文の短縮 URL を差し替える
				string newurl;
				var qid = s.GetString("quoted_status_id_str");
				if (s.Has("quoted_status")
				 && expd_url.contains(qid) == true
				 && text.chomp().has_suffix(url) == true)
				{
					// この場合はコメント付き RT の URL なので取り除く
					newurl = "";
				} else {
					newurl = disp_url;
				}

				tags[start] = new TextTag(start, end, Color.Url, newurl);

				// 外部画像サービスを解析
				var minfo = format_image_url(expd_url, disp_url);
				if (minfo != null) {
					mediainfo.append_val(minfo);
				}
			}
		}

		// メディア情報を展開
		if (s.Has("extended_entities")
		 && s.GetJson("extended_entities").Has("media")) {
			var media = s.GetJson("extended_entities").GetArray("media");
			for (var i = 0; i < media.length; i++) {
				var m = media.index(i);

				// 本文の短縮 URL を差し替える
				var indices = m.GetArray("indices");
				var start = indices.index(0).AsInt;
				var end   = indices.index(1).AsInt;
				tags[start] = new TextTag(start, end, Color.Url,
					m.GetString("display_url"));

				// 画像展開に使う
				//   url         本文中の短縮 URL (twitterから)
				//   display_url 差し替えて表示用の URL (twitterから)
				//   media_url   指定の実ファイル URL (twitterから)
				//   target_url  それを元に実際に使う URL
				//   width       幅指定。ピクセルか割合で
				var disp_url = m.GetString("display_url");
				var media_url = m.GetString("media_url");

				// pic.twitter.com の画像のうち :thumb は縮小ではなく切り抜き
				// なので使わない。:small は縦横比に関わらず横 340px に縮小。
				// 横長なら 340 x (340以下)、縦長なら 340 x (340以上) になって
				// そのままでは縦長写真と横長写真で縮尺が揃わないクソ仕様なので
				// ここでは長辺を基準に 40% に縮小する。
				var small = m.GetJson("sizes").GetJson("small");
				var w = small.GetInt("w");
				var h = small.GetInt("h");
				int width;
				if (h > w) {
					width = (int)((double)w / h * imagesize);
				} else {
					width = imagesize;
				}

				var target_url = @"$(media_url):small";
				var minfo = new MediaInfo(target_url, disp_url, width);
				mediainfo.append_val(minfo);	
			}
		}

		// タグ情報をもとにテキストを整形
		var newtext = new StringBuilder();
		for (var i = 0; i < utext.length; ) {
			if (tags[i] != null) {
				switch (tags[i].Type) {
				 case Color.Tag:
				 case Color.UserId:
					var sb = new StringBuilder();
					for (var j = 0; j < tags[i].length; j++) {
						sb.append_unichar(utext[i + j]);
					}
					newtext.append(coloring(sb.str, tags[i].Type));
					i += tags[i].length;
					break;
				 case Color.Url:
					newtext.append(coloring(tags[i].Text, tags[i].Type));
					i += tags[i].length;
					break;
				}
			} else {
				newtext.append_unichar(utext[i++]);
			}
		}
		text = newtext.str;

		// タグの整形が済んでからエスケープと改行を整形
		text = unescape(text)
			. replace("\r\n", "\n")
			. replace("\r", "\n");

		return text;
	}

	// 外部画像サービス URL を解析した結果を返す
	public MediaInfo? format_image_url(string expd_url, string disp_url)
	{
		MatchInfo m;
		string target;
		int width = 0;

		try {
			if (new Regex("twitpic.com/(\\w+)")
					.match(expd_url, 0, out m)) {
				target = "http://twitpic.com/show/mini/%s".printf(m.fetch(1));

			} else if (new Regex("movapic.com/(pic/)?(\\w+)")
					.match(expd_url, 0, out m)) {
				target = "http://image.movapic.com/pic/t_%s.jpeg"
					.printf(m.fetch(2));

			} else if (new Regex("p.twipple.jp/(\\w+)")
					.match(expd_url, 0, out m)) {
				target = "http://p.twpl.jp/show/thumb/%s".printf(m.fetch(1));

			} else if (new Regex("(.*instagram.com/p/[\\w\\-]+)/?")
					.match(expd_url, 0, out m)) {
				target = "%s/media/?size=t".printf(m.fetch(1));

			} else if (new Regex("shindanmaker.com/pic/s_(\\d+)/(.*)_wct")
					.match(expd_url, 0, out m)) {
				target = "http://pic.shindanmaker.com/s/%s/%s.jpg"
					.printf(m.fetch(1), m.fetch(2));
				width = imagesize;

			} else if (new Regex("\\.(jpg|jpeg|png|gif)$").
					match(expd_url, 0, out m)) {
				target = expd_url;
				width = imagesize;

			} else {
				return null;

			}
		} catch (RegexError e) {
			stderr.printf("%s\n", e.message);
			return null;
		}

		return new MediaInfo(target, disp_url, width);
	}

	// 現在のカーソル位置に user のアイコンを表示。
	// アイコン表示後にカーソル位置を表示前の位置に戻す。
	public void show_icon(ULib.Json user)
	{
		// 改行x3 + カーソル上移動x3 を行ってあらかじめスクロールを発生させ
		// アイコン表示時にスクロールしないようにしてからカーソル位置を保存する
		// (スクロールするとカーソル位置復元時に位置が合わない)
		stdout.printf("\n\n\n" + CSI + "3A" + @"$(ESC)7");

		var screen_name = unescape(user.GetString("screen_name"));
		var image_url = user.GetString("profile_image_url");

		// URLのファイル名部分をキャッシュのキーにする
		var filename = Path.get_basename(image_url);
		var img_file =
			@"icon-$(iconsize)x$(iconsize)-$(screen_name)-$(filename)";

		if (show_image(img_file, image_url, iconsize, -1) == false) {
			stdout.printf("\n\n\n");
		}

		stdout.printf("\r");
		// カーソル位置保存/復元に対応していない端末でも動作するように
		// カーソル位置復元前にカーソル上移動x3を行う
		stdout.printf(CSI + "3A" + @"$(ESC)8");
	}

	// index は画像の番号 (位置決めに使用する)
	public bool show_photo(string img_url, int width, int index)
	{
		string img_file = img_url;
		try {
			Regex regex = new Regex("[:/()? ]");
			img_file = regex.replace(img_url, img_url.length, 0, "_");
		} catch (Error e) {
			stdout.printf(@"show_photo: regex: $(e.message)\n");
		}

		return show_image(img_file, img_url, width, index);
	}

	// 画像をキャッシュして表示
	//  $img_file はキャッシュディレクトリ内でのファイル名
	//  $img_url は画像の URL
	//  $width は画像の幅。ピクセルで指定。0 を指定すると、リサイズせず
	//  オリジナルのサイズ。
	//  $index は添付写真の位置決めに使用する画像番号。-1 なら位置不要。
	// 表示できれば真を返す。
	public bool show_image(string img_file, string img_url, int width,
		int index)
	{
		// CSI."0C" は0文字でなく1文字になってしまうので、必要な時だけ。
		if (indent_depth > 0) {
			var left = indent_cols * indent_depth;
			stdout.printf(@"$(CSI)$(left)C");
		}

		if (opt_noimg) return false;

		var tmp = Path.build_path(Path.DIR_SEPARATOR_S,
			cachedir, img_file);
		img_file = tmp;

		diag.Debug(@"show_image: img_file=$(img_file), img_url=$(img_url)");

		if (sixel_cmd == "") {
			// インデントはあっちで行っている

			return show_image_internal(img_file, img_url, width, index);
		} else {
			return show_image_external(img_file, img_url, width);
		}
	}

	public bool show_image_internal(string img_file, string img_url, int width,
		int index)
	{
		var sx = new SixelConverter();

		try {
			sx.Load(img_file);
		} catch {
			diag.Debug("no cache found");
			try {
				Curl fg = new Curl(img_url);
				fg.Family = address_family;
				var basestream = fg.GET();
				var ms = new MemoryOutputStream.resizable();
				try {
					ms.splice(basestream, 0);
				} catch {
					// ignore
				}
				ms.close();

				// ms のバックエンドバッファの所有権を移す。
				var msdata = ms.steal_data();
				msdata.length = (int)ms.get_data_size();
				var stream = new MemoryInputStream.from_data(msdata, null);

				// イメージファイルそのままをキャッシュ
				try {
					(stream as Seekable).seek(0, SeekType.SET);
					var f = File.new_for_path(img_file);
					var fs = f.replace(null, false, FileCreateFlags.NONE);
					fs.splice(stream, 0);
					fs.close();
				} catch (Error e) {
					stderr.printf("sayaka: %s\n", e.message);
				}

				(stream as Seekable).seek(0, SeekType.SET);
				sx.LoadFromStream(stream);
			} catch {
				return false;
			}
		}

		if (width != 0) {
			sx.ResizeByWidth(width);
		}

		// この画像が占める文字数
		var image_rows = (sx.Height + fontheight - 1) / fontheight;
		var image_cols = (sx.Width + fontwidth - 1) / fontwidth;

		// 表示位置などの計算
		if (index >= 0) {
			var indent = (indent_depth + 1) * indent_cols;
			if ((max_image_count > 0 && image_count >= max_image_count) ||
				(indent + image_next_cols + image_cols >= screen_cols))
			{
				// 指定された枚数を越えるか、画像が入らない場合は折り返す
				stdout.printf("\r");
				stdout.printf(@"$(CSI)$(indent)C");
				image_count = 0;
				image_max_rows = 0;
				image_next_cols = 0;
			} else {
				// 前の画像の横に並べる
				if (image_count > 0) {
					if (image_max_rows > 0) {
						stdout.printf(@"$(CSI)$(image_max_rows)A");
					}
					if (image_next_cols > 0) {
						stdout.printf(@"$(CSI)$(image_next_cols)C");
					}
				}
			}
		}

		// color_modeでよしなに減色する
		if (opt_x68k) {
			sx.SetPaletteX68k();
			sx.DiffuseReduceCustom(sx.FindCustom);
		} else if (color_mode <= 2) {
			sx.SetPaletteGray(2);
			sx.DiffuseReduceGray();
		} else if (color_mode < 8) {
			sx.SetPaletteGray(color_mode);
			sx.DiffuseReduceGray();
		} else if (color_mode < 16) {
			sx.SetPaletteFixed8();
			sx.DiffuseReduceFixed8();
		} else if (color_mode < 256) {
			sx.SetPaletteFixed16();
			sx.DiffuseReduceFixed16();
		} else {
			sx.SetPaletteFixed256();
			sx.DiffuseReduceFixed256();
		}
		in_sixel = true;
		sx.SixelToStream(stdout);
		stdout.flush();
		in_sixel = false;

		if (index >= 0) {
			image_count++;
			image_next_cols += image_cols;

			// カーソル位置は同じ列に表示した画像の中で最長のものの下端に揃える
			if (image_max_rows > image_rows) {
				var down = image_max_rows - image_rows;
				stdout.printf(@"$(CSI)$(down)B");
			} else {
				image_max_rows = image_rows;
			}
		}

		return true;
	}

	// 外部プログラムを起動して画像を表示
	public bool show_image_external(string img_file, string img_url, int width)
	{
		var tmp = @"$(img_file).sixel";
		img_file = tmp;

		string width_opt = "";
		if (width != 0) {
			width_opt = @" -w $(width)";
		}

		FileStream stream;
		stream = FileStream.open(img_file, "r");
		if (stream == null) {
			diag.Debug("no cache found");
			var imgconv = @"$(sixel_cmd)$(width_opt)";
			Posix.system(@"(curl -Lks $(img_url) | "
				+ @"$(imgconv) > $(img_file)) 2> /dev/null");
			stream = FileStream.open(img_file, "r");
		}
		// XXX うーん…この辺
		size_t fsize = 0;
		if (stream == null || (fsize = get_filesize(stream)) == 0) {
			Posix.unlink(img_file);
			return false;
		}

		// ファイルを読んで標準出力に吐き出す
		uint8[] buf = new uint8[fsize];
		stream.read(buf);
		in_sixel = true;
		stdout.write(buf);
		stdout.flush();
		in_sixel = false;

		return true;
	}

	// FileStream からファイルサイズを取得
	public size_t get_filesize(FileStream stream)
	{
		stream.seek(0, FileSeek.END);
		var fsize = stream.tell();
		stream.rewind();
		return fsize;
	}

	// ブロックユーザ一覧の読み込み
	public void get_block_list()
	{
		// ブロックユーザ一覧は一度に全部送られてくるとは限らず、
		// next_cursor{,_str} が 0 なら最終ページ、そうでなければ
		// これを cursor に指定してもう一度リクエストを送る。

		blocklist.Clear();
		var cursor = "-1";

		do {
			var options = new Dictionary<string, string>();
			options["cursor"] = cursor;

			// JSON を取得
			var json = tw.API2Json("GET", Twitter.APIRoot, "blocks/ids",
				options);
			diag.Debug(@"json=|$(json)|");
			if (json == null) {
				stderr.printf("get_block_list failed: json == null\n");
				break;
			}
			if (json.Has("errors")) {
				var errorlist = json.GetArray("errors");
				// エラーが複数返ってきたらどうするかね
				var code = errorlist.index(0).GetInt("code");
				var message = errorlist.index(0).GetString("message");
				stderr.printf(@"get_block_list failed: $(message)($(code))\n");
				Process.exit(1);
			}

			var users = json.GetArray("ids");
			for (var i = 0; i < users.length; i++) {
				var id_str = users.index(i).AsNumber;
				blocklist[id_str] = id_str;
			}

			cursor = json.GetString("next_cursor_str");
			diag.Debug(@"cursor=|$(cursor)|");
		} while (cursor != "0");
	}

	// 取得したブロックユーザの一覧を表示する
	public void cmd_blocklist()
	{
		tw = new Twitter();
		get_access_token();

		get_block_list();

		for (var i = 0; i < blocklist.Count; i++) {
			var kv = blocklist.At(i);
			stdout.printf("%s\n".printf(kv.Key));
		}
	}

	// ミュートユーザ一覧の読み込み
	public void get_mute_list()
	{
		// ミュートユーザ一覧は一度に全部送られてくるとは限らず、
		// next_cursor{,_str} が 0 なら最終ページ、そうでなければ
		// これを cursor に指定してもう一度リクエストを送る。

		mutelist.Clear();
		var cursor = "0";

		do {
			var options = new Dictionary<string, string>();
			if (cursor != "0") {
				options["cursor"] = cursor;
			}

			// JSON を取得
			var json = tw.API2Json("GET", Twitter.APIRoot, "mutes/users/ids",
				options);
			diag.Debug(@"json=|$(json)|");
			if (json == null) {
				stderr.printf("get_mute_list failed: json == null\n");
				break;
			}
			if (json.Has("errors")) {
				var errorlist = json.GetArray("errors");
				// エラーが複数返ってきたらどうするかね
				var code = errorlist.index(0).GetInt("code");
				var message = errorlist.index(0).GetString("message");
				stderr.printf(@"get_mute_list failed: $(message)($(code))\n");
				Process.exit(1);
			}

			var users = json.GetArray("ids");
			for (var i = 0; i < users.length; i++) {
				var id_json = users.index(i);
				var id_str = id_json.AsNumber;
				mutelist[id_str] = id_str;
			}

			cursor = json.GetString("next_cursor_str");
			diag.Debug(@"cursor=|$(cursor)|");
		} while (cursor != "0");
	}

	// ミュートユーザを追加
	public void add_mute_list(Json user)
	{
		var id_str = user.GetString("id_str");
		mutelist[id_str] = id_str;
	}

	// ミュートユーザを削除
	public void del_mute_list(Json user)
	{
		//var id_str = user.GetString("id_str");

		// not yet
		// mutelist.Delete ? (id_str);
	}

	// 取得したミュートユーザの一覧を表示する
	public void cmd_mutelist()
	{
		tw = new Twitter();
		get_access_token();

		get_mute_list();

		for (var i = 0; i < mutelist.Count; i++) {
			var kv = mutelist.At(i);
			stdout.printf("%s\n".printf(kv.Key));
		}
	}

	// RT非表示ユーザ一覧の読み込み
	public void get_nort_list()
	{
		// ミュートユーザ一覧とは違って、リスト一発で送られてくるっぽい。
		// なんであっちこっちで仕様が違うんだよ…。

		nortlist.Clear();

		// JSON を取得
		var json = tw.API2Json("GET", Twitter.APIRoot,
			"friendships/no_retweets/ids");
		diag.Debug(@"json=|$(json)|");
		if (json == null) {
			stderr.printf("get_nort_list failed: json == null\n");
			return;
		}

		if (json.IsArray == false) {
			// どうするかね
			return;
		}

		var users = json.AsArray;
		for (var i = 0; i < users.length; i++) {
			var id_json = users.index(i);
			var id_str = id_json.AsNumber;
			nortlist[id_str] = id_str;
		}
	}

	// 取得した RT 非表示ユーザの一覧を表示する
	public void cmd_nortlist()
	{
		tw = new Twitter();
		get_access_token();

		get_nort_list();

		for (var i = 0; i < nortlist.Count; i++) {
			var kv = nortlist.At(i);
			stdout.printf("%s\n".printf(kv.Key));
		}
	}

	// 古いキャッシュを破棄する
	public void invalidate_cache()
	{
		// アイコンは7日分くらいか
		Posix.system(
			@"find $(cachedir) -name icon-\\* -type f -atime +7 -exec rm {} +");

		// 写真は24時間分くらいか
		Posix.system(
			@"find $(cachedir) -name http\\* -type f -atime +1 -exec rm {} +");
	}

	// NG ワードをファイルから読み込む
	public void read_ngword_file()
	{
		ngwords = new Array<ULib.Json>();

		// ファイルがないのは構わない
		if (FileUtils.test(ngwordfile, FileTest.EXISTS) == false) {
			return;
		}

		try {
			var file = Json.FromString(FileReadAllText(ngwordfile));
			if (file.Has("ngword_list")) {
				// 簡単にチェック
				var ngword_list = file.GetJson("ngword_list");
				if (ngword_list.IsArray == false) {
					stderr.printf(@"Error: ngword file broken\n");
					Process.exit(1);
				}
				ngwords = ngword_list.AsArray;
			}
		} catch (Error e) {
			stderr.printf(@"Warning: ngword ignored: $(e.message)\n");
		}
	}

	// NG ワードをファイルに保存する
	public void write_ngword_file()
	{
		// 再構成
		var ngword_list = new Json.Array(ngwords);
		var rootdict = new Dictionary<string, ULib.Json>();
		rootdict.set("ngword_list", ngword_list);
		var root = new Json.Object(rootdict);

		try {
			FileWriteAllText(ngwordfile, root.to_string());
		} catch (Error e) {
			stderr.printf(@"write_ngword_file: Error: $(e.message)\n");
		}
	}

	// NG ワードと照合し、結果を NGStatus で返す。
	// 一致したら match = true で、他のすべてのパラメータを埋めて返す。
	// 一致しなければ match = false で、他のパラメータは不定で返す。
	public NGStatus match_ngword(ULib.Json status)
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
					if (match_ngword_main(ng, s)) {
						user = s.GetJson("user");
					}
				} else {
					// ユーザ指定があって、RT元かRT先のユーザと一致すれば
					// RT先本文を比較。ただしユーザ情報はマッチしたほう。
					if (match_ngword_user(ng_user, status)) {
						if (match_ngword_main_rt(ng, s)) {
							user = status.GetJson("user");
						}
					} else if (match_ngword_user(ng_user, s)) {
						if (match_ngword_main(ng, s)) {
							user = s.GetJson("user");
						}
					}
				}
			} else {
				// RT でないステータス
				// ユーザ指定がないか、あって一致すれば、本文を比較
				if (ng_user == "" || match_ngword_user(ng_user, status)) {
					if (match_ngword_main(ng, status)) {
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
	public bool match_ngword_user(string ng_user, ULib.Json status)
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
	public bool match_ngword_main(ULib.Json ng, ULib.Json status)
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
		}

		// 単純ワード比較
		try {
			var regex = new Regex(ngword, RegexCompileFlags.DOTALL);
			if (regex.match(status.GetString("text"))) {
				return true;
			}
		} catch (RegexError e) {
			stderr.printf("Regex failed: %s\n", e.message);
		}

		return false;
	}

	// status の本文その他を NG ワード ng と照合する。
	// リツイートメッセージ用。
	public bool match_ngword_main_rt(ULib.Json ng, ULib.Json status)
	{
		// まず通常比較
		if (match_ngword_main(ng, status)) {
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

	// strptime() によく似た俺様版。
	// "%a" と "%R" だけ対応。戻り値は int。
	private int my_strptime(string buf, string fmt)
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

	// NGワードを追加する
	public void cmd_ngword_add()
	{
		read_ngword_file();

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
		dict.set("ngword", new Json.String(opt_ngword));
		dict.set("user", new Json.String(opt_ngword_user ?? ""));
		var obj = new Json.Object(dict);

		ngwords.append_val(obj);
		stdout.printf(@"id $(new_id) added\n");

		write_ngword_file();
	}

	// NGワードを削除する
	public void cmd_ngword_del()
	{
		read_ngword_file();

		var opt_id = int.parse(opt_ngword);
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

		if (removed) {
			stdout.printf(@"id $(opt_id) removed\n");
		} else {
			stdout.printf(@"id $(opt_id) not found\n");
		}

		write_ngword_file();
	}

	// NGワード一覧を表示する
	public void cmd_ngword_list()
	{
		read_ngword_file();

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


	public static void signal_handler(int signo)
	{
		sayakaMain.signal_handler_2(signo);
	}

	public void signal_handler_2(int signo)
	{
		switch (signo) {
		 case SIGINT:
			// SIXEL 出力中なら中断する (CAN + ST)
			if (in_sixel) {
				stdout.printf("%c%c%c", CAN, ESC, '\\');
				stdout.flush();
			} else {
				Process.exit(0);
			}
			break;

		 case SIGWINCH:
			int ws_cols = 0;
			int ws_width = 0;
			int ws_height = 0;

			winsize ws = winsize();
			var r = ioctl.TIOCGWINSZ(Posix.STDOUT_FILENO, out ws);
			if (r != 0) {
				stdout.printf("TIOCGWINSZ failed.\n");
			} else {
				ws_cols = ws.ws_col;

				if (ws.ws_col != 0) {
					ws_width = ws.ws_xpixel / ws.ws_col;
				}
				if (ws.ws_row != 0) {
					ws_height = ws.ws_ypixel / ws.ws_row;
				}
			}

			var msg_cols = "";
			var msg_width = "";
			var msg_height = "";

			// 画面幅は常に更新
			if (ws_cols > 0) {
				screen_cols = ws_cols;
				msg_cols = " (from ioctl)";
			} else {
				screen_cols = 0;
				msg_cols = " (not detected)";
			}
			// フォント幅と高さは指定されてない時だけ取得した値を使う
			var use_default_font = false;
			if (opt_fontwidth > 0) {
				fontwidth = opt_fontwidth;
			} else {
				if (ws_width > 0) {
					fontwidth = ws_width;
					msg_width = " (from ioctl)";
				} else {
					fontwidth = DEFAULT_FONT_WIDTH;
					msg_width = " (DEFAULT)";
					use_default_font = true;
				}
			}
			if (opt_fontheight > 0) {
				fontheight = opt_fontheight;
			} else {
				if (ws_height > 0) {
					fontheight = ws_height;
					msg_height = " (from ioctl)";
				} else {
					fontheight = DEFAULT_FONT_HEIGHT;
					msg_height = " (DEFAULT)";
					use_default_font = true;
				}
			}
			if (use_default_font) {
				stdout.printf("sayaka: Fontsize not detected. "
					+ @"Application default $(fontwidth)x$(fontheight) "
					+ "is used\n");
			}

			// フォントの高さからアイコンサイズを決定する。
			//
			// SIXEL 表示後のカーソル位置は、
			// o xterm 等では SIXEL 最終ラスタを含む行の次の行、
			// o VT382 等では SIXEL 最終ラスタの次のラスタを含む行
			// になる。
			// アイコンは2行以上3行未満にする必要があり、
			// かつ6の倍数だと SIXEL 的に都合がいい。
			iconsize  = ((fontheight * 3 - 1) / 6) * 6;
			// 画像サイズにはアイコンのような行制約はないので計算は適当。
			// XXX まだ縦横について考慮してない
			imagesize = ((fontheight * 9 - 1) / 6) * 6;

			// そこからインデント幅を決定
			indent_cols = ((int)(iconsize / fontwidth)) + 1;

			if (debug) {
				stdout.printf("screen columns=%d%s\n", screen_cols, msg_cols);
				stdout.printf("font height=%d%s\n", fontheight, msg_height);
				stdout.printf("font width=%d%s\n", fontwidth, msg_width);
				stdout.printf("iconsize=%d\n", iconsize);
				stdout.printf("indent columns=%d\n", indent_cols);
				stdout.printf("imagesize=%d\n", imagesize);
			}
			break;
		 default:
			break;
		}
	}

	public void cmd_version()
	{
		stdout.printf(@"sayaka.vala $(version)\n");
	}

	public void usage()
	{
		stdout.printf(
"""usage: sayaka [<options>...]
	--color <n> : color mode { 2 .. 256 }. default 256.
	--font <w>x<h> : font width x height. default 7x14.
	--filter <keyword>
	--white / --black : darken/lighten the text color. (default: --white)
	--noimg
	--jis
	--eucjp
	--play : read JSON from stdin.
	--post : post tweet from stdin (utf-8 is expected).
	--protect : don't display protected user's tweet.
	--sixel-cmd <fullpath>: external 'img2sixel'.
		or an internal sixel converter if not specified.
	--show-ng
	--support-evs
	--token <file> : token file (default: ~/.sayaka/token.json)
	--version
	--x68k

	-4
	-6
	--blocklist
	--ciphers <ciphers>
	--debug
	--max-cont <n>
	--max-image-cols <n>
	--mutelist
	--ngword-add
	--ngword-del
	--ngword-list
	--norest
	--nortlist
	--relay-server
	--user
"""
		);
		Process.exit(0);
	}

	private void TRACE(string msg)
	{
		//stderr.printf("%s\n", msg);
	}
}
