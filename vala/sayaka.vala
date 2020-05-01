/*
 * sayaka - twitter client
 */
/*
 * Copyright (C) 2014-2020 Tetsuya Isaki
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
#if TEST
		return sayaka.Test(args);
#else
		return sayaka.Main(args);
#endif
	}
}

public class MediaInfo
{
	public string target_url;
	public string display_url;

	public MediaInfo(string t, string d)
	{
		target_url = t;
		display_url = d;
	}
}

public class SayakaMain
{
	public const string version = "3.4.4 (2020/05/01)";

	public const char CAN = '\x18';
	public const char ESC = '\x1b';
	public const string CSI = "\x1b[";

	public const int DEFAULT_FONT_WIDTH  = 7;
	public const int DEFAULT_FONT_HEIGHT = 14;

	public const int ColorFixedX68k = -1;

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
		Noop,
		StreamMode,
		PlayMode,
		TweetMode,
		FollowlistMode,
		MutelistMode,
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
	public int color_mode;
	public bool protect;
	public Diag diag;			// デバッグ (無分類)
	public Diag diagHttp;		// デバッグ (HTTP コネクション)
	public Diag diagImage;		// デバッグ (画像周り)
	public Diag diagShow;		// デバッグ (メッセージ表示判定)
	public int opt_debug_sixel;	// デバッグレベル (SIXEL変換周り)
	public bool opt_debug;		// デバッグオプション (後方互換性)
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
	public Dictionary<string, string> followlist
		= new Dictionary<string, string>();
	public Dictionary<string, string> blocklist
		= new Dictionary<string, string>();
	public Dictionary<string, string> mutelist
		= new Dictionary<string, string>();
	public Dictionary<string, string> nortlist
		= new Dictionary<string, string>();
	public bool opt_norest;
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
	public bool opt_full_url;
	public bool opt_progress;
	public NGWord ngword;
	public bool opt_ormode;
	public bool opt_outputpalette;
	public int opt_timeout_image;	// 画像取得の(接続)タイムアウト [msec]
	public bool opt_pseudo_home;	// 疑似ホームタイムライン
	public string myid;				// 自身の user id
	public bool opt_nocolor;		// テキストに(色)属性を一切付けない

	public string basedir;
	public string cachedir;
	public string tokenfile;
	public string colormapdir;

	static SayakaMain sayakaMain;

	public SayakaMain()
	{
		sayakaMain = this;
	}

	public int Main(string[] args)
	{
		diag = new Diag();
		diagHttp = new Diag.name("HttpClient");
		diagImage = new Diag();
		diagShow = new Diag();

		SayakaCmd cmd = SayakaCmd.Noop;

		basedir = Environment.get_home_dir() + "/.sayaka/";
		cachedir    = basedir + "cache";
		tokenfile   = basedir + "token.json";
		colormapdir = basedir;
		ngword = new NGWord(basedir + "ngword.json");

		address_family = SocketFamily.INVALID;	// UNSPEC がないので代用
		color_mode = 256;
		bg_white = true;
		opt_evs = false;
		opt_show_ng = false;
		opt_filter = "";
		last_id = "";
		last_id_count = 0;
		last_id_max = 10;
		ciphers = null;
		opt_full_url = false;
		opt_progress = false;
		opt_ormode = false;
		opt_outputpalette = true;
		opt_timeout_image = 3000;

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
				if (++i >= args.length) {
					usage();
				}
				ciphers = args[i];
				break;
			 case "--color":
			 {
				if (++i >= args.length) {
					usage();
				}
				var color_arg = args[i];
				if (color_arg == "x68k") {
					color_mode = ColorFixedX68k;
				} else {
					color_mode = int.parse(color_arg);
				}
				break;
			 }
			 case "--debug":
				if (++i >= args.length) {
					usage();
				}
				diag.SetLevel(int.parse(args[i]));
				// とりあえず後方互換
				opt_debug = (diag.GetLevel() > 0);
				break;
			 case "--debug-http":
				if (++i >= args.length) {
					usage();
				}
				diagHttp.SetLevel(int.parse(args[i]));
				break;
			 case "--debug-image":
				if (++i >= args.length) {
					usage();
				}
				diagImage.SetLevel(int.parse(args[i]));
				break;
			 case "--debug-show":
				if (++i >= args.length) {
					usage();
				}
				diagShow.SetLevel(int.parse(args[i]));
				break;
			 case "--debug-sixel":
				if (++i >= args.length) {
					usage();
				}
				opt_debug_sixel = int.parse(args[i]);
				max_image_count = 1;
				break;
			 case "--eucjp":
				iconv_tocode = "euc-jp";
				break;
			 case "--filter":
				if (++i >= args.length) {
					usage();
				}
				cmd = SayakaCmd.StreamMode;
				opt_filter = args[i];
				break;
			 case "--followlist":
				cmd = SayakaCmd.FollowlistMode;
				break;
			 case "--font":
				if (++i >= args.length) {
					usage();
				}
				var metric = args[i].split("x");
				if (metric.length != 2) {
					usage();
				}
				opt_fontwidth = int.parse(metric[0]);
				opt_fontheight = int.parse(metric[1]);
				break;
			 case "--full-url":
				opt_full_url = true;
				break;
			 case "--home":
				cmd = SayakaCmd.StreamMode;
				opt_pseudo_home = true;
				break;
			 case "--jis":
				iconv_tocode = "iso-2022-jp";
				break;
			 case "--max-cont":
				if (++i >= args.length) {
					usage();
				}
				last_id_max = int.parse(args[i]);
				break;
			 case "--max-image-cols":
				if (++i >= args.length) {
					usage();
				}
				max_image_count = int.parse(args[i]);
				if (max_image_count < 1) {
					max_image_count = 0;
				}
				break;
			 case "--mutelist":
				cmd = SayakaCmd.MutelistMode;
				break;
			 case "--ngword-add":
				if (++i >= args.length) {
					usage();
				}
				cmd = SayakaCmd.NgwordAdd;
				opt_ngword = args[i];
				break;
			 case "--ngword-del":
				if (++i >= args.length) {
					usage();
				}
				cmd = SayakaCmd.NgwordDel;
				opt_ngword = args[i];
				break;
			 case "--ngword-list":
				cmd = SayakaCmd.NgwordList;
				break;
			 case "--ngword-user":
				if (++i >= args.length) {
					usage();
				}
				opt_ngword_user = args[i];
				break;
			 case "--no-color":
				opt_nocolor = true;
				break;
			 case "--noimg":
			 case "--no-image":
				opt_noimg = true;
				break;
			 case "--no-rest":
				opt_norest = true;
				break;
			 case "--nortlist":
				cmd = SayakaCmd.NortlistMode;
				break;
			 case "--ormode":
			 {
				if (++i >= args.length) {
					usage();
				}
				var value = args[i];
				if (value == "on") {
					opt_ormode = true;
				} else if (value == "off") {
					opt_ormode = false;
				} else {
					usage();
				}
				break;
			 }
			 case "--palette":
			 {
				if (++i >= args.length) {
					usage();
				}
				var value = args[i];
				if (value == "on") {
					opt_outputpalette = true;
				} else if (value == "off") {
					opt_outputpalette = false;
				} else {
					usage();
				}
				break;
			 }
			 case "--play":
				cmd = SayakaCmd.PlayMode;
				break;
			 case "--post":
				cmd = SayakaCmd.TweetMode;
				break;
			 case "--progress":
				opt_progress = true;
				break;
			 case "--protect":
				protect = true;
				break;
			 case "--record":
				if (++i >= args.length) {
					usage();
				}
				record_file = args[i];
				break;
			 case "--show-ng":
				opt_show_ng = true;
				break;
			 case "--support-evs":
				opt_evs = true;
				break;
			 case "--timeout-image":
				if (++i >= args.length) {
					usage();
				}
				opt_timeout_image = int.parse(args[i]) * 1000;
				break;
			 case "--token":
			 {
				if (++i >= args.length) {
					usage();
				}
				var path = args[i];
				if (path == null) {
					usage();
				}
				if (path.contains("/")) {
					tokenfile = path;
				} else {
					tokenfile = basedir + path;
				}
				break;
			 }
			 case "--version":
				cmd = SayakaCmd.Version;
				break;
			 case "--white":
				bg_white = true;
				break;
			 case "--x68k":
				// 以下を指定したのと同じ
				color_mode = ColorFixedX68k;
				opt_fontwidth = 8;
				opt_fontheight = 16;
				iconv_tocode = "iso-2022-jp";
				bg_white = false;
				opt_progress = true;
				opt_ormode = true;
				opt_outputpalette = false;
				break;
			 case "-h":
			 case "--help":
				usage();
				break;
			 default:
				// 知らない引数はエラー。
				// そうでなければ filter キーワード。
				if (args[i][0] == '-') {
					stdout.printf(@"unknown option $(args[i])\n");
					usage();
				} else {
					cmd = SayakaCmd.StreamMode;
					opt_filter = args[i];
				}
				break;
			}
		}
		// --progress ならそれを展開したコマンドラインを表示してみるか
		if (opt_progress) {
			stdout.printf("%s", args[0]);
			for (var i = 1; i < args.length; i++) {
				if (args[i] == "--x68k") {
					stdout.printf(" --color x68k --font 8x16 --jis --black"
						+ " --progress --ormode on --palette off");
				} else {
					stdout.printf(@" $(args[i])");
				}
			}
			stdout.printf("\n");
		}

		// usage() は init() より前のほうがいいか。
		if (cmd == SayakaCmd.Noop) {
			usage();
			Process.exit(0);
		}

		if (opt_pseudo_home && opt_filter != "") {
			stdout.printf("filter keyword and --home must be exclusive.\n");
			usage();
			Process.exit(0);
		}

		diag.Debug(@"tokenfile = $(tokenfile)");
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
		 case SayakaCmd.FollowlistMode:
			cmd_followlist();
			break;
		 case SayakaCmd.MutelistMode:
			cmd_mutelist();
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
		Posix.@signal(Posix.Signal.INT,		signal_handler);
		Posix.@signal(Posix.Signal.HUP,		signal_handler);
		Posix.@signal(Posix.Signal.PIPE,	signal_handler);
		Posix.@signal(Posix.Signal.ALRM,	signal_handler);
		Posix.@signal(Posix.Signal.XCPU,	signal_handler);
		Posix.@signal(Posix.Signal.XFSZ,	signal_handler);
		Posix.@signal(Posix.Signal.VTALRM,	signal_handler);
		Posix.@signal(Posix.Signal.PROF,	signal_handler);
		Posix.@signal(Posix.Signal.USR1,	signal_handler);
		Posix.@signal(Posix.Signal.USR2,	signal_handler);
		// SIGWINCH は *BSD では SA_RESTART が立っていて
		// Linux では立っていないらしい。とりあえず立てておく。
		var act = Posix.sigaction_t();
		act.sa_handler = signal_handler;
		act.sa_flags = Posix.SA_RESTART;
		Posix.sigaction(Posix.Signal.WINCH, act, null);
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
		CreateTwitter();

		// 投稿するパラメータを用意
		var options = new Dictionary<string, string>();
		options.AddOrUpdate("status", text);
		options.AddOrUpdate("trim_user", "1");

		// 投稿
		var json = tw.API2Json("POST", Twitter.APIRoot, "statuses/update",
			options);
		if (json == null) {
			stderr.printf("statuses/update API2Json failed\n");
			Process.exit(1);
		}
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

	// ストリームモードのための準備
	public void init_stream()
	{
		// 端末が SIXEL をサポートしてなければ画像オフ
		if (native.term_support_sixel() == false) {
			if (opt_noimg == false) {
				stdout.printf(
					"Terminal doesn't support sixel, switch to --no-image\n");
			}
			opt_noimg = true;
		}

		// 色の初期化
		init_color();

		// 一度手動で呼び出して桁数を取得
		sigwinch();

		// NGワード取得
		ngword.parse_file();
	}

	// フィルタストリーム
	public void cmd_stream()
	{
		DataInputStream stream = null;

		// 古いキャッシュを削除
		if (opt_debug || opt_progress) {
			stdout.printf("Deleting expired cache files...");
			stdout.flush();
		}
		invalidate_cache();
		if (opt_debug || opt_progress) {
			stdout.printf("done\n");
		}

		// アクセストークンを取得
		CreateTwitter();

		if (opt_norest == false) {
			if (opt_pseudo_home) {
				// 疑似タイムライン用に自分の ID 取得
				if (opt_debug || opt_progress) {
					stdout.printf("Getting credentials...");
					stdout.flush();
				}
				get_credentials();
				if (opt_debug || opt_progress) {
					stdout.printf("done\n");
				}

				// 疑似タイムライン用にフォローユーザ取得
				if (opt_debug || opt_progress) {
					stdout.printf("Getting follow list...");
					stdout.flush();
				}
				get_follow_list();
				if (opt_debug || opt_progress) {
					stdout.printf("done\n");
				}

				// ストリームの場合だけフォローの中に自身を入れておく。
				// 表示するかどうかの判定はほぼフォローと同じなので。
				followlist.AddOrUpdate(myid, myid);
			}

			// ブロックユーザ取得
			if (opt_debug || opt_progress) {
				stdout.printf("Getting block users list...");
				stdout.flush();
			}
			get_block_list();
			if (opt_debug || opt_progress) {
				stdout.printf("done\n");
			}

			// ミュートユーザ取得
			if (opt_debug || opt_progress) {
				stdout.printf("Getting mute users list...");
				stdout.flush();
			}
			get_mute_list();
			if (opt_debug || opt_progress) {
				stdout.printf("done\n");
			}

			// RT非表示ユーザ取得
			if (opt_debug || opt_progress) {
				stdout.printf("Getting nort users list...");
				stdout.flush();
			}
			get_nort_list();
			if (opt_debug || opt_progress) {
				stdout.printf("done\n");
			}
		}

		stdout.printf("Ready..");
		stdout.flush();

		// ストリーミング開始
		diag.Debug("PostAPI call");
		try {
			var dict = new Dictionary<string, string>();
			if (opt_pseudo_home) {
				// 疑似ホームタイムライン
				// followlist には自分自身を加えているので必ず1人以上いる
				// ことを利用して join(",") 相当の処理をする
				var liststr = followlist.At(0).Key;
				for (var i = 1; i < followlist.Count; i++) {
					var kv = followlist.At(i);
					liststr += "," + kv.Key;
				}
				dict.AddOrUpdate("follow", liststr);
			} else {
				// キーワード検索
				dict.AddOrUpdate("track", opt_filter);
			}
			stream = tw.PostAPI(Twitter.PublicAPIRoot, "statuses/filter", dict);
		} catch (Error e) {
			stderr.printf("statuses/filter: %s\n", e.message);
			Process.exit(1);
		}
		stdout.printf("Connected.\n");

		while (true) {
			string line;
			try {
				line = stream.read_line();
			} catch (Error e) {
				stderr.printf("statuses/filter: read_line: %s\n", e.message);
				Process.exit(1);
			}
			if (line == null) {
				stderr.printf("statuses/filter: read_line: EOF?\n");
				Process.exit(1);
			}
			if (showobject(line) == false) {
				break;
			}
		}
	}

	// 再生モード
	public void cmd_play()
	{
		for (string line; (line = stdin.read_line()) != null; ) {
			if (showobject(line) == false) {
				break;
			}
		}
	}

	// Twitter オブジェクトを初期化
	public void CreateTwitter()
	{
		if (tw == null) {
			tw = new Twitter(diagHttp);
			get_access_token();

			// userstream 用なので今となってはもういらないのだが
			if (ciphers != null) {
				tw.SetCiphers(ciphers);
			}
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

	// ストリームから受け取った何かの1行 line を処理する共通部分。
	// line はイベントかメッセージの JSON 文字列1行分。
	// ファイルかソケットかで全部 read_line() が使えてれば
	// こんなことにはならないんだが…。
	// true でループ継続、false でループ終了。
	//
	// ユーザストリーム時代には各種イベントも飛んできていたので、ここでその
	// ディスパッチを行っていたためこういう構造になっている。
	// フィルタストリームではツイート以外が飛んでくることはないのだが、
	// ユーザストリーム時代の録画データの再生に対応するため、残してある。
	public bool showobject(string line)
	{
		// 空行がちょくちょく送られてくるようだ
		if (line == "") {
			diag.Debug("empty line");
			return true;
		}

		// line から obj にデコード。
		// obj が (イベント or メッセージ) のオブジェクト。
		ULib.Json obj;
		try {
			var parser = new ULib.JsonParser();
			obj = parser.Parse(line);
		} catch (Error e) {
			stdout.printf("showstatus_callback_line: Json Parser failed: %s\n",
				e.message);
			stdout.printf("showstatus_callback_line: "
				+ "There may be something wrong with twitter.\n");
			return false;
		}

		// status はツイート。
		ULib.Json status = null;

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
				last_id = "";
				return true;

			 case "mute":
				add_mute_list(obj.GetJson("target"));
				return true;

			 case "unmute":
				del_mute_list(obj.GetJson("target"));
				return true;

			 default:
				return true;
			}
		} else if (obj.Has("text")) {
			// 通常のツイート
			// status はツイートメッセージ
			status = obj;
		} else if (obj.Has("friends")) {
			// 最初に送られてくる friends リストはいらない
			return true;
		} else {
			// それ以外の情報はとりあえず無視
			return true;
		}

		// ツイートメッセージ
		var crlf = showstatus(status, false);
		if (crlf) {
			stdout.printf("\n");
		}
		return true;
	}

	// このツイートを表示するか。表示しないなら false。
	// NG ワード判定はここではない。
	public bool showstatus_acl(ULib.Json status, bool is_quoted)
	{
		// このツイートの発言者
		var user_id = status.GetJson("user").GetString("id_str");
		// リプライ先 (なければ "")
		var replyto_id = status.GetString("in_reply_to_user_id_str");
		// リツイート先の発言者 (なければ "")
		string rt_user_id = "";
		// リツイート先のリプライ先 (なければ "")
		string rt_replyto_id = "";
		if (status.Has("retweeted_status")) {
			var rt_status = status.GetJson("retweeted_status");
			rt_user_id = rt_status.GetJson("user").GetString("id_str");
			rt_replyto_id = rt_status.GetString("in_reply_to_user_id_str");
		}
		// デバッグ用
		string user_name = "";
		string replyto_name = "";
		string rt_user_name = "";
		string rt_replyto_name = "";
		if (diagShow.GetLevel() > 0) {
			user_name = status.GetJson("user").GetString("screen_name");
			replyto_name = status.GetString("in_reply_to_screen_name");
			if (status.Has("retweeted_status")) {
				var rt = status.GetJson("retweeted_status");
				rt_user_name = rt.GetJson("user").GetString("screen_name");
				rt_replyto_name = rt.GetString("in_reply_to_screen_name");
			}
		}

		bool? r;

		// ツイート(ベースのほう)を評価。
		r = showstatus_acl1(user_id, replyto_id, user_name, replyto_name);
		if (r != null) {
			return r;
		}

		// リツイートがあればそちらについても評価。
		if (rt_user_id != "") {
			r = showstatus_acl1(rt_user_id, rt_replyto_id,
				rt_user_name, rt_replyto_name);
			if (r != null) {
				return r;
			}
		}

		// 引用先なら、最低限ブロックした以外は全部表示。
		if (is_quoted) {
			return true;
		}

		if (opt_pseudo_home) {
			// RT非表示氏の発言はリツイートのみ別対応
			if (rt_user_id != "" && nortlist.ContainsKey(user_id)) {
				// ホームなら、RT非表示氏が他人のツイートを RT を弾く
				// Twitter の動作とは異なるけど、RT 非表示氏がフォロー氏を
				// RT するのは別に表示してもいいんじゃないかなあ
				if (!followlist.ContainsKey(rt_user_id)) {
					diagShow.Print(1, "showstatus_acl: "
						+ @"noretweet(@$(user_name)) -> false");
					return false;
				}
			} else if (followlist.ContainsKey(user_id)) {
				// ホームなら、フォロー氏から他人へのリプは弾く
				if (replyto_id != "" && !followlist.ContainsKey(replyto_id)) {
					diagShow.Print(2, "showstatus_acl: "
						+ @"follow(@$(user_name)) replies to other -> false");
					return false;
				}
			} else {
				// ホームなら、他人からは自分絡みのみ表示
				if (replyto_id == myid) {
					diagShow.Print(1,
						"showstatus_acl: other replies to me -> true");
					return true;
				}
				if (rt_user_id == myid) {
					diagShow.Print(1,
						"showstatus_acl: other retweet me -> true");
					return true;
				}
				diagShow.Print(2, "showstatus_acl: other -> false");
				return false;
			}
		}
		return true;
	}

	// 1ツイートに対する判定。
	// ベースと(あれば)リツイート先や引用先で同じ判定をするため。
	// user_id は発言者。
	// replyto_id はリプライ先(なければ "")。
	// 戻り値は true なら表示確定、false なら非表示確定。null なら未確定。
	public bool? showstatus_acl1(string user_id, string replyto_id,
		string user_name, string replyto_name)
	{
		// 俺氏の発言はすべて表示
		if (user_id == myid) {
			diagShow.Print(1, "showstatus_acl1: myid -> true");
			return true;
		}
		// ブロック氏の発言はすべて非表示
		if (blocklist.ContainsKey(user_id)) {
			diagShow.Print(1,
				@"showstatus_acl1: block(@$(user_name)) -> false");
			return false;
		}
		// ブロック以外からの俺氏宛の発言はすべて表示
		if (replyto_id == myid) {
			diagShow.Print(1, "showstatus_acl1: reply to me -> true");
			return true;
		}
		// ミュート氏の発言は、自分宛のリプのみ表示、それ以外は非表示だが
		// 自分宛はもう処理済みなので、ここは非表示だけでいい。
		if (mutelist.ContainsKey(user_id)) {
			diagShow.Print(1, @"showstatus_acl1: mute(@$(user_name)) -> false");
			return false;
		}

		// ミュート氏/ブロック氏に絡むものは非表示
		if (mutelist.ContainsKey(replyto_id)) {
			diagShow.Print(1, "showstatus_acl1: "
				+ @"@$(user_name) reply to mute(@$(replyto_name)) -> false");
			return false;
		}
		if (blocklist.ContainsKey(replyto_id)) {
			diagShow.Print(1, "showstatus_acl1: "
				+ @"@$(user_name) reply to block(@$(replyto_name)) -> false");
			return false;
		}

		return null;
	}

#if TEST
	public void test_showstatus_acl()
	{
		// id:1 が自分、id:2,3 がフォロー、
		// id:4 はミュートしているフォロー、
		// id:5 はRTを表示しないフォロー
		// id:6,7 はブロック、
		// id:8,9 がフォロー外
		myid = "1";
		followlist.AddOrUpdate("1", "1");	// 自身もフォローに入れてある
		followlist.AddOrUpdate("2", "2");
		followlist.AddOrUpdate("3", "3");
		followlist.AddOrUpdate("4", "4");
		followlist.AddOrUpdate("5", "5");
		mutelist.AddOrUpdate("4", "4");
		nortlist.AddOrUpdate("5", "5");
		blocklist.AddOrUpdate("6", "6");
		blocklist.AddOrUpdate("7", "7");

		// 簡易 JSON みたいな独自書式でテストを書いてコード中で JSON にする。
		// o 発言者 id (number) -> user.id_str (string)
		// o リプ先 reply (number) -> in_reply_to_user_id_str (string)
		// o リツイート rt (number) -> retweeted_status.user.id_str (string)
		// o リツイート先のリプライ先 rt_rep (number) ->
		//                 retweeted_status.in_reply_to_user_id_str (string)
		// 結果はホームタイムラインとフィルタモードによって期待値が異なり
		// それぞれ home, filt で表す。あれば表示、省略は非表示を意味する。
		// h---, f--- は流れてこないはずのためテスト不要を意味する。
		var table = new string[] {
			// 俺氏の発言
			"{id:1,        home,filt}",		// 平文
			"{id:1,reply:1,home,filt}",		// 自分自身へ
			"{id:1,reply:2,home,filt}",		// フォローへ
			"{id:1,reply:4,home,filt}",		// ミュートへ
			"{id:1,reply:5,home,filt}",		// RT非表示へ
			"{id:1,reply:6,h---,f---}",		// ブロックへ
			"{id:1,reply:8,home,filt}",		// 他人へ

			// フォロー氏の発言 (RT非表示氏も同じになるはずなので以下参照)
			"{id:2,        home,filt}",		// 平文
			"{id:2,reply:1,home,filt}",		// 自分へ
			"{id:2,reply:2,home,filt}",		// フォローへ
			"{id:2,reply:4,         }",		// ミュートへ
			"{id:2,reply:5,home,filt}",		// RT非表示へ
			"{id:2,reply:6,         }",		// ブロックへ
			"{id:2,reply:8,     filt}",		// 他人へ

			// ミュート氏の発言
			"{id:4,                 }",		// 平文
			"{id:4,reply:1,home,filt}",		// 自分へ
			"{id:4,reply:2,         }",		// フォローへ
			"{id:4,reply:4,         }",		// ミュートへ
			"{id:4,reply:5,         }",		// RT非表示へ
			"{id:4,reply:6,         }",		// ブロックへ
			"{id:4,reply:8,         }",		// 他人へ

			// RT非表示氏の発言 (リプはフォロー氏発言と同じ扱いでよいはず)
			"{id:5,        home,filt}",		// 平文
			"{id:5,reply:1,home,filt}",		// 自分へ
			"{id:5,reply:2,home,filt}",		// フォローへ
			"{id:5,reply:4,         }",		// ミュートへ
			"{id:5,reply:5,home,filt}",		// RT非表示へ
			"{id:5,reply:6,         }",		// ブロックへ
			"{id:5,reply:8,     filt}",		// 他人へ

			// ブロック氏の発言
			"{id:6,        h---     }",		// 平文
			"{id:6,reply:1,h---,f---}",		// 自分へ
			"{id:6,reply:2,         }",		// フォローへ
			"{id:6,reply:4,         }",		// ミュートへ
			"{id:6,reply:5,         }",		// RT非表示へ
			"{id:6,reply:6,h---,f---}",		// ブロックへ
			"{id:6,reply:8,         }",		// 他人へ

			// 他人氏の発言
			"{id:8,        h---,filt}",		// 平文
			"{id:8,reply:1,home,filt}",		// 自分へ
			"{id:8,reply:2,     filt}",		// フォローへ
			"{id:8,reply:4,         }",		// ミュートへ
			"{id:8,reply:5,     filt}",		// RT非表示へ
			"{id:8,reply:6,         }",		// ブロックへ
			"{id:8,reply:8,     filt}",		// 他人へ

			// 俺氏がリツイート
			"{id:1,rt:1,home,filt}",		// 自分自身を
			"{id:1,rt:2,home,filt}",		// フォローを
			"{id:1,rt:4,home,filt}",		// ミュートを
			"{id:1,rt:5,home,filt}",		// RT非表示を
			"{id:1,rt:6,h---,f---}",		// ブロックを
			"{id:1,rt:8,home,filt}",		// 他人を

			// フォロー氏がリツイート
			"{id:2,rt:1,home,filt}",		// 自分を
			"{id:2,rt:2,home,filt}",		// フォローを
			"{id:2,rt:4,         }",		// ミュートを
			"{id:2,rt:5,home,filt}",		// RT非表示を
			"{id:2,rt:6,         }",		// ブロックを
			"{id:2,rt:8,home,filt}",		// 他人を

			// ミュート氏がリツイート
			// XXX ミュート氏が自分のツイートをリツイートは表示するかどうか
			"{id:4,rt:1          }",		// 自分を
			"{id:4,rt:2,         }",		// フォローを
			"{id:4,rt:4,         }",		// ミュートを
			"{id:4,rt:5,         }",		// RT非表示を
			"{id:4,rt:6,         }",		// ブロックを
			"{id:4,rt:8,         }",		// 他人を

			// RT非表示氏がリツイート
			// 自分の発言をリツイートは表示してもいいだろう
			// フィルタストリームなら表示してもいいだろうか
			"{id:5,rt:1,home,filt}",		// 自分を
			"{id:5,rt:2,home,filt}",		// フォローを
			"{id:5,rt:4,         }",		// ミュートを
			"{id:5,rt:5,home,filt}",		// RT非表示を
			"{id:5,rt:6,         }",		// ブロックを
			"{id:5,rt:8,     filt}",		// 他人を

			// ブロック氏がリツイート (そもそも来ないような気がするけど一応)
			"{id:6,rt:1,h---,f---}",		// 自分を
			"{id:6,rt:2,         }",		// フォローを
			"{id:6,rt:4,         }",		// ミュートを
			"{id:6,rt:5,         }",		// RT非表示を
			"{id:6,rt:6,         }",		// ブロックを
			"{id:6,rt:8,         }",		// 他人を

			// 他人氏がリツイート
			"{id:8,rt:1,home,filt}",		// 自分を
			"{id:8,rt:2,     filt}",		// フォローを
			"{id:8,rt:4,         }",		// ミュートを
			"{id:8,rt:5,     filt}",		// RT非表示を
			"{id:8,rt:6,         }",		// ブロックを
			"{id:8,rt:8,     filt}",		// 他人を

			//
			// フォロー氏がリツイート
			"{id:2,rt:1,rt_rep:1,home,filt}",	// 俺氏から俺氏宛リプ
			"{id:2,rt:1,rt_rep:2,home,filt}",	// 俺氏からフォロー宛リプ
			"{id:2,rt:1,rt_rep:4,home,filt}",	// 俺氏からミュート宛リプ
			"{id:2,rt:1,rt_rep:5,home,filt}",	// 俺氏からRT非表示宛リプ
			"{id:2,rt:1,rt_rep:6,h---,f---}",	// 俺氏からブロック宛リプ
			"{id:2,rt:1,rt_rep:8,home,filt}",	// 俺氏から他人宛リプ
			"{id:2,rt:2,rt_rep:1,home,filt}",	// フォローから俺氏宛リプ
			"{id:2,rt:2,rt_rep:2,home,filt}",	// フォローからフォロー宛リプ
			"{id:2,rt:2,rt_rep:4,         }",	// フォローからミュート宛リプ
			"{id:2,rt:2,rt_rep:5,home,filt}",	// フォローからRT非表示宛リプ
			"{id:2,rt:2,rt_rep:6,         }",	// フォローからブロック宛リプ
			"{id:2,rt:2,rt_rep:8,home,filt}",	// フォローから他人宛リプ
			"{id:2,rt:4,rt_rep:1,home,filt}",	// ミュートから俺氏宛リプ
			"{id:2,rt:4,rt_rep:2,         }",	// ミュートからフォロー宛リプ
			"{id:2,rt:4,rt_rep:4,         }",	// ミュートからミュート宛リプ
			"{id:2,rt:4,rt_rep:5,         }",	// ミュートからRT非表示宛リプ
			"{id:2,rt:4,rt_rep:6,         }",	// ミュートからブロック宛リプ
			"{id:2,rt:4,rt_rep:8,         }",	// ミュートから他人宛リプ
			"{id:2,rt:5,rt_rep:1,home,filt}",	// RT非表示から俺氏宛リプ
			"{id:2,rt:5,rt_rep:2,home,filt}",	// RT非表示からフォロー宛リプ
			"{id:2,rt:5,rt_rep:4,         }",	// RT非表示からミュート宛リプ
			"{id:2,rt:5,rt_rep:5,home,filt}",	// RT非表示からRT非表示宛リプ
			"{id:2,rt:5,rt_rep:6,         }",	// RT非表示からブロック宛リプ
			"{id:2,rt:5,rt_rep:8,home,filt}",	// RT非表示から他人宛リプ
			"{id:2,rt:6,rt_rep:1,         }",	// ブロックから俺氏宛リプ
			"{id:2,rt:6,rt_rep:2,         }",	// ブロックからフォロー宛リプ
			"{id:2,rt:6,rt_rep:4,         }",	// ブロックからブロック宛リプ
			"{id:2,rt:6,rt_rep:5,         }",	// ブロックからRT非表示宛リプ
			"{id:2,rt:6,rt_rep:6,         }",	// ブロックからブロック宛リプ
			"{id:2,rt:6,rt_rep:8,         }",	// ブロックから他人宛リプ
			"{id:2,rt:8,rt_rep:1,home,filt}",	// 他人から俺氏宛リプ
			"{id:2,rt:8,rt_rep:2,home,filt}",	// 他人からフォロー宛リプ
			"{id:2,rt:8,rt_rep:4,         }",	// 他人からブロック宛リプ
			"{id:2,rt:8,rt_rep:5,home,filt}",	// 他人からRT非表示宛リプ
			"{id:2,rt:8,rt_rep:6,         }",	// 他人からブロック宛リプ
			"{id:2,rt:8,rt_rep:8,home,filt}",	// 他人から他人宛リプ
			// ミュート氏がリツイート
		/* XXX 面倒すぎる
			"{id:4,rt:1,rt_rep:1,home,filt}",	// 俺氏から俺氏宛リプ
			"{id:4,rt:1,rt_rep:2,home,filt}",	// 俺氏からフォロー宛リプ
			"{id:4,rt:1,rt_rep:4,home,filt}",	// 俺氏からミュート宛リプ
			"{id:4,rt:1,rt_rep:5,home,filt}",	// 俺氏からRT非表示宛リプ
			"{id:4,rt:1,rt_rep:6,home,filt}",	// 俺氏からブロック宛リプ(*N)
			"{id:4,rt:1,rt_rep:8,home,filt}",	// 俺氏から他人宛リプ
			"{id:4,rt:2,rt_rep:1,home,filt}",	// フォローから俺氏宛リプ
			"{id:4,rt:2,rt_rep:2,     filt}",	// フォローからフォロー宛リプ
			"{id:4,rt:2,rt_rep:4,         }",	// フォローからミュート宛リプ
			"{id:4,rt:2,rt_rep:5,     filt}",	// フォローからRT非表示宛リプ
			"{id:4,rt:2,rt_rep:6,         }",	// フォローからブロック宛リプ
			"{id:4,rt:2,rt_rep:8,     filt}",	// フォローから他人宛リプ
			"{id:4,rt:4,rt_rep:1,home,filt}",	// ミュートから俺氏宛リプ
			"{id:4,rt:4,rt_rep:2,         }",	// ミュートからフォロー宛リプ
			"{id:4,rt:4,rt_rep:4,         }",	// ミュートからミュート宛リプ
			"{id:4,rt:4,rt_rep:5,         }",	// ミュートからRT非表示宛リプ
			"{id:4,rt:4,rt_rep:6,         }",	// ミュートからブロック宛リプ
			"{id:4,rt:4,rt_rep:8,         }",	// ミュートから他人宛リプ
			"{id:4,rt:5,rt_rep:1,home,filt}",	// RT非表示から俺氏宛リプ
			"{id:4,rt:5,rt_rep:2,     filt}",	// RT非表示からフォロー宛リプ
			"{id:4,rt:5,rt_rep:4,         }",	// RT非表示からミュート宛リプ
			"{id:4,rt:5,rt_rep:5,     filt}",	// RT非表示からRT非表示宛リプ
			"{id:4,rt:5,rt_rep:6,         }",	// RT非表示からブロック宛リプ
			"{id:4,rt:5,rt_rep:8,     filt}",	// RT非表示から他人宛リプ
			"{id:4,rt:6,rt_rep:1,         }",	// ブロックから俺氏宛リプ
			"{id:4,rt:6,rt_rep:2,         }",	// ブロックからフォロー宛リプ
			"{id:4,rt:6,rt_rep:4,         }",	// ブロックからブロック宛リプ
			"{id:4,rt:6,rt_rep:5,         }",	// ブロックからRT非表示宛リプ
			"{id:4,rt:6,rt_rep:6,         }",	// ブロックからブロック宛リプ
			"{id:4,rt:6,rt_rep:8,         }",	// ブロックから他人宛リプ
			"{id:4,rt:8,rt_rep:1,home,filt}",	// 他人から俺氏宛リプ
			"{id:4,rt:8,rt_rep:2,     filt}",	// 他人からフォロー宛リプ
			"{id:4,rt:8,rt_rep:4,         }",	// 他人からブロック宛リプ
			"{id:4,rt:8,rt_rep:5,     filt}",	// 他人からRT非表示宛リプ
			"{id:4,rt:8,rt_rep:6,         }",	// 他人からブロック宛リプ
			"{id:4,rt:8,rt_rep:8,     filt}",	// 他人から他人宛リプ
		*/
			// 他人がリツイート
			"{id:8,rt:1,rt_rep:1,home,filt}",	// 俺氏から俺氏宛リプ
			"{id:8,rt:1,rt_rep:2,home,filt}",	// 俺氏からフォロー宛リプ
			"{id:8,rt:1,rt_rep:4,home,filt}",	// 俺氏からミュート宛リプ
			"{id:8,rt:1,rt_rep:5,home,filt}",	// 俺氏からRT非表示宛リプ
			"{id:8,rt:1,rt_rep:6,home,filt}",	// 俺氏からブロック宛リプ(*N)
			"{id:8,rt:1,rt_rep:8,home,filt}",	// 俺氏から他人宛リプ
			"{id:8,rt:2,rt_rep:1,home,filt}",	// フォローから俺氏宛リプ
			"{id:8,rt:2,rt_rep:2,     filt}",	// フォローからフォロー宛リプ
			"{id:8,rt:2,rt_rep:4,         }",	// フォローからミュート宛リプ
			"{id:8,rt:2,rt_rep:5,     filt}",	// フォローからRT非表示宛リプ
			"{id:8,rt:2,rt_rep:6,         }",	// フォローからブロック宛リプ
			"{id:8,rt:2,rt_rep:8,     filt}",	// フォローから他人宛リプ
			"{id:8,rt:4,rt_rep:1,home,filt}",	// ミュートから俺氏宛リプ
			"{id:8,rt:4,rt_rep:2,         }",	// ミュートからフォロー宛リプ
			"{id:8,rt:4,rt_rep:4,         }",	// ミュートからミュート宛リプ
			"{id:8,rt:4,rt_rep:5,         }",	// ミュートからRT非表示宛リプ
			"{id:8,rt:4,rt_rep:6,         }",	// ミュートからブロック宛リプ
			"{id:8,rt:4,rt_rep:8,         }",	// ミュートから他人宛リプ
			"{id:8,rt:5,rt_rep:1,home,filt}",	// RT非表示から俺氏宛リプ
			"{id:8,rt:5,rt_rep:2,     filt}",	// RT非表示からフォロー宛リプ
			"{id:8,rt:5,rt_rep:4,         }",	// RT非表示からミュート宛リプ
			"{id:8,rt:5,rt_rep:5,     filt}",	// RT非表示からRT非表示宛リプ
			"{id:8,rt:5,rt_rep:6,         }",	// RT非表示からブロック宛リプ
			"{id:8,rt:5,rt_rep:8,     filt}",	// RT非表示から他人宛リプ
			"{id:8,rt:6,rt_rep:1,         }",	// ブロックから俺氏宛リプ
			"{id:8,rt:6,rt_rep:2,         }",	// ブロックからフォロー宛リプ
			"{id:8,rt:6,rt_rep:4,         }",	// ブロックからブロック宛リプ
			"{id:8,rt:6,rt_rep:5,         }",	// ブロックからRT非表示宛リプ
			"{id:8,rt:6,rt_rep:6,         }",	// ブロックからブロック宛リプ
			"{id:8,rt:6,rt_rep:8,         }",	// ブロックから他人宛リプ
			"{id:8,rt:8,rt_rep:1,home,filt}",	// 他人から俺氏宛リプ
			"{id:8,rt:8,rt_rep:2,     filt}",	// 他人からフォロー宛リプ
			"{id:8,rt:8,rt_rep:4,         }",	// 他人からブロック宛リプ
			"{id:8,rt:8,rt_rep:5,     filt}",	// 他人からRT非表示宛リプ
			"{id:8,rt:8,rt_rep:6,         }",	// 他人からブロック宛リプ
			"{id:8,rt:8,rt_rep:8,     filt}",	// 他人から他人宛リプ
		};
		var ntest = 0;
		var nfail = 0;
		foreach (var input_sq in table) {
			// 入力文字列はテストを書きやすいよう簡易 JSON みたいな表記に
			// してあるので、これを正しい JSON に置換。
			var input_str = input_sq.replace(" ", "")
				.replace("id:",		"\"id\":")
				.replace("reply:",	"\"reply\":")
				.replace("rt:",		"\"rt\":")
				.replace("rt_rep:",	"\"rt_rep\":")
				.replace("home",	"\"home\":1")
				.replace("filt",	"\"filt\":1")
				.replace("h---",	"\"home\":-1")
				.replace("f---",	"\"filt\":-1")
				// 末尾カンマは許容しておいてここで消すほうが楽
				.replace(",}",		"}")
			;
			ULib.Json input;
			try {
				input = ULib.Json.FromString(input_str);
			} catch (Error e) {
				stdout.printf(@"Json.FromString($(input_str)) failed: "
					+ @"$(e.message)\n");
				Process.exit(1);
			}

			// それらから status をでっちあげる
			var dict = new Dictionary<string, ULib.Json>();
			// user.id_str
			var dictuser = new Dictionary<string, ULib.Json>();
			dictuser.AddOrUpdate("id_str",
				new Json.String(input.GetInt("id").to_string()));
			dictuser.AddOrUpdate("screen_name",
				new Json.String(input.GetInt("id").to_string()));
			dict.AddOrUpdate("user", new Json.Object(dictuser));
			// in_reply_to_user_id_str
			if (input.Has("reply")) {
				dict.AddOrUpdate("in_reply_to_user_id_str",
					new Json.String(input.GetInt("reply").to_string()));
				dict.AddOrUpdate("in_reply_to_screen_name",
					new Json.String(input.GetInt("reply").to_string()));
			}
			// retweeted_status.user.id_str
			if (input.Has("rt")) {
				var dict_rt = new Dictionary<string, ULib.Json>();
				var dict_rtuser = new Dictionary<string, ULib.Json>();
				dict_rtuser.AddOrUpdate("id_str",
					new Json.String(input.GetInt("rt").to_string()));
				dict_rtuser.AddOrUpdate("screen_name",
					new Json.String(input.GetInt("rt").to_string()));
				dict_rt.AddOrUpdate("user", new Json.Object(dict_rtuser));

				// retweeted_status.in_reply_to_user_id_str
				if (input.Has("rt_rep")) {
					dict_rt.AddOrUpdate("in_reply_to_user_id_str",
						new Json.String(input.GetInt("rt_rep").to_string()));
					dict_rt.AddOrUpdate("in_reply_to_screen_name",
						new Json.String(input.GetInt("rt_rep").to_string()));
				}

				dict.AddOrUpdate("retweeted_status", new Json.Object(dict_rt));
			}
			var status = new Json.Object(dict);

			// 期待値 (1=true, 0=false, -1 ならテストしない)
			var expected_home_int = input.GetInt("home");
			var expected_filt_int = input.GetInt("filt");
			var expected_home = (bool)expected_home_int;
			var expected_filt = (bool)expected_filt_int;

			// テスト (home)
			if (expected_home_int != -1) {
				ntest++;
				opt_pseudo_home = true;
				var result = showstatus_acl(status, false);
				if (result != expected_home) {
					stdout.printf(@"$(input_str) (for home) "
						+ @"expects '$(expected_home)' but '$(result)'\n");
					nfail++;
				}
			}

			if (expected_filt_int != -1) {
				// テスト (home/quoted)
				ntest++;
				var result = showstatus_acl(status, true);
				if (result != expected_filt) {
					stdout.printf(@"$(input_str) (for home/quoted) "
						+ @"expects '$(expected_filt)' but '$(result)'\n");
					nfail++;
				}

				// テスト (filter)
				ntest++;
				opt_pseudo_home = false;
				result = showstatus_acl(status, false);
				if (result != expected_filt) {
					stdout.printf(@"$(input_str) (for filter) "
						+ @"expects '$(expected_filt)' but '$(result)'\n");
					nfail++;
				}

				// テスト (filter/quoted)
				ntest++;
				result = showstatus_acl(status, true);
				if (result != expected_filt) {
					stdout.printf(@"$(input_str) (for filter/quoted) "
						+ @"expects '$(expected_filt)' but '$(result)'\n");
					nfail++;
				}
			}
		}
		stdout.printf(@"$(ntest) tests, $(ntest - nfail) passes");
		if (nfail > 0) {
			stdout.printf(@", $(nfail) FAILED!");
		}
		stdout.printf("\n");
	}
