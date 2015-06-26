using System.OS;
using Gee;

class Program
{
	public static int main(string[] args)
	{
		var sayaka = new SayakaMain();
		return sayaka.Main(args);
	}
}

// PHP 由来
class PHP
{
	public static string unescape(string text)
	{
		// PHP の htmlspecialchars_decode(ENT_NOQUOTES) 相当品
		return text
			.replace("&lt;", "<")
			.replace("&gt;", ">")
			.replace("&amp;", "&");
	}

	public static string strip_tags(string text)
	{
		StringBuilder sb = new StringBuilder();
		bool intag = false;
		for (var i = 0; i < text.length; i++) {
			var c = text[i];
			if (intag) {
				if (c == '>') {
					intag = false;
				}
			} else {
				if (c == '<') {
					intag = true;
				} else {
					sb.append_c(c);
				}
			}
		}
		return sb.str;
	}
}

class SayakaMain
{
	public const string ESC = "\x1b";
	public const string CSI = ESC + "[";

	public string img2sixel;
	public int color_mode;
	public bool protect;
	public bool debug;
	public int screen_cols;
	public int fontheight;
	public int iconsize;
	public int imagesize;
	public int global_indent_level;

	public enum Color {
		UserName,
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
	}

	static SayakaMain sayakaMain;

	public SayakaMain()
	{
		sayakaMain = this;
	}

	public int Main(string[] args)
	{
		for (var i = 1; i < args.length; i++) {
			switch (args[i]) {
			 case "--color":
				color_mode = int.parse(args[++i]);
				break;
			 case "--noimg":
				img2sixel = "none";
				break;
			 case "--debug":
				debug = true;
				break;
			 default:
				usage();
				break;
			}
		}

		init_stream();

		string line;
		while ((line = stdin.read_line()) != null) {
			var parser = new Json.Parser();
			try {
				parser.load_from_data(line, -1);
			} catch {
				stdout.printf("error\n");
				return 0;
			}
			var obj = parser.get_root().get_object();
			showstatus_callback(obj);
		}
		return 0;
	}

	// ユーザストリームモードのための準備
	public void init_stream()
	{
		// 色の初期化
		//init_color();

		// img2sixel
		// --noimg オプションなら img2sixel を使わない
		// そうでなければ探して使う。がなければ使わない
		if (img2sixel == "none") {
			img2sixel = "";
		} else {
			string output;
			try {
				Process.spawn_command_line_sync("which img2sixel",
					out output);
			} catch {
			}
			img2sixel = TrimEnd(output);
		}
		if (img2sixel != "") {
			img2sixel += " -S";
			if (color_mode == 2) {
				img2sixel += " -e --quality=low";
			} else {
				img2sixel += " -m colormap%d.png".printf(color_mode);
			}
		}

		// シグナルハンドラを設定
		Posix.@signal(SIGWINCH, signal_handler);

		// 一度手動で呼び出して桁数を取得
		signal_handler(SIGWINCH);
	}

	// 1ツイートを表示するコールバック関数
	// ここではループ中からそのまま呼ばれる
	public void showstatus_callback(Json.Object obj)
	{
		Json.Object status = null;

		// obj が元オブジェクト (イベント or メッセージ)

		// 録画

		if (obj.has_member("event")) {
			// event => イベント種別
			//			"favorite", "unfavorite", "follow", "unfollow", ...
			// timestamp_ms => イベント発生時刻(UNIXTIME)
			// created_at => イベント発生時刻

			var event = obj.get_string_member("event");
			switch (event) {
			 case "favorite":
				if (obj.has_member("target_object")) {
					status = obj.get_object_member("target_object");

					// これだけだと、$status から $object が拾えないので
					// $object をバックリンクしておく。
					status.set_object_member("object", obj);
				}
				break;
			 case "follow":
			 case "mute":
			 case "unmute":
			 default:
				return;
			}
		} else if (obj.has_member("text")) {
			// 通常のツイート
			// status はツイートメッセージ
			status = obj;
		} else if (obj.has_member("friends")) {
			// 最初に送られてくる friends リストはいらない
			return;
		} else {
			// それ以外の情報はとりあえず無視
			return;
		}

		// ミュート...

		// NGワード...

		showstatus(status);
		stdout.printf("\n");
	}

