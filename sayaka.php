<?php
/*
 * sayaka - twitter client
 */
/*
 * Copyright (C) 2011-2014 Tetsuya Isaki
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

	// 設定ファイルに分けるか
	$iconsize = 32;
	$color_mode = 256;

	// まず引数のチェックをする
	$progname = $_SERVER["argv"][0];
	$cmd = "";

	if ($_SERVER["SERVER_PROTOCOL"] === "HTTP/1.1") {
		header("Connection: Keep-alive");
		$cmd = "stream";
	} else {
		if ($_SERVER["argc"] < 2) {
			usage();
		}
		for ($i = 1; $i < $_SERVER["argc"]; $i++) {
			switch ($_SERVER["argv"][$i]) {
			 case "--help":
			 default:
				usage();
				exit();
			 case "--stream":
				$cmd = "stream";
				break;
			 case "--color":
				$i++;
				$color_mode = $_SERVER["argv"][$i];
				if ($color_mode == "") {
					usage();
				}
				break;
			 case "--post":
				$cmd = "tweet";
				$i++;
				$text = $_SERVER["argv"][$i];
				if ($text == "") {
					usage();
				}
				break;
			 case "--pipe":
				// パイプモードなら標準入力から全部読み込む
				$text = "";
				while (($buf = fgets(STDIN))) {
					$text .= $buf;
				}
				$cmd = "tweet";
				break;
			}
		}
	}

	// ここからメインルーチン
	require_once "TwistOAuth.php";
	require_once "subr.php";
	setTimeZone();
	mb_internal_encoding("UTF-8");

	// DB からアクセストークンを取得
	$db = new sayakaSQLite3($configdb);
	$result = $db->query("select token, secret from t_token where id=1");
	$access = $result->fetcharray(SQLITE3_ASSOC);
	$db->close();

	// OAuth オブジェクト作成
	$tw = new TwistOAuth($consumer_key, $consumer_secret,
		$access["token"], $access["secret"]);

	// コマンド別処理
	switch ($cmd) {
	 case "tweet":
		tweet($text);
		break;
	 case "stream":
		init_stream();
		stream();
		break;
	}
	exit(0);
?>
<?php
function tweet($text)
{
	global $tw;

	// エンコードは?
	$encoded_text = $text;

	// 投稿するパラメータを用意
	$options = array();
	$options["status"] = $encoded_text;
	$options["trim_user"] = 1;
	// 投稿
	$json = $tw->post("statuses/update", $options);
	if (isset($json->error)) {
		print "Error has occured: {$json->error}\n";
	} else {
		print "Posted.\n";
	}
}

// ユーザストリームモードのための準備
function init_stream()
{
	global $color_mode;
	global $img2sixel;
	global $cachedir;

	// 色の初期化
	init_color();

	// img2sixel
	// XXX sixelなしモードを引数で指定したい
	$img2sixel = rtrim(`which img2sixel`);
	if ($img2sixel != "") {
		if ($color_mode <= 16) {
			if ($color_mode == 2) {
				$img2sixel .= " -e --quality=low";
			} else {
				$file = "colormap{$color_mode}.png";
				if (file_exists($file)) {
					$img2sixel .= " -m {$file}";
				} else {
					print "No colormap file: {$file}\n";
					exit(1);
				}
			}
		}
	}

	// 古いキャッシュを削除
	invalidate_cache();

	// ミュートユーザ取得
	get_mute_list();
}

// ユーザストリーム
function stream()
{
	global $tw;

	// Disable timeout
	set_time_limit(0);

	// Finish all buffering
	while (ob_get_level()) {
		ob_end_clean();
	}

	print "Ready..";
	@fflush(STDOUT);

	// Start streaming
	$tw->streaming("user", showstatus_callback,
		headerfunction_callback);
}

function headerfunction_callback($ch, $text)
{
	global $f_connected;
	if (isset($f_connected) == false) {
		print "Connected.\n";
		$f_connected = true;
	}
	return strlen($text);
}

//
// 1ツイートを表示するコールバック関数
//
function showstatus_callback($object)
{
	global $mediainfo;
	global $mutelist;

	define("ESC", "\x1b");
	define("CSI", ESC."[");

	// $object が元オブジェクト (イベント or メッセージ)

	// https://dev.twitter.com/streaming/overview/messages-types#Events_event
	if (isset($object->event)) {
		// event => イベント種別
		//			"favorite", "unfavorite", "follow", "unfollow", ...
		// timestamp_ms => イベント発生時刻(UNIXTIME)
		// created_at => イベント発生時刻
		// source => ふぁぼなら、ふぁぼ元ユーザ情報
		// target => ふぁぼなら、ふぁぼ先ユーザ情報
		// target_object => ふぁぼなら、ふぁぼったメッセージ

		switch ($object->event) {
		 case "favorite":
			$status = $object->target_object;
			break;
		 case "follow":
			$time = coloring(formattime($object), COLOR_TIME);
			$u = $object->source;
			$src_userid = coloring("@".unescape($u->screen_name), COLOR_USERID);
			$src_name   = coloring(unescape($u->name), COLOR_USERNAME);
			$u = $object->target;
			$dst_userid = coloring("@".unescape($u->screen_name), COLOR_USERID);
			$dst_name   = coloring(unescape($u->name), COLOR_USERNAME);

			print "\x1b[6C";
			print "{$src_userid} {$src_name} が {$dst_userid} {$dst_name} を";
			print "フォローしました。\n";
			print "\x1b[6C{$time} ";
			print coloring("sayakaちゃんからお知らせ", COLOR_SOURCE);
			print "\n";
			print "\n";
			return;

		 case "mute":
			add_mute_list($object->target->id_str);
			return;

		 case "unmute":
			del_mute_list($object->target->id_str);
			return;

		 default:
			return;
		}

	} else if (isset($object->text)) {
		// 通常のツイート
		// $status はツイートメッセージ
		$status = $object;
	} else {
		// それ以外の情報はとりあえず無視
		return;
	}

	// $status は常に元ツイート
	// $s はリツイートかどうかで変わる。
	$s = $status;
	if (isset($status->retweeted_status)) {
		$s = $status->retweeted_status;
	}

	// ミュートしてるユーザも stream には流れてきてしまうので、ここで弾く
	if (isset($mutelist[$status->user->id_str])) {
		return;
	}
	if (isset($status->retweeted_status)) {
		if (isset($mutelist[$s->user->id_str])) {
			return;
		}
	}

	$userid = coloring("@".unescape($s->user->screen_name), COLOR_USERID);
	$name   = coloring(unescape($s->user->name), COLOR_USERNAME);
	$src    = coloring(unescape(strip_tags($s->source))." から", COLOR_SOURCE);
	$time   = coloring(formattime($s), COLOR_TIME);
	$verified = $s->user->verified
		? coloring(" ●", COLOR_VERIFIED)
		: "";
	$protected = $s->user->protected
		? coloring(" ■", COLOR_PROTECTED)
		: "";

	$msg = formatmsg($s);

	// 今のところローカルアカウントはない
	$profile_image_url = $s->user->profile_image_url;

	print "";
	show_icon(unescape($s->user->screen_name), $profile_image_url);
	print CSI."3A";
	print CSI."6C";
	print "{$name} {$userid}{$verified}{$protected}\n";
	print CSI."6C";
	print $msg;
	print "\n";

	// picture
	foreach ($mediainfo as $m) {
		print CSI."6C";
		show_photo($m["target_url"], $m["width"]);
		print CSI."1A";
	}

	// source
	print CSI."6C";
	print "{$time} {$src}";
	// RT
	$rtcnt = $s->retweet_count;
	$rtcnt += 0;
	if ($rtcnt > 0) {
		print coloring(" {$rtcnt}RT", COLOR_RETWEET);
	}
	// fav
	$favcnt = $s->favorite_count;
	$favcnt += 0;
	if ($favcnt > 0) {
		print coloring(" {$favcnt}Fav", COLOR_FAVORITE);
	}
	print "\n";

	// リツイート元
	if (isset($status->retweeted_status)) {
		print CSI."6C";
		$rt_time   = formattime($status);
		$rt_userid = "@".unescape($status->user->screen_name);
		$rt_name   = unescape($status->user->name);
		print coloring("{$rt_time} {$rt_name} {$rt_userid} がリツイート",
			COLOR_RETWEET);
		print "\n";
	}

	// ふぁぼ元
	if (isset($object->event) && $object->event == "favorite") {
		print CSI."6C";
		$fav_time   = formattime($object);
		$fav_userid = "@".unescape($object->source->screen_name);
		$fav_name   = unescape($object->source->name);
		print coloring("{$fav_time} {$fav_name} {$fav_userid} がふぁぼ",
			COLOR_FAVORITE);
		print "\n";
	}

	print "\n";
}

function unescape($text)
{
	return htmlspecialchars_decode($text, ENT_NOQUOTES);
}

function init_color()
{
	global $color2esc;

	define("BOLD",		"1");
	define("UNDERSCORE","4");
	define("BLACK",		"30");
	define("RED",		"31");
	define("GREEN",		"32");
	define("YELLOW",	"33");
	define("BLUE",		"34");
	define("MAGENTA",	"35");
	define("CYAN",		"36");
	define("WHITE",		"37");
	define("BG_BLACK",	"40");
	define("BG_RED",	"41");
	define("BG_GREEN",	"42");
	define("BG_YELLOW",	"43");
	define("BG_BLUE",	"44");
	define("BG_MAGENTA","45");
	define("BG_CYAN",	"46");
	define("BG_WHITE",	"47");

	$color2esc = array(
		"COLOR_USERNAME"	=> YELLOW,
		"COLOR_USERID"		=> CYAN,
		"COLOR_TIME"		=> BOLD.";".BLACK,
		"COLOR_SOURCE"		=> BOLD.";".BLACK,

		"COLOR_RETWEET"		=> BOLD.";".GREEN,
		"COLOR_FAVORITE"	=> BOLD.";".YELLOW,
		"COLOR_URL"			=> UNDERSCORE.";".CYAN,
		"COLOR_TAG"			=> CYAN,
		"COLOR_VERIFIED"	=> CYAN,
		"COLOR_PROTECTED"	=> BOLD.";".BLACK,
	);
}

function coloring($text, $color_type)
{
	global $color2esc;

	if (isset($color2esc[$color_type])) {
		$rv = "\x1b[{$color2esc[$color_type]}m{$text}\x1b[0m";
	} else {
		$rv = "coloring({$text},{$color_type})";
	}
	return $rv;
}

// $object の日付時刻を表示用に整形して返す。
// timestamp_ms があれば使い、なければ created_at を使う。
// 今のところ、timestamp_ms はたぶん新しめのツイート/イベント通知には
// 付いてるはずだが、リツイートされた側は created_at しかない模様。
function formattime($object)
{
	$unixtime = isset($object->timestamp_ms)
		? ($object->timestamp_ms / 1000)
		: conv_twtime_to_unixtime($object->created_at);

	if (strftime("%F", $unixtime) == strftime("%F", time())) {
		// 今日なら時刻のみ
		return strftime("%T", $unixtime);
	} else {
		// 今日でなければ日付時刻
		return strftime("%F %T", $unixtime);
	}
}

// ステータスを解析しながら本文を整形したり情報抜き出したりを同時にする。
function formatmsg($s)
{
	global $mediainfo;

	$mediainfo = array();

	// 本文
	$text = $s->text;

	// タグ情報を展開
	// 文字位置しか指定されてないので、$text に一切の変更を加える前に
	// 調べないとタグが分からないというクソ仕様…。
	if (isset($s->entities) && isset($s->entities->hashtags)) {
		// 何文字目から何文字目までっていう構造なので後ろから処理する。
		// その際ソートは面倒なので、きっと前から並んでると思って逆順にする。
		$rev_tags = array();
		foreach ($s->entities->hashtags as $t) {
			array_unshift($rev_tags, $t);
		}

		// 後ろから調べる
		foreach ($rev_tags as $t) {
			// t->indices[0] … 開始位置、1文字目からなら0
			// t->indices[1] … 終了位置。この1文字前まで
			list ($start, $end) = $t->indices;

			// タグより前、タグ、タグより後ろに分解
			$pre  = mb_substr($text, 0, $start);
			$tag  = mb_substr($text, $start, $end - $start);
			$post = mb_substr($text, $end);

			$text = $pre . coloring($tag, COLOR_TAG) . $post;
		}
	}

	// ハッシュタグが済んでからエスケープ取り除く
	$text = unescape($text);

	// ユーザID
	$text = preg_replace_callback(
			"/(^|[^A-Za-z\d])(@\w+)/",
			function($m) { return $m[1].coloring($m[2], COLOR_USERID); },
			$text);

	// 短縮 URL を展開
	if (isset($s->entities) && isset($s->entities->urls)) {
		foreach ($s->entities->urls as $u) {
			//   url         本文中の短縮 URL (twitterから)
			//   display_url 差し替えて表示用の URL (twitterから)
			//   expanded_url 展開後の URL (twitterから)
			$disp = $u->display_url;
			$exp  = $u->expanded_url;

			// 本文の短縮 URL を差し替える
			// indices 使ってないけどまあ大丈夫だろう
			$text = preg_replace("|{$u->url}|",
				coloring($disp, COLOR_URL), $text);

			// 外部画像サービス
			if (preg_match("|twitpic.com/(\w+)|", $exp, $m)) {
				$target = "http://twitpic.com/show/mini/{$m[1]}";
				$mediainfo[] = array(
					"display_url" => $disp,
					"target_url"  => $target,
				);
			} else
			if (preg_match("|movapic.com/(pic/)?(\w+)|", $exp, $m)) {
				$target = "http://image.movapic.com/pic/t_{$m[2]}.jpeg";
				$mediainfo[] = array(
					"display_url" => $disp,
					"target_url"  => $target,
				);
			} else
			if (preg_match("|p.twipple.jp/(\w+)|", $exp, $m)) {
				$target = "http://p.twpl.jp/show/thumb/{$m[1]}";
				$mediainfo[] = array(
					"display_url" => $disp,
					"target_url"  => $target,
				);
			} else
			if (preg_match("|(.*instagram.com/p/[\w\-]+)/?|", $exp, $m)) {
				$target = "{$m[1]}/media/?size=t";
				$mediainfo[] = array(
					"display_url" => $disp,
					"target_url"  => $target,
				);
			} else
			if (preg_match("/\.(jpg|png|gif)$/", $exp)) {
				$mediainfo[] = array(
					"display_url" => $disp,
					"target_url"  => $exp,
					"width"       => 120,
				);
			}
		}
	}

	// メディア情報を展開
	if (isset($s->extended_entities) && isset($s->extended_entities->media)) {
		foreach ($s->extended_entities->media as $m) {
			// 本文の短縮 URL を差し替える
			// indices 使ってないけどまあ大丈夫だろう

			// 画像複数枚貼り付けてるとこの preg_replace をn回通るけど
			// 一応副作用はないので気にしないことにするか

			$text = preg_replace("|{$m->url}|",
				coloring($m->display_url, COLOR_URL), $text);

			// あとで画像展開につかうために覚えておく
			//   url         本文中の短縮 URL (twitterから)
			//   display_url 差し替えて表示用の URL (twitterから)
			//   media_url   指定の実ファイル URL (twitterから)
			//   target_url  それを元に実際に使う URL
			//   width       幅指定。ピクセルか割合で

			// pic.twitter.com の画像は :small でもでかいので 120px に縮小。
			// :thumb は縮小ではなく切り抜きなので使わない。
			$mediainfo[] = array(
				"display_url" => $m->display_url,
				"target_url"  => "{$m->media_url}:small",
				"width"       => 120,
			);
		}
	}

	return $text;
}

function show_icon($user, $img_url)
{
	global $iconsize;

	// URLのファイル名部分をキャッシュのキーにする
	$filename = basename($img_url);
	$img_file = "icon-{$user}-{$iconsize}-{$filename}.sixel";

	if (show_image($img_file, $img_url, $iconsize) === false) {
		print "\n\n\n\n";
	}
}

function show_photo($img_url, $percent)
{
	$img_file = preg_replace("|[:/]|", "_", $img_url);
	if (show_image($img_file, $img_url, $percent) == true) {
		print "\n";
	}
}

// 画像をキャッシュして表示
//  $img_file はキャッシュディレクトリ内でのファイル名
//  $img_url は画像の URL
//  $width は画像の幅。ピクセルかパーセントで指定。
// 表示できれば真を返す。
function show_image($img_file, $img_url, $width)
{
	global $cachedir;
	global $img2sixel;

	$img_file = "{$cachedir}/{$img_file}";

	if (strlen($width) > 0) {
		$width = "-w {$width}";
	} else {
		$width = "";
	}

	if (!file_exists($img_file)) {
		if ($img2sixel != "") {
			$imgconv = "{$img2sixel} {$width}";
			if (preg_match("/.gif$/i", $img_url)) {
				// img2sixel では表示できない GIF があるため
				$imgconv = "giftopnm | {$imgconv}";
			}
			system("(curl -Lks {$img_url} | "
			     . "{$imgconv} > {$img_file}) 2>/dev/null");
		}
	}
	if (filesize($img_file) == 0) {
		unlink($img_file);
		return false;
	} else {
		// ファイルを読んで標準出力に吐き出す(標準関数)
		readfile($img_file);
		@fflush(STDOUT);
		return true;
	}
}

// ミュートユーザ一覧の読み込み。
// $mutelist は "id_str" => "id_str" の連想配列。
// 今は毎回 twitter から取得しているだけ。
function get_mute_list()
{
	global $tw;
	global $mutelist;

	$options = array();
	$options["include_entities"] = false;
	$json = $tw->get("mutes/users/list", $options);
	if (isset($json->error)) {
		print "get(mutes/users/list) failed: {$json->error}\n";
		return;
	}

	$mutelist = array();
	foreach ($json->users as $user) {
		$mutelist[$user->id_str] = $user->id_str;
	}
}

// ミュートユーザを追加
function add_mute_list($id_str)
{
	global $mutelist;

	$mutelist[$id_str] = $id_str;
}

// ミュートユーザを削除
function del_mute_list($id_str)
{
	global $mutelist;

	unset($mutelist[$id_str]);
}

// 古いキャッシュを破棄する
function invalidate_cache()
{
	global $cachedir;

	// アイコンは7日分くらいか

	// 写真は24時間分くらいか

}

function usage()
{
	global $progname;

	print <<<__EOM__
usage:
 {$progname} --stream [--color <n>]
	streaming mode
 {$progname} --pipe
	tweet from stdin
 {$progname} --post "msg"
	tweet "msg" (without quote)

__EOM__;
	exit(0);
}
?>