#endif

	// 1ツイートを表示。
	// true なら戻ったところで1行空ける改行。ツイートとツイートの間は1行
	// 空けるがここで判定の結果何も表示しなかったら空けないなど。
	public bool showstatus(ULib.Json status, bool is_quoted)
	{
		ULib.Json obj = status.GetJson("object");

		// このツイートを表示するかどうかの判定。
		// これは、このツイートがリツイートを持っているかどうかも含めた判定を
		// 行うのでリツイート分離前に行う。
		if (showstatus_acl(status, is_quoted) == false) {
			return false;
		}

		// NGワード
		var ngstat = ngword.match(status);
		if (ngstat.match) {
			// マッチしたらここで表示
			diagShow.Print(1, "showstatus: ng -> false");
			if (opt_show_ng) {
				var userid = coloring(formatid(ngstat.screen_name), Color.NG);
				var name = coloring(formatname(ngstat.name), Color.NG);
				var time = coloring(ngstat.time, Color.NG);

				var msg = coloring(@"NG:$(ngstat.ngword)", Color.NG);

				print_(@"$(name) $(userid)\n"
				     + @"$(time) $(msg)\n");
				return true;
			}
			return false;
		}

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
				return true;
			}
		}

		// 簡略表示の判定。QT 側では行わない
		if (is_quoted == false) {
			if (has_retweet) {
				var rt_id = s.GetString("id_str");

				// 直前のツイートが (フォロー氏による) 元ツイートで
				// 続けてこれがそれを RT したツイートなら簡略表示だが、
				// この二者は別なので1行空けたまま表示。
				if (rt_id == last_id) {
					if (last_id_count++ < last_id_max) {
						var rtmsg = format_rt_owner(status);
						var rtcnt = format_rt_cnt(s);
						var favcnt = format_fav_cnt(s);
						print_(rtmsg + rtcnt + favcnt + "\n");
						// これ以降はリツイートの連続とみなす
						last_id += "_RT";
						return true;
					}
				}
				// 直前のツイートがすでに誰か氏によるリツイートで
				// 続けてこれが同じツイートを RT したものなら簡略表示だが、
				// これはどちらも他者をリツイートなので区別しなくていい。
				if (rt_id + "_RT" == last_id) {
					if (last_id_count++ < last_id_max) {
						var rtmsg = format_rt_owner(status);
						var rtcnt = format_rt_cnt(s);
						var favcnt = format_fav_cnt(s);
						stdout.printf(@"$(CSI)1A");
						print_(rtmsg + rtcnt + favcnt + "\n");
						return true;
					}
				}
			}

			// 直前のツイートのふぁぼなら簡略表示
			if (obj != null && obj.GetString("event") == "favorite"
			 && last_id == status.GetString("id_str")) {
				if (last_id_count++ < last_id_max) {
					var favmsg = format_fav_owner(obj);
					var rtcnt = format_rt_cnt(s);
					var favcnt = format_fav_cnt(s);
					stdout.printf(@"$(CSI)1A");
					print_(favmsg + rtcnt + favcnt + "\n");
					return true;
				}
			}

			// 表示確定
			// 次回の簡略表示のために覚えておく。その際今回表示するのが
			// 元ツイートかリツイートかで次回の連続表示が変わる。
			if (has_retweet) {
				last_id = s.GetString("id_str") + "_RT";
			} else {
				last_id = status.GetString("id_str");
			}
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
			var indent = (indent_depth + 1) * indent_cols;
			stdout.printf(@"$(CSI)$(indent)C");
			show_photo(m.target_url, imagesize, i);
			stdout.printf("\r");
		}

		// コメント付きRT の引用部分
		if (s.Has("quoted_status")) {
			// この中はインデントを一つ下げる
			stdout.printf("\n");
			indent_depth++;
			showstatus(s.GetJson("quoted_status"), true);
			indent_depth--;
			// 引用表示後のここは改行しない
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

		return true;
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

	// インデントを付けて文字列を表示する
	public void print_(string text)
	{
		StringBuilder sb;

		// まず Unicode 文字単位でいろいろフィルターかける。
		var textarray = new List<unichar>();
		unichar uni;
		for (var i = 0; text.get_next_char(ref i, out uni); ) {
			// Private Use Area (外字) をコードポイント形式(?)にする
			if ((  0xe000 <= uni && uni <=   0xf8ff)	// BMP
			 || ( 0xf0000 <= uni && uni <=  0xffffd)	// 第15面
			 || (0x100000 <= uni && uni <= 0x10fffd)) 	// 第16面
			{
				textarray_addstring(ref textarray, "<U+%X>".printf(uni));
				continue;
			}

			// ここで EVS 文字を抜く。
			// 絵文字セレクタらしいけど、mlterm + sayaka14 フォント
			// だと U+FE0E とかの文字が前の文字に上書き出力されて
			// ぐちゃぐちゃになってしまうので、mlterm が対応するまでは
			// こっちでパッチ対応。
			if (uni == 0xfe0e || uni == 0xfe0f) {
				if (opt_evs) {
					textarray.append(uni);
				}
				continue;
			}

			// JIS/EUC-JP(/Shift-JIS) に変換する場合のマッピング
			if (iconv_tocode != "") {
				// 全角チルダ(U+FF5E) -> 波ダッシュ(U+301C)
				if (uni == 0xff5e) {
					textarray.append(0x301c);
					continue;
				}

				// 全角ハイフンマイナス(U+FF0D) -> マイナス記号(U+2212)
				if (uni == 0xff0d) {
					textarray.append(0x2212);
					continue;
				}

				// BULLET (U+2022) -> 中黒(U+30FB)
				if (uni == 0x2022) {
					textarray.append(0x30fb);
					continue;
				}
			}

			// NetBSD/x68k なら半角カナは表示できる。
			// XXX 正確には JIS という訳ではないのだがとりあえず
			if (iconv_tocode == "iso-2022-jp"){
				if (0xff61 <= uni && uni < 0xffa0) {
					// 半角カナはそのまま、あるいは JIS に変換、SI/SO を
					// 使うなどしても、この後の JIS-> UTF-8 変換を安全に
					// 通せないので、ここで半角カナを文字コードではない
					// 自前エスケープシーケンスに置き換えておいて、
					// 変換後にもう一度デコードして復元することにする。
					// ESC [ .. n は端末問い合わせの CSI シーケンスなので
					// 入力には来ないはずだし、仮にそのまま出力されたと
					// してもあまりまずいことにはならないんじゃないかな。
					// 半角カナを GL に置いた時の10進数2桁でエンコード。
					var str = "%c[%dn".printf(ESC, (int)uni - 0xff60 + 0x20);
					textarray_addstring(ref textarray, str);
					continue;
				}
			}

			textarray.append(uni);
		}

		// 文字コードを変換する場合は、
		// ここで一度変換してみて、それを Unicode に戻す。
		// この後の改行処理で、Unicode では半角幅だが変換すると全角ゲタ(〓)
		// になるような文字の文字幅が合わなくなるのを避けるため。
		if (iconv_tocode != "") {
			// textarray(リスト)を string に
			sb = new StringBuilder();
			for (var i = 0; i < textarray.length(); i++) {
				sb.append_unichar(textarray.nth_data(i));
			}

			// 変換してみる(変換できない文字をフォールバックさせる)
			string converted_text = null;
			try {
				converted_text = convert_with_fallback(sb.str, -1,
					iconv_tocode, "utf-8", null);
			} catch {
				converted_text = null;
			}
			sb = null;

			// UTF-8 に戻す
			string utf_text = null;
			try {
				// フォールバック指定しなくていいよな?
				utf_text = convert(converted_text, -1,
					"utf-8", iconv_tocode);
			} catch {
				utf_text = null;
			}

			// それを何事もなかったように textarray 形式に戻す。
			// ただし ESC [ %d n があれば NetBSD/x68k コンソールの
			// 半角カナ ESC ( I %c ESC ( B に置換する…。なんだかなあ。
			textarray = new List<unichar>();
			var inescape = 0;
			StringBuilder argstr = null;
			for (var i = 0; utf_text.get_next_char(ref i, out uni); ) {
				if (inescape == 0) {
					if (uni == ESC) {
						// ESC は出力せず次へ
						inescape = 1;
					} else {
						// 通常文字
						textarray.append(uni);
					}
				} else if (inescape == 1) {
					if (uni == '[') {
						// ESC [ なら出力せず次へ
						inescape = 2;
						argstr = new StringBuilder();
					} else {
						// ESC だが [ でなければ通常文字に戻す
						textarray.append(ESC);
						textarray.append(uni);
						inescape = 0;
					}
				} else {	/* inescape == 2 */
					if ('0' <= uni && uni <= '9') {
						argstr.append_unichar(uni);
					} else if (uni == 'n') {
						// ESC [ \d+ n なら半角カナを出力
						var ch = int.parse(argstr.str);
						var str = "%c(I%c%c(B".printf(ESC, ch, ESC);
						textarray_addstring(ref textarray, str);
						inescape = 0;
					} else {
						// 'n' 以外ならそのまま出力しておく。
						// ';' だとここでエスケープ切れるけど、エスケープを
						// 処理してるわけではないので、問題ないはず。
						textarray.append(ESC);
						textarray.append('[');
						textarray_addstring(ref textarray, argstr.str);
						textarray.append(uni);
						inescape = 0;
					}
				}
			}
		}

		// ここからインデント
		sb = new StringBuilder();

		// インデント階層
		var left = indent_cols * (indent_depth + 1);
		string indent = CSI + @"$(left)C";
		sb.append(indent);

		if (screen_cols == 0) {
			// 桁数が分からない場合は何もしない
			for (var i = 0; i < textarray.length(); i++) {
				sb.append_unichar(textarray.nth_data(i));
			}
		} else {
			// 1文字ずつ文字幅を数えながら出力用に整形していく
			int inescape = 0;
			var x = left;
			for (var i = 0; i < textarray.length(); i++) {
				uni = textarray.nth_data(i);
				if (inescape > 0) {
					// 1: ESC直後
					// 2: ESC [
					// 3: ESC (
					sb.append_unichar(uni);
					switch (inescape) {
					 case 1:
						// ESC 直後の文字で二手に分かれる
						if (uni == '[') {
							inescape = 2;
						} else {
							inescape = 3;	// 手抜き
						}
						break;
					 case 2:
						// ESC [ 以降 'm' まで
						if (uni == 'm') {
							inescape = 0;
						}
						break;
					 case 3:
						// ESC ( の次の1文字だけ
						inescape = 0;
						break;
					}
				} else {
					if (uni == ESC) {
						sb.append_unichar(uni);
						inescape = 1;
					} else if (uni == '\n') {
						sb.append_unichar(uni);
						sb.append(indent);
						x = left;
					} else if (uni.iswide_cjk()
					        || (0x1f000 <= uni && uni <= 0x1ffff))	// 絵文字
					{
						if (x > screen_cols - 2) {
							sb.append("\n");
							sb.append(indent);
							x = left;
						}
						sb.append_unichar(uni);
						x += 2;
					} else {
						sb.append_unichar(uni);
						x++;
					}
					if (x > screen_cols - 1) {
						sb.append("\n");
						sb.append(indent);
						x = left;
					}
				}
			}
		}
		var outtext = sb.str;

		// 出力文字コードの変換
		if (iconv_tocode != "") {
			try {
				string outtext2;
				outtext2 = convert(outtext, -1, iconv_tocode, "utf-8");
				outtext = outtext2;
			} catch {
				// nop
			}
		}

		stdout.puts(outtext);
	}

	// textarray に文字列 str を追加する。
	// print_() の下請け関数。
	private void textarray_addstring(ref List<unichar> textarray, string str)
	{
		unichar uni;

		for (var i = 0; str.get_next_char(ref i, out uni); ) {
			textarray.append(uni);
		}
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
		string blue = "";
		string green = "";
		string username = "";
		string fav = "";
		string gray = "";
		string verified = "";

		if (color_mode == 2) {
			// 2色モードなら色は全部無効にする。
			// ユーザ名だけボールドにすると少し目立って分かりやすいか。
			username = BOLD;
		} else {
			// それ以外のケースは色ごとに個別調整

			// 青は黒背景か白背景かで色合いを変えたほうが読みやすい
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
			if (color_mode == ColorFixedX68k) {
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

			// x68k 独自16色パッチでは 90 は黒、97 がグレー。
			// mlterm では 90 がグレー、97 は白。
			if (color_mode == ColorFixedX68k) {
				gray = "97";
			} else {
				gray = "90";
			}

			// 認証マークは白背景でも黒背景でもシアンでよさそう
			verified = CYAN;
		}

		color2esc[Color.Username]	= username;
		color2esc[Color.UserId]		= blue;
		color2esc[Color.Time]		= gray;
		color2esc[Color.Source]		= gray;

		color2esc[Color.Retweet]	= str_join(";", BOLD, green);
		color2esc[Color.Favorite]	= str_join(";", BOLD, fav);
		color2esc[Color.Url]		= str_join(";", UNDERSCORE, blue);
		color2esc[Color.Tag]		= blue;
		color2esc[Color.Verified]	= verified;
		color2esc[Color.Protected]	= gray;
		color2esc[Color.NG]			= str_join(";", STRIKE, gray);
	}

	// 文字列を separator で結合した文字列を返します。
	// ただし string.join() と異なり、(null と)空文字列の要素は排除した後に
	// 結合を行います。
	// XXX 今の所、引数は2つのケースしかないので手抜き。
	// 例)
	//   string.join(";", "AA", "") -> "AA;"
	//   str_join(";", "AA", "")    -> "AA"
	public static string str_join(string separator, string s1, string s2)
	{
		if (s1 == "" || s2 == "") {
			return s1 + s2;
		} else {
			return s1 + separator + s2;
		}
	}

	public string coloring(string text, Color col)
	{
		string rv;

		if (opt_nocolor) {
			// --nocolor なら一切属性を付けない
			rv = text;
		} else if (color2esc[col] == null) {
			// ポカ避け
			rv = @"Coloring($(text),$(col))";
		} else {
			rv = @"$(CSI)$(color2esc[col])m$(text)$(CSI)0m";
		}
		return rv;
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

	// 本文を整形して返す
	// (そのためにここでハッシュタグ、メンション、URL を展開)
	//
	// 従来はこうだった(↓)が
	//   "text":本文,
	//   "entities":{
	//     "hashtag":[..]
	//     "user_mentions":[..]
	//     "urls":[..]
	//   },
	//   "extended_entities":{
	//     "media":[..]
	//   }
	// extended_tweet 形式ではこれに加えて
	//   "extended_tweet":{
	//     "full_text":本文,
	//     "entities":{
	//     "hashtag":[..]
	//     "user_mentions":[..]
	//     "urls":[..]
	//     "media":[..]
	//   }
	// が追加されている。media の位置に注意。
	public string formatmsg(ULib.Json s, Array<MediaInfo> mediainfo)
	{
		ULib.Json extw = null;
		string text;

		// 本文
		if (s.Has("extended_tweet")) {
			extw = s.GetJson("extended_tweet");
			text = extw.GetString("full_text");
		} else {
			text = s.GetString("text");
		}

		// 1文字ずつに分解して配列に
		var utext = new unichar[text.char_count()];
		unichar uni;
		for (var pos = 0, i = 0; text.get_next_char(ref pos, out uni); ) {
			utext[i++] = uni;
		}

		// エンティティの位置が新旧で微妙に違うのを吸収
		ULib.Json entities;
		ULib.Json media_entities;
		if (extw != null) {
			entities = extw.GetJson("entities");
			media_entities = entities;
		} else {
			entities = s.GetJson("entities");
			media_entities = s.GetJson("extended_entities");
		}

		// エンティティを調べる
		var tags = new TextTag[utext.length];
		if (entities != null) {
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
				// --full-url モードなら短縮 URL ではなく元 URL を使う
				if (opt_full_url && newurl.has_suffix("…")) {
					newurl = expd_url.replace("http://", "");
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
		if (media_entities != null && media_entities.Has("media")) {
			var media = media_entities.GetArray("media");
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

				var target_url = @"$(media_url):small";
				var minfo = new MediaInfo(target_url, disp_url);
				mediainfo.append_val(minfo);	
			}
		}

		// タグ情報をもとにテキストを整形
		// 表示区間が指定されていたらそれに従う
		// XXX 後ろは添付画像 URL とかなので削るとして
		// XXX 前はメンションがどうなるか分からないのでとりあえず後回し
		var display_end = utext.length;
		if (extw != null && extw.Has("display_text_range")) {
			var range = extw.GetArray("display_text_range");
			display_end = range.data[1].AsInt;
		}
		var newtext = new StringBuilder();
		for (var i = 0; i < display_end; ) {
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

	// 現在行に user のアイコンを表示。
	// 呼び出し時点でカーソルは行頭にあるため、必要なインデントを行う。
	// アイコン表示後にカーソル位置を表示前の位置に戻す。
	public void show_icon(ULib.Json user)
	{
		// 改行x3 + カーソル上移動x3 を行ってあらかじめスクロールを
		// 発生させ、アイコン表示時にスクロールしないようにしてから
		// カーソル位置を保存する
		// (スクロールするとカーソル位置復元時に位置が合わない)
		stdout.printf("\n\n\n" + CSI + "3A" + @"$(ESC)7");

		// インデント。
		// CSI."0C" は0文字でなく1文字になってしまうので、必要な時だけ。
		if (indent_depth > 0) {
			var left = indent_cols * indent_depth;
			stdout.printf(@"$(CSI)$(left)C");
		}

		if (opt_noimg) {
			stdout.printf("◆");
		} else {
			var screen_name = unescape(user.GetString("screen_name"));
			var image_url = user.GetString("profile_image_url");

			// URLのファイル名部分をキャッシュのキーにする
			var filename = Path.get_basename(image_url);
			var img_file =
				@"icon-$(iconsize)x$(iconsize)-$(screen_name)-$(filename)";

			show_image(img_file, image_url, iconsize, -1);
		}

		// カーソル位置を復帰
		stdout.printf("\r");
		// カーソル位置保存/復元に対応していない端末でも動作するように
		// カーソル位置復元前にカーソル上移動x3を行う
		stdout.printf(CSI + "3A" + @"$(ESC)8");
	}

	// index は画像の番号 (位置決めに使用する)
	public bool show_photo(string img_url, int resize_width, int index)
	{
		string img_file = img_url;
		try {
			Regex regex = new Regex("[:/()? ]");
			img_file = regex.replace(img_url, img_url.length, 0, "_");
		} catch (Error e) {
			stdout.printf(@"show_photo: regex: $(e.message)\n");
		}

		return show_image(img_file, img_url, resize_width, index);
	}

	// 画像をキャッシュして表示
	//  $img_file はキャッシュディレクトリ内でのファイル名
	//  $img_url は画像の URL
	//  $resize_width はリサイズ後の画像の幅。ピクセルで指定。0 を指定すると
	//  リサイズせずオリジナルのサイズ。
	//  $index は -1 ならアイコン、0 以上なら添付写真の何枚目かを表す。
	//  どちらも位置決めなどのために使用する。
	// 表示できれば真を返す。
	public bool show_image(string img_file, string img_url, int resize_width,
		int index)
	{
		if (opt_noimg) return false;

		var tmp = Path.build_path(Path.DIR_SEPARATOR_S,
			cachedir, img_file);
		img_file = tmp;

		diagImage.Debug(@"show_image: img_url=$(img_url)");
		diagImage.Debug(@"show_image: img_file=$(img_file)");
		var cache_filename = img_file + ".sixel";
		var cache_file = FileStream.open(cache_filename, "r");
		if (cache_file == null) {
			// キャッシュファイルがないので、画像を取得
			diagImage.Debug("sixel cache is not found");
			cache_file = fetch_image(cache_filename, img_url, resize_width);
			if (cache_file == null) {
				return false;
			}
		}

		// SIXEL の先頭付近から幅と高さを取得
		var sx_width = 0;
		var sx_height = 0;
		uint8[] buf = new uint8[4096];
		var n = cache_file.read(buf);
		if (n < 32) {
			return false;
		}
		// " <Pan>; <Pad>; <Ph>; <Pv>
		int i;
		// Search "
		for (i = 0; i < buf.length && buf[i] != '\x22' ; i++)
			;
		// Skip Pad;
		for (i++; i < buf.length && buf[i] != ';'; i++)
			;
		// Skip Pad;
		for (i++; i < buf.length && buf[i] != ';'; i++)
			;
		// Ph
		var sb = new StringBuilder();
		for (i++; i < buf.length && ('0' <= buf[i] && buf[i] <= '9'); i++) {
			sb.append_unichar(buf[i]);
		}
		sx_width = int.parse(sb.str);
		// Pv
		sb = new StringBuilder();
		for (i++; i < buf.length && ('0' <= buf[i] && buf[i] <= '9'); i++) {
			sb.append_unichar(buf[i]);
		}
		sx_height = int.parse(sb.str);

		if (sx_width == 0 || sx_height == 0)
			return false;

		// この画像が占める文字数
		var image_rows = (sx_height + fontheight - 1) / fontheight;
		var image_cols = (sx_width + fontwidth - 1) / fontwidth;

		if (index < 0) {
			// アイコンの場合は呼び出し側で実施。
		} else {
			// 添付画像の場合、
			// 表示位置などの計算
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

		// 最初の1回はすでに buf に入っているのでまず出力して、
		// 次からは順次読みながら最後まで出力。
		do {
			in_sixel = true;
			stdout.write(buf[0:n]);
			stdout.flush();
			in_sixel = false;

			n = cache_file.read(buf);
		} while (n > 0);

		if (index < 0) {
			// アイコンの場合は呼び出し側で実施。
		} else {
			// 添付画像の場合
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

	// 画像をダウンロードして SIXEL に変換してキャッシュする。
	// 成功すれば、書き出したキャッシュファイルの FileStream クラス
	// (位置は先頭) を返す。失敗すれば null を返す。
	// cache_filename はキャッシュするファイルのファイル名
	// img_url は画像 URL
	// resize_width はリサイズすべき幅を指定、0 ならリサイズしない
	public FileStream? fetch_image(string cache_filename, string img_url,
		int resize_width)
	{
		var sx = new SixelConverter(opt_debug_sixel);

		// 共通の設定
		// 一番高速になる設定
		sx.LoaderMode = SixelLoaderMode.Lib;
		sx.ResizeMode = SixelResizeMode.ByLoad;
		// 縮小するので X68k でも画質 High でいける
		sx.ReduceMode = ReductorReduceMode.HighQuality;
		// 縮小のみの長辺指定変形。
		// height にも resize_width を渡すことで長辺を resize_width に
		// 制限できる。この関数の呼び出し意図がそれを想定している。
		// もともと幅しか指定できなかった経緯があり、
		// 本当は width/height をうまく分離すること。
		sx.ResizeWidth = resize_width;
		sx.ResizeHeight = resize_width;
		sx.ResizeAxis = ResizeAxisMode.ScaleDownLong;

		if (color_mode == ColorFixedX68k) {
			// とりあえず固定 16 色
			// システム取得する?
			sx.ColorMode = ReductorColorMode.FixedX68k;
		} else {
			if (color_mode <= 2) {
				sx.ColorMode = ReductorColorMode.Mono;
			} else if (color_mode < 8) {
				sx.ColorMode = ReductorColorMode.Gray;
				// グレーの場合の色数として colormode を渡す
				sx.GrayCount = color_mode;
			} else if (color_mode < 16) {
				sx.ColorMode = ReductorColorMode.Fixed8;
			} else if (color_mode < 256) {
				sx.ColorMode = ReductorColorMode.FixedANSI16;
			} else {
				sx.ColorMode = ReductorColorMode.Fixed256;
			}
		}
		if (opt_ormode) {
			sx.OutputMode = SixelOutputMode.Or;
		} else {
			sx.OutputMode = SixelOutputMode.Normal;
		}
		sx.OutputPalette = opt_outputpalette;

		HttpClient fg = new HttpClient(diagHttp);
		if (fg.Init(img_url) == false) {
			return null;
		}
		fg.Family = address_family;
		fg.SetTimeout(opt_timeout_image);
		DataInputStream stream;
		try {
			stream = fg.GET();
		} catch (Error e) {
			diag.Debug(@"Warning: fetch_image GET failed: $(e.message)");
			return null;
		}
		// URL の末尾が .jpg とか .png なのに Content-Type が image/* でない
		// (= HTML とか) を返すやつは画像ではないので無視。
		var content_type = fg.GetHeader(fg.RecvHeaders, "Content-Type");
		if (content_type != "" && content_type.has_prefix("image/") == false) {
			return null;
		}
		try {
			sx.LoadFromStream(stream);
		} catch (Error e) {
			diag.Debug("Warning: fetch_image LoadFromStream failed: "
				+ @"$(e.message)");
			return null;
		}

		// インデックスカラー変換
		sx.ConvertToIndexed();

		var file = FileStream.open(cache_filename, "w+");
		sx.SixelToStream(file);
		file.seek(0, FileSeek.SET);
		return file;
	}

	// 自分の ID を取得
	public void get_credentials()
	{
		CreateTwitter();

		var options = new Dictionary<string, string>();
		options["include_entities"] = "false";
		options["include_email"] = "false";
		var json = tw.API2Json("GET", Twitter.APIRoot,
			"account/verify_credentials", options);
		if (json == null) {
			stderr.printf("get_credentials API2Json failed\n");
			Process.exit(1);
		}
		diag.Debug(@"json=|$(json)|");
		if (json.Has("errors")) {
			// エラーのフォーマットがこれかどうかは分からんけど
			var errorlist = json.GetArray("errors");
			// エラーが複数返ってきたらどうするかね
			var code = errorlist.index(0).GetInt("code");
			var message = errorlist.index(0).GetString("message");
			stderr.printf(@"get_credentials failed: $(message)($(code))\n");
			Process.exit(1);
		}

		myid = json.GetString("id_str");
	}

	// ユーザ一覧を読み込む(共通)。
	// フォロー(friends)、ブロックユーザ、ミュートユーザは同じ形式。
	// 読み込んだリストを Dictionary 形式で返す。エラーなら終了する。
	// funcname はエラー時の表示用。
	public Dictionary<string, string> get_paged_list(string api,
		string funcname)
	{
		// ユーザ一覧は一度に全部送られてくるとは限らず、
		// next_cursor{,_str} が 0 なら最終ページ、そうでなければ
		// これを cursor に指定してもう一度リクエストを送る。

		var cursor = "-1";
		var list = new Dictionary<string, string>();
		list.Clear();

		do {
			var options = new Dictionary<string, string>();
			options["cursor"] = cursor;

			// JSON を取得
			var json = tw.API2Json("GET", Twitter.APIRoot, api, options);
			if (json == null) {
				stderr.printf(@"$(funcname) API2Json failed\n");
				Process.exit(1);
			}
			diag.Debug(@"json=|$(json)|");
			if (json.Has("errors")) {
				var errorlist = json.GetArray("errors");
				// エラーが複数返ってきたらどうするかね
				var code = errorlist.index(0).GetInt("code");
				var message = errorlist.index(0).GetString("message");
				stderr.printf(@"$(funcname) failed: $(message)($(code))\n");
				Process.exit(1);
			}

			var users = json.GetArray("ids");
			for (var i = 0; i < users.length; i++) {
				var id_str = users.index(i).AsNumber;
				list[id_str] = id_str;
			}

			cursor = json.GetString("next_cursor_str");
			diag.Debug(@"cursor=|$(cursor)|");
		} while (cursor != "0");

		return list;
	}

	// フォローユーザ一覧の読み込み
	public void get_follow_list()
	{
		followlist = get_paged_list("friends/ids", "get_follow_list");
	}

	// 取得したフォローユーザの一覧を表示する
	public void cmd_followlist()
	{
		CreateTwitter();

		get_follow_list();

		for (var i = 0; i < followlist.Count; i++) {
			var kv = followlist.At(i);
			stdout.printf("%s\n".printf(kv.Key));
		}
	}

	// ブロックユーザ一覧の読み込み
	public void get_block_list()
	{
		blocklist = get_paged_list("blocks/ids", "get_block_list");
	}

	// 取得したブロックユーザの一覧を表示する
	public void cmd_blocklist()
	{
		CreateTwitter();

		get_block_list();

		for (var i = 0; i < blocklist.Count; i++) {
			var kv = blocklist.At(i);
			stdout.printf("%s\n".printf(kv.Key));
		}
	}

	// ミュートユーザ一覧の読み込み
	public void get_mute_list()
	{
		mutelist = get_paged_list("mutes/users/ids", "get_mute_list");
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
		CreateTwitter();

		get_mute_list();

		for (var i = 0; i < mutelist.Count; i++) {
			var kv = mutelist.At(i);
			stdout.printf("%s\n".printf(kv.Key));
		}
	}

	// RT非表示ユーザ一覧の読み込み
	public void get_nort_list()
	{
		// ミュートユーザ一覧等とは違って、リスト一発で送られてくるっぽい。
		// なんであっちこっちで仕様が違うんだよ…。

		nortlist.Clear();

		// JSON を取得
		var json = tw.API2Json("GET", Twitter.APIRoot,
			"friendships/no_retweets/ids");
		if (json == null) {
			stderr.printf("get_nort_list API2Json failed\n");
			Process.exit(1);
		}
		diag.Debug(@"json=|$(json)|");

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
		CreateTwitter();

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

	// NGワードを追加する
	public void cmd_ngword_add()
	{
		ngword.cmd_add(opt_ngword, opt_ngword_user);
	}

	// NGワードを削除する
	public void cmd_ngword_del()
	{
		ngword.cmd_del(opt_ngword);
	}

	// NGワード一覧を表示する
	public void cmd_ngword_list()
	{
		ngword.cmd_list();
	}

	public static void signal_handler(int signo)
	{
		sayakaMain.signal_handler_2(signo);
	}

	public void signal_handler_2(int signo)
	{
		switch (signo) {
		 case Posix.Signal.INT:
			// SIXEL 出力中なら中断する (CAN + ST)
			if (in_sixel) {
				stdout.printf("%c%c%c", CAN, ESC, '\\');
				stdout.flush();
			} else {
				Process.exit(0);
			}
			break;

		 case Posix.Signal.WINCH:
			sigwinch();
			break;

		 default:
			stderr.printf(@"sayaka caught signal $(signo)\n");
			break;
		}
	}

	// SIGWINCH の処理。
	public void sigwinch()
	{
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

		diag.Debug(@"screen columns=$(screen_cols)$(msg_cols)");
		diag.Debug(@"font height=$(fontheight)$(msg_height)");
		diag.Debug(@"font width=$(fontwidth)$(msg_width)");
		diag.Debug(@"iconsize=$(iconsize)");
		diag.Debug(@"indent columns=$(indent_cols)");
		diag.Debug(@"imagesize=$(imagesize)");
	}

#if TEST
	public int Test(string[] args)
	{
		test_showstatus_acl();
		return 0;
	}
#endif

	public void cmd_version()
	{
		stdout.printf(@"sayaka.vala $(version)\n");
	}

	public void usage()
	{
		stdout.printf(
"""usage: sayaka [<options>...] --home
       sayaka [<options>...] <keyword>
	--color <n> : color mode { 2 .. 256 or x68k }. default 256.
	--font <w>x<h> : font width x height. default 7x14.
	--filter <keyword>
	--full-url : display full URL even if the URL is abbreviated.
	--home : pseudo home timeline using filter stream
	--white / --black : darken/lighten the text color. (default: --white)
	--no-color : disable all text color sequences
	--no-image / --noimg
	--jis
	--eucjp
	--play : read JSON from stdin.
	--post : post tweet from stdin (utf-8 is expected).
	--progress: show start up progress.
	--protect : don't display protected user's tweet.
	--show-ng
	--support-evs
	--token <file> : token file (default: ~/.sayaka/token.json)
	--version
	--x68k : preset options for x68k.

	-4
	-6
	--blocklist
	--ciphers <ciphers>
	--debug
	--debug-http <0-2>
	--debug-show <0-2>
	--debug-image <0-1>
	--debug-sixel <0-2>
	--followlist
	--max-cont <n>
	--max-image-cols <n>
	--mutelist
	--ngword-add
	--ngword-del
	--ngword-list
	--ngword-user
	--no-rest
	--nortlist
	--ormode <on|off> (default off)
	--palette <on|off> (default on)
"""
		);
		Process.exit(0);
	}
}