	// 1ツイートを表示
	public void showstatus(Json.Object status)
	{
		Json.Object obj;

		if (status.has_member("object")) {
			obj = status.get_object_member("object");
		}

		Json.Object s = status;
		// RT なら、RT 元を $status、RT先を $s
		if (status.has_member("retweeted_status")) {
			s = status.get_object_member("retweeted_status");
		}

		var s_user = s.get_object_member("user");
		var userid = coloring(formatid(
			s_user.get_string_member("screen_name")),
			Color.UserId);
		var name = coloring(formatname(
			s_user.get_string_member("name")),
			Color.UserName);
		var src = coloring(PHP.unescape(PHP.strip_tags(
			s.get_string_member("source") + "から")),
			Color.Source);
		var time = coloring(formattime(s), Color.Time);
		var verified = s_user.get_boolean_member("verified")
			? coloring(" ●", Color.Verified)
			: "";
		var protected = s_user.get_boolean_member("protected")
			? coloring(" ■", Color.Protected)
			: "";

		// --protect オプションなら鍵ユーザのツイートを表示しない
		if (protect == true && protected != "") {
			print_(coloring("鍵垢", Color.NG) + "\n"
				+ time);
			stdout.printf("\n");
			return;
		}

		var mediainfo = new ArrayList<HashMap<string, string>>();
		var msg = formatmsg(s, ref mediainfo);

		// 今のところローカルアカウントはない
		var profile_image_url = s_user.get_string_member("profile_image_url");

		show_icon(PHP.unescape(s_user.get_string_member("screen_name")),
			profile_image_url);
		stdout.printf("\n");
		stdout.printf(CSI + "3A");

		print_(name + " " + userid + verified + protected);
		stdout.printf("\n");
		print_(msg);
		stdout.printf("\n");

		// picture
#if false
		foreach (var m in mediainfo) {
			stdout.printf(CSI + "6C");
			//show_photo();
			stdout.printf("\r");
		}
#endif

		// コメント付きRT の引用部分
		if (s.has_member("quoted_status")) {
			// この中はインデントを一つ下げる
			stdout.printf("\n");
			global_indent_level++;
			showstatus(s.get_object_member("quoted_status"));
			global_indent_level--;
		}

		// このステータスの既 RT、既ふぁぼ数
		var rtmsg = "";
		var favmsg = "";
		// RT
		var rtcnt = (int)s.get_int_member("retweet_count");
		if (rtcnt > 0) {
			rtmsg = coloring(" %dRT".printf(rtcnt), Color.Retweet);
		}
		// Fav
		var favcnt = (int)s.get_int_member("favorite_count");
		if (favcnt > 0) {
			favmsg = coloring(" %dFav".printf(favcnt), Color.Favorite);
		}
		print_("%s %s%s%s".printf(time, src, rtmsg, favmsg));
		stdout.printf("\n");
	}

	public void print_(string msg)
	{
		string rv;
		rv = make_indent(msg);

		// XXX 置換

		// XXX 文字コード

		stdout.printf("%s", rv);
	}

	// インデントをつける
	public string make_indent(string text)
	{
#if false
		// インデント階層
		var left = 6 * (global_indent_level + 1);
		var indent = string.printf(CSI + "%dC", left);

		var state = "";
		var newtext = indent;
		var x = left;
#endif
		return text;	/* XXX */
	}

	// 名前表示用に整形
	public string formatname(string text)
	{
		return PHP.unescape(text);	/* XXX */
	}

	// ID 表示用に整形
	public string formatid(string text)
	{
		return "@" + PHP.unescape(text);
	}

	public string coloring(string src, Color col)
	{
		return src;	/* XXX */
	}

	public string formattime(Json.Object obj)
	{
		return "";	/* XXX */
	}

	public string formatmsg(Json.Object s,
		ref ArrayList<HashMap<string, string>> mediainfo)
	{
		// 本文
		var text = s.get_string_member("text");

		// タグ情報を展開
		// 文字位置しか指定されてないので、text に一切の変更を加える前に
		// 調べないとタグが分からないというクソ仕様…。
		if (s.has_member("entities")) {
			var hashtags = s.get_object_member("entities")
			                .get_array_member("hashtags");
			if (hashtags.get_length() > 0) {
				text = format_hashtags(text, hashtags);
			}
		}

		// ハッシュタグが済んでからエスケープを取り除く
		text = PHP.unescape(text);

		return text;
	}

	public string format_hashtags(string text, Json.Array hashtags)
	{
		return text;	/* XXX */
	}

	public void show_icon(string user, string img_url)
	{
		stdout.printf("\n\n\n\n");	/* XXX */
	}

	public static void signal_handler(int signo)
	{
		sayakaMain.signal_handler_2(signo);
	}

	public void signal_handler_2(int signo)
	{
		switch (signo) {
		 case SIGWINCH:
			screen_cols = -1;
			fontheight = -1;
			winsize ws = winsize();
			var r = ioctl.TIOCGWINSZ(Posix.STDIN_FILENO, out ws);
			if (r != 0) {
				stdout.printf("TIOCGWINSZ failed\n");
			} else {
				screen_cols = ws.ws_col;
				if (ws.ws_row != 0) {
					fontheight = ws.ws_ypixel / ws.ws_row;
				}
				(void)ws.ws_xpixel;	// shut up warning
			}

			// フォント高さからアイコンの大きさを決定
			iconsize = (int)(fontheight * 2.5);
			imagesize = (int)(fontheight * 8.5);

			if (debug) {
				stdout.printf("screen columns=%d\n", screen_cols);
				stdout.printf("font height=%d\n", fontheight);
				stdout.printf("iconsize=%d\n", iconsize);
				stdout.printf("imagesize=%d\n", imagesize);
			}
			break;
		 default:
			break;
		}
	}

	// chomp が使えない
	public static string TrimEnd(string s)
	{
		var rv = s;
		rv._chomp();
		return rv;
	}

	public void usage()
	{
		stdout.printf("usage...\n");
		Process.exit(0);
	}
}
