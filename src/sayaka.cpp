/*
 * Copyright (C) 2014-2023 Tetsuya Isaki
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

#include "autofd.h"
#include "eaw_code.h"
#include "sayaka.h"
#include "FileStream.h"
#include "MathAlphaSymbols.h"
#include "RichString.h"
#include "StringUtil.h"
#include "UString.h"
#include "fetch_image.h"
#include "main.h"
#include "subr.h"
#include "term.h"
#include <memory>
#include <cstdio>
#include <cstring>
#include <err.h>

#if !defined(PATH_SEPARATOR)
#define PATH_SEPARATOR "/"
#endif

class MediaInfo
{
 public:
	MediaInfo()
	{
	}
	MediaInfo(const std::string& target_url_, const std::string& display_url_)
	{
		target_url = target_url_;
		display_url = display_url_;
	}

	std::string target_url  {};
	std::string display_url {};
};

enum Color {
	Username,
	UserId,
	Time,
	Source,

	Retweet,
	Favorite,
	Url,
	Tag,
	Verified,
	NG,
	Max,
};

static bool showobject(const std::string& line);
static bool showobject(const Json& obj);
static bool showstatus(const Json *status, bool is_quoted);
static UString format_rt_owner(const Json& s);
static UString format_rt_cnt(const Json& s);
static UString format_fav_cnt(const Json& s);
static void print_(const UString& utext);
static std::string str_join(const std::string& sep,
	const std::string& s1, const std::string& s2);
static UString ColorBegin(Color col);
static UString ColorEnd(Color col);
static UString coloring(const std::string& text, Color col);
static UString formatmsg(const Json& s, std::vector<MediaInfo> *mediainfo);
static void SetTag(RichString& t, const Json& list, Color color);
static void SetUrl_main(RichString& text, int start, int end,
	const std::string& url);
static void show_icon(const Json& user);
static bool show_photo(const std::string& img_url, int resize_width, int index);
static bool show_image(const std::string& img_file, const std::string& img_url,
	int resize_width, int index);
static StringDictionary get_paged_list(const std::string& api,
	const char *funcname);
static Json API2Json(const std::string& method, const std::string& apiRoot,
	const std::string& api, const StringDictionary& options,
	std::vector<std::string> *recvp = NULL);
static std::unique_ptr<HttpClient> API(const std::string& method,
	const std::string& apiRoot, const std::string& api,
	const StringDictionary& options);
static void record(const Json& obj);
static void invalidate_cache();
static std::string errors2string(const Json& json);

// 色定数
static const std::string BOLD		= "1";
static const std::string UNDERSCORE	= "4";
static const std::string STRIKE		= "9";
static const std::string BLACK		= "30";
static const std::string RED		= "31";
static const std::string GREEN		= "32";
static const std::string BROWN		= "33";
static const std::string BLUE		= "34";
static const std::string MAGENTA	= "35";
static const std::string CYAN		= "36";
static const std::string WHITE		= "37";
static const std::string GRAY		= "90";
static const std::string YELLOW		= "93";

int  address_family;			// AF_INET*
UseSixel use_sixel;				// SIXEL 画像を表示するかどうか
int  color_mode;				// 色数もしくはカラーモード
Diag diag;						// デバッグ (無分類)
Diag diagHttp;					// デバッグ (HTTP コネクション)
Diag diagImage;					// デバッグ (画像周り)
Diag diagShow;					// デバッグ (メッセージ表示判定)
bool opt_debug_format;			// デバッグフラグ (formatmsg 周り)
int  opt_debug_sixel;			// デバッグレベル (SIXEL変換周り)
int  screen_cols;				// 画面の桁数
int  opt_fontwidth;				// オプション指定のフォント幅
int  opt_fontheight;			// オプション指定のフォント高さ
int  fontwidth;					// フォントの幅(ドット数)
int  fontheight;				// フォントの高さ(ドット数)
int  iconsize;					// アイコンの大きさ(正方形、ドット数)
int  imagesize;					// 画像の大きさ(どこ?)
int  indent_cols;				// インデント1階層分の桁数
int  indent_depth;				// インデント深さ
int  max_image_count;			// この列に表示する画像の最大数
int  image_count;				// この列に表示している画像の数
int  image_next_cols;			// この列で次に表示する画像の位置(桁数)
int  image_max_rows;			// この列で最大の画像の高さ(行数)
enum bgcolor bgcolor;			// 背景用の色タイプ
std::string output_codeset;		// 出力文字コード ("" なら UTF-8)
OAuth oauth;
bool opt_show_ng;				// NG ツイートを隠さない
std::string opt_ngword;			// NG ワード (追加削除コマンド用)
std::string opt_ngword_user;	// NG 対象ユーザ (追加コマンド用)
std::string record_file;		// 記録用ファイルパス
std::vector<std::string> opt_filter;	// フィルタキーワード
std::string last_id;			// 直前に表示したツイート
int  last_id_count;				// 連続回数
int  last_id_max;				// 連続回数の上限
bool in_sixel;					// SIXEL 出力中なら true
std::string opt_ciphers;		// 暗号スイート
bool opt_full_url;				// URL を省略表示しない
bool opt_progress;				// 起動時の途中経過表示
NGWordList ngword_list;			// NG ワードリスト
bool opt_ormode;				// SIXEL ORmode で出力するなら true
bool opt_output_palette;		// SIXEL にパレット情報を出力するなら true
int  opt_timeout_image;			// 画像取得の(接続)タイムアウト [msec]
bool opt_pseudo_home;			// 疑似ホームタイムライン
std::string myid;				// 自身の user id
bool opt_nocolor;				// テキストに(色)属性を一切付けない
int  opt_record_mode;			// 0:保存しない 1:表示のみ 2:全部保存
bool opt_mathalpha;				// Mathematical AlphaNumeric を全角英数字に変換
std::string basedir;
std::string cachedir;
std::string tokenfile;
std::string colormapdir;

static std::array<UString, Color::Max> color2esc;	// 色エスケープ文字列

// 投稿する
void
cmd_tweet()
{
	// 標準入力から受け取る。UTF-8 前提。
	// ツイートは半角240字、全角140字換算で、全角はたぶんだいたい3バイト
	// なので、420 バイト程度が上限のはず?
	std::array<char, 1024> buf;
	int len = 0;
	while (len < buf.size() - 1) {
		if (fgets(buf.data() + len, buf.size() - len - 1, stdin) == NULL)
			break;
		len = strlen(buf.data());
	}

	std::string text(buf.data());
	text = Chomp(text);

	// アクセストークンを取得
	InitOAuth();

	// 投稿するパラメータを用意
	StringDictionary options;
	options.AddOrUpdate("status", text);
	options.AddOrUpdate("trim_user", "1");

	// 投稿
	auto json = API2Json("POST", APIROOT, "statuses/update",
		options);
	if (json.is_null()) {
		errx(1, "statuses/update API2Json failed");
	}
	if (json.contains("errors")) {
		errx(1, "statuses/update failed%s", errors2string(json).c_str());
	}
	printf("Posted.\n");
}

// フィルタストリーム
void
cmd_stream()
{
	// 古いキャッシュを削除
	progress("Deleting expired cache files...");
	invalidate_cache();
	progress("done\n");

	// アクセストークンを取得
	InitOAuth();

	int sleep_sec = 120;
	for (;;) {
		StringDictionary options;
		std::vector<std::string> recvhdrs;

		if (__predict_false(last_id.empty())) {
			// 最初の1回は home から直近の1件を取得。
			options["count"] = "1";
		} else {
			// 次からは前回以降を取得。
			//printf("sleep %d\n", sleep_sec);
			sleep(sleep_sec);
			options["since_id"] = last_id;
		}

		auto json = API2Json("GET", APIROOT, "statuses/home_timeline",
			options, &recvhdrs);
		if (json.is_array()) {
			// json は新→旧の順に並んでいるので、逆順に取り出す。
			for (int i = json.size() -1; i >= 0; i--) {
				const Json& j = json[i];
				showobject(j);

				auto id_str = j.value("id_str", "");
				if (id_str > last_id) {
					last_id = id_str;
				}
			}
		} else {
			printf("Not array: %s\n", json.dump().c_str());
			return;
		}

		// x-rate-limit-reset: <UNIXTIME> と
		// x-rate-limit-remaining: <num> から次の接続までの待ち時間を決定。
		// 15分間で 15回しかないが、リセット時間までに 3回くらいは残して
		// おいてみる。つまり 15分で最大 12回分。
		auto resettime_str = HttpClient::GetHeader(recvhdrs,
			"x-rate-limit-reset");
		auto remaining_str = HttpClient::GetHeader(recvhdrs,
			"x-rate-limit-remaining");

		uint64 resettime = strtoull(resettime_str.c_str(), NULL, 10);
		uint32 remaining = strtoul(remaining_str.c_str(), NULL, 10);
		time_t now = time(NULL);
		//printf("remain=%d until reset=%ld\n", remaining, (resettime - now));
		if (resettime > now) {
			if (remaining > 3) {
				sleep_sec = (resettime - now) / (remaining - 3);
			} else {
				sleep_sec = (resettime - now);
			}
		} else {
			// ?
			sleep_sec = 120;
		}
	}
}

// 再生モード
void
cmd_play()
{
	FileInputStream stdinstream(stdin, false);

	for (;;) {
		std::string line;
		auto r = stdinstream.ReadLine(&line);
		if (__predict_false(r <= 0)) {
			break;
		}
		if (showobject(line) == false) {
			break;
		}
	}
}

// ストリームから受け取った何かの1行 line を処理する共通部分。
// line はイベントかメッセージの JSON 文字列1行分。
// たぶんイベントは userstream 用なので、もう来ないはず。
static bool
showobject(const std::string& line)
{
	// 空行がちょくちょく送られてくるようだ
	if (line.empty()) {
		Debug(diag, "empty line");
		return true;
	}

	// line (文字列) から obj (JSON) に。
	Json obj = Json::parse(line);
	if (obj.is_null()) {
		warnx("%s: Json parser failed.\n"
			"There may be something wrong with twitter.", __func__);
		return false;
	}

	return showobject(obj);
}

// 1ツイート分の JSON を処理する。
static bool
showobject(const Json& obj)
{
	// 全ツイートを録画
	if (opt_record_mode == 2) {
		record(obj);
	}

	if (obj.contains("text")) {
		// 通常のツイート
		bool crlf = showstatus(&obj, false);
		if (crlf) {
			printf("\n");
		}
	} else {
		// それ以外はとりあえず無視
	}
	return true;
}

// 1ツイートを表示。
// true なら戻ったところで1行空ける改行。ツイートとツイートの間は1行
// 空けるがここで判定の結果何も表示しなかったら空けないなど。
static bool
showstatus(const Json *status, bool is_quoted)
{
	// 表示範囲だけ録画ならここで保存。
	// 実際にはここから NG ワードと鍵垢の非表示判定があるけど
	// もういいだろう。
	if (opt_record_mode == 1 && is_quoted == false) {
		record(*status);
	}

	// NGワード
	NGStatus ngstat;
	bool match = ngword_list.Match(&ngstat, *status);
	if (match) {
		// マッチしたらここで表示
		Debug(diagShow, "showstatus: ng -> false");
		if (opt_show_ng) {
			auto userid = coloring(formatid(ngstat.screen_name), Color::NG);
			auto name = coloring(formatname(ngstat.name), Color::NG);
			auto time = coloring(ngstat.time, Color::NG);
			auto msg = coloring("NG:" + ngstat.ngword, Color::NG);

			print_(name + ' ' + userid + '\n'
			     + time + ' ' + msg + '\n');
			return true;
		}
		return false;
	}

	// RT なら、RT 元を status に、RT先を s に。
	const Json *s = status;
	bool has_retweet = false;
	if ((*status).contains("retweeted_status")) {
		s = &(*status)["retweeted_status"];
		has_retweet = true;
	}

	// 簡略表示の判定。QT 側では行わない
	if (is_quoted == false) {
		if (has_retweet) {
			auto rt_id = (*s).value("id_str", "");

			// 直前のツイートが (フォロー氏による) 元ツイートで
			// 続けてこれがそれを RT したツイートなら簡略表示だが、
			// この二者は別なので1行空けたまま表示。
			if (rt_id == last_id) {
				if (last_id_count++ < last_id_max) {
					auto rtmsg = format_rt_owner(*status);
					auto rtcnt = format_rt_cnt(*s);
					auto favcnt = format_fav_cnt(*s);
					print_(rtmsg + rtcnt + favcnt + '\n');
					// これ以降のリツイートは連続とみなす
					last_id += "_RT";
					return true;
				}
			}
			// 直前のツイートがすでに誰か氏によるリツイートで
			// 続けてこれが同じツイートを RT したものなら簡略表示だが、
			// これはどちらも他者をリツイートなので区別しなくていい。
			if (rt_id + "_RT" == last_id) {
				if (last_id_count++ < last_id_max) {
					auto rtmsg = format_rt_owner(*status);
					auto rtcnt = format_rt_cnt(*s);
					auto favcnt = format_fav_cnt(*s);
					printf(CSI "1A");
					print_(rtmsg + rtcnt + favcnt + '\n');
					return true;
				}
			}
		}

		// 直前のツイートのふぁぼなら簡略表示
		if (0) {
			// userstream でしか来ない
		}

		// 表示確定
		// 次回の簡略表示のために覚えておく。その際今回表示するのが
		// 元ツイートかリツイートかで次回の連続表示が変わる。
		if (has_retweet) {
			last_id = (*s).value("id_str", "") + "_RT";
		} else {
			last_id = (*status).value("id_str", "");
		}
		last_id_count = 0;
	}

	const Json& s_user = (*s)["user"];
	auto userid = coloring(formatid(s_user.value("screen_name", "")),
		Color::UserId);
	auto name = coloring(formatname(s_user.value("name", "")), Color::Username);
	auto src = coloring(unescape(strip_tags((*s).value("source", ""))) + "から",
		Color::Source);
	auto time = coloring(formattime(*s), Color::Time);
	UString verified;
	if (s_user.value("verified", false)) {
		verified = coloring(" ●", Color::Verified);
	}

	std::vector<MediaInfo> mediainfo;
	auto msg = formatmsg(*s, &mediainfo);

	show_icon(s_user);
	print_(name + ' ' + userid + verified);
	printf("\n");
	print_(msg);
	printf("\n");

	// picture
	image_count = 0;
	image_next_cols = 0;
	image_max_rows = 0;
	for (int i = 0; i < mediainfo.size(); i++) {
		const auto& m = mediainfo[i];

		auto indent = (indent_depth + 1) * indent_cols;
		printf(CSI "%dC", indent);
		show_photo(m.target_url, imagesize, i);
		printf("\r");
	}

	// コメント付きRT の引用部分
	if ((*s).contains("quoted_status")) {
		// この中はインデントを一つ下げる
		printf("\n");
		indent_depth++;
		showstatus(&(*s)["quoted_status"], true);
		indent_depth--;
		// 引用表示後のここは改行しない
	}

	// このステータスの既 RT、既ふぁぼ数
	auto rtmsg = format_rt_cnt(*s);
	auto favmsg = format_fav_cnt(*s);
	print_(time + ' ' + src + rtmsg + favmsg);
	printf("\n");

	// リツイート元
	if (has_retweet) {
		print_(format_rt_owner(*status));
		printf("\n");
	}

	// ふぁぼはもう飛んでこない

	return true;
}

// リツイート元通知を整形して返す
static UString
format_rt_owner(const Json& status)
{
	const Json& user = status["user"];
	auto rt_time   = formattime(status);
	auto rt_userid = formatid(user.value("screen_name", ""));
	auto rt_name   = formatname(user.value("name", ""));
	auto str = coloring(string_format("%s %s %s がリツイート",
		rt_time.c_str(), rt_name.c_str(), rt_userid.c_str()), Color::Retweet);
	return str;
}

// リツイート数を整形して返す
static UString
format_rt_cnt(const Json& s)
{
	UString str;

	auto rtcnt = s.value("retweet_count", 0);
	if (rtcnt > 0) {
		str = coloring(string_format(" %dRT", rtcnt), Color::Retweet);
	}
	return str;
}

// ふぁぼ数を整形して返す
static UString
format_fav_cnt(const Json& s)
{
	UString str;

	auto favcnt = s.value("favorite_count", 0);
	if (favcnt > 0) {
		str = coloring(string_format(" %dFav", favcnt), Color::Favorite);
	}
	return str;
}

// RichString をインデントを付けて文字列を表示する
static void
print_(const UString& src)
{
	// Stage1: Unicode 文字単位でいろいろフィルターかける。
	UString utext;
	for (const auto uni : src) {
		// Private Use Area (外字) をコードポイント形式(?)にする
		if (__predict_false((  0xe000 <= uni && uni <=   0xf8ff))	// BMP
		 || __predict_false(( 0xf0000 <= uni && uni <=  0xffffd))	// 第15面
		 || __predict_false((0x100000 <= uni && uni <= 0x10fffd))) 	// 第16面
		{
			auto tmp = string_format("<U+%X>", uni);
			utext.AppendASCII(tmp);
			continue;
		}

		if (__predict_false((0x1d400 <= uni && uni <= 0x1d7ff)) &&
		    opt_mathalpha == true)
		{
			// Mathematical Alphanumeric Symbols を全角英数字に変換
			utext.Append(ConvMathAlpha(uni));
			continue;
		}

		if (__predict_false(!output_codeset.empty())) {
			// JIS/EUC-JP(/Shift-JIS) に変換する場合のマッピング
			// 本当は変換先がこれらの時だけのほうがいいだろうけど。

			// 全角チルダ(U+FF5E) -> 波ダッシュ(U+301C)
			if (uni == 0xff5e) {
				utext.Append(0x301c);
				continue;
			}

			// 全角ハイフンマイナス(U+FF0D) -> マイナス記号(U+2212)
			if (uni == 0xff0d) {
				utext.Append(0x2212);
				continue;
			}

			// BULLET (U+2022) -> 中黒(U+30FB)
			if (uni == 0x2022) {
				utext.Append(0x30fb);
				continue;
			}

			// NetBSD/x68k なら半角カナは表示できる。
			// XXX 正確には JIS という訳ではないのだがとりあえず
			if (output_codeset == "iso-2022-jp") {
				if (__predict_false(0xff61 <= uni && uni < 0xffa0)) {
					utext.AppendASCII(ESC "(I");
					utext.Append(uni - 0xff60 + 0x20);
					utext.AppendASCII(ESC "(B");
					continue;
				}
			}

			// 変換先に対応する文字がなければゲタ'〓'(U+3013)にする
			if (__predict_false(UString::IsUCharConvertible(uni) == false)) {
				utext.Append(0x3013);
				continue;
			}
		}

		utext.Append(uni);
	}

	// Stage2: インデントつけていく。
	UString utext2;
	// インデント階層
	auto left = indent_cols * (indent_depth + 1);
	auto indent = UString(string_format(CSI "%dC", left));
	utext2.Append(indent);

	if (__predict_false(screen_cols == 0)) {
		// 桁数が分からない場合は何もしない
		utext2.Append(utext);
	} else {
		// 1文字ずつ文字幅を数えながら出力用に整形していく
		int in_escape = 0;
		auto x = left;
		for (int i = 0, end = utext.size(); i < end; i++) {
			const auto uni = utext[i];
			if (__predict_false(in_escape > 0)) {
				// 1: ESC直後
				// 2: ESC [
				// 3: ESC (
				utext2.Append(uni);
				switch (in_escape) {
				 case 1:
					// ESC 直後の文字で二手に分かれる
					if (uni == '[') {
						in_escape = 2;
					} else {
						in_escape = 3;	// 手抜き
					}
					break;
				 case 2:
					// ESC [ 以降 'm' まで
					if (uni == 'm') {
						in_escape = 0;
					}
					break;
				 case 3:
					// ESC ( の次の1文字だけ
					in_escape = 0;
					break;
				}
			} else {
				if (uni == ESCchar) {
					utext2.Append(uni);
					in_escape = 1;
				} else if (uni == '\n') {
					utext2.Append(uni);
					utext2.Append(indent);
					x = left;
				} else {
					// 文字幅を取得
					auto width = get_eaw_width(uni);
					if (width == 1) {
						utext2.Append(uni);
						x++;
					} else {
						assert(width == 2);
						if (x > screen_cols - 2) {
							utext2.Append('\n');
							utext2.Append(indent);
							x = left;
						}
						utext2.Append(uni);
						x += 2;
					}
				}
				if (x > screen_cols - 1) {
					utext2.Append('\n');
					utext2.Append(indent);
					x = left;
				}
			}

			// デバッグ用
			if (0) {
				printf("[%d] U+%04x, x = %d", i, uni, x);
				if (uni == ESCchar) {
					printf(" ESC");
				} else if (uni == '\n') {
					printf(" '\\n'");
				} else if (0x20 <= uni && uni < 0x7f) {
					printf(" '%c'", uni);
				}
				printf("\n");
			}
		}
	}

	// 出力文字コードに変換
	std::string outstr = utext2.ToString();
	fputs(outstr.c_str(), stdout);
}

#define BG_ISBLACK(c)	((c) == BG_BLACK)
#define BG_ISWHITE(c)	((c) != BG_BLACK) // 姑息な最適化

void
init_color()
{
	std::string blue;
	std::string green;
	std::string username;
	std::string fav;
	std::string gray;
	std::string verified;

	if (color_mode == 2) {
		// 2色モードなら色は全部無効にする。
		// ユーザ名だけボールドにすると少し目立って分かりやすいか。
		username = BOLD;
	} else {
		// それ以外のケースは色ごとに個別調整。

		// 青は黒背景か白背景かで色合いを変えたほうが読みやすい
		if (BG_ISWHITE(bgcolor)) {
			blue = BLUE;
		} else {
			blue = CYAN;
		}

		// ユーザ名。白地の場合は出来ればもう少し暗めにしたい
		if (BG_ISWHITE(bgcolor) && color_mode > 16) {
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
		if (BG_ISWHITE(bgcolor) && color_mode > 16) {
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

	color2esc[Color::Username]	= UString(username);
	color2esc[Color::UserId]	= UString(blue);
	color2esc[Color::Time]		= UString(gray);
	color2esc[Color::Source]	= UString(gray);

	color2esc[Color::Retweet]	= UString(str_join(";", BOLD, green));
	color2esc[Color::Favorite]	= UString(str_join(";", BOLD, fav));
	color2esc[Color::Url]		= UString(str_join(";", UNDERSCORE, blue));
	color2esc[Color::Tag]		= UString(blue);
	color2esc[Color::Verified]	= UString(verified);
	color2esc[Color::NG]		= UString(str_join(";", STRIKE, gray));
}

// 文字列 s1 と s2 を sep で結合した文字列を返す。
// ただし (glib の) string.join() と異なり、(null と)空文字列の要素は排除
// した後に結合を行う。
// XXX 今の所、引数は2つのケースしかないので手抜き。
// 例)
//   string.join(";", "AA", "") -> "AA;"
//   str_join(";", "AA", "")    -> "AA"
static std::string
str_join(const std::string& sep, const std::string& s1, const std::string& s2)
{
	if (s1.empty()) {
		return s2;
	} else if (s2.empty()) {
		return s1;
	} else {
		return s1 + sep + s2;
	}
}

// 属性付け開始文字列を UString で返す
static UString
ColorBegin(Color col)
{
	UString esc;

	if (opt_nocolor) {
		// --no-color なら一切属性を付けない
	} else {
		esc.AppendASCII(CSI);
		esc.Append(color2esc[col]);
		esc.Append('m');
	}
	return esc;
}

// 属性付け終了文字列を UString で返す
static UString
ColorEnd(Color col)
{
	UString esc;

	if (opt_nocolor) {
		// --no-color なら一切属性を付けない
	} else {
		esc.AppendASCII(CSI "0m");
	}
	return esc;
}

// 文字列 text を UString に変換して、色属性を付けた UString を返す
static UString
coloring(const std::string& text, Color col)
{
	UString utext;

	utext += ColorBegin(col);
	utext += UString::FromUTF8(text);
	utext += ColorEnd(col);

	return utext;
}

// URL を差し替える
#define SetUrl(r, s, e, u) SetUrl_inline(r, s, e, u, display_end)
static inline void
SetUrl_inline(RichString& richtext, int start, int end, const std::string& url,
	int display_end)
{
	if (end <= display_end) {
#if defined(DEBUG_FORMAT)
		if (__predict_false(opt_debug_format)) {
			printf("SetUrl [%d,%d) |%s|\n", start, end, url.c_str());
		}
#endif
		SetUrl_main(richtext, start, end, url);
	}
#if defined(DEBUG_FORMAT)
	else if (__predict_false(opt_debug_format)) {
		printf("SetUrl [%d,%d) |%s| out of range\n",
			start, end, url.c_str());
	}
#endif
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
static UString
formatmsg(const Json& s, std::vector<MediaInfo> *mediainfo)
{
	const Json *extw = NULL;
	const Json *textj = NULL;

	// 本文
	if (s.contains("extended_tweet")) {
		extw = &s["extended_tweet"];
		if ((*extw).contains("full_text")) {
			textj = &(*extw)["full_text"];
		}
	} else {
		if (s.contains("text")) {
			textj = &s["text"];
		}
	}
	if (__predict_false(textj == NULL)) {
		// ないことはないはず
		return UString("(no text field?)");
	}
	const std::string& text = (*textj).get<std::string>();
	RichString richtext(text);

	// richtext は終端文字も含んだ長さなので、最後の文字は一つ手前。
	int display_end = richtext.size() - 1;

	// エンティティの位置が新旧で微妙に違うのを吸収
	const Json *entities = NULL;
	const Json *media_entities = NULL;
	if (extw) {
		if ((*extw).contains("entities")) {
			entities = &(*extw)["entities"];
		}
		media_entities = entities;

		// 表示区間が指定されていたら取得。
		// 後ろの添付画像 URL とかを削るためのもので、
		// 前がどうなるのかは不明。
		if ((*extw).contains("display_text_range")) {
			const Json& range = (*extw)["display_text_range"];
			if (range.is_array() && range.size() >= 2) {
				display_end = range[1].get<int>();
			}
		}
	} else {
		if (s.contains("entities")) {
			entities = &s["entities"];
		}
		if (s.contains("extended_entities")) {
			media_entities = &s["extended_entities"];
		}
	}

	// エンティティを調べる
	if (entities) {
		// ハッシュタグ情報を展開
		if ((*entities).contains("hashtags")) {
			const Json& hashtags = (*entities)["hashtags"];
			SetTag(richtext, hashtags, Color::Tag);
		}

		// ユーザID情報を展開
		if ((*entities).contains("user_mentions")) {
			const Json& mentions = (*entities)["user_mentions"];
			SetTag(richtext, mentions, Color::UserId);
		}

		// URL を展開
		if ((*entities).contains("urls")) {
			const Json& urls = (*entities)["urls"];
			if (urls.is_array()) {
				for (const Json& u : urls) {
					if (u.contains("indices")) {
						const Json& indices = u["indices"];
						if (!indices.is_array() || indices.size() != 2) {
							continue;
						}
						int start = indices[0].get<int>();
						int end   = indices[1].get<int>();

						// url          … 本文中の短縮 URL
						// display_url  … 差し替えて表示用の URL
						// expanded_url … 展開後の URL
						const auto& url      = u.value("url", "");
						const auto& disp_url = u.value("display_url", "");
						const auto& expd_url = u.value("expanded_url", "");

						// 本文の短縮 URL を差し替える
						std::string newurl;
						const auto& qid = s.value("quoted_status_id_str", "");
						std::string text2 = Chomp(text);
						if (s.contains("quoted_status")
						 && expd_url.find(qid) != std::string::npos
						 && EndWith(text2, url))
						{
							// この場合は引用 RT の URL なので、表示しなくていい
							newurl = "";
						} else {
							newurl = disp_url;
						}
						// --full-url モードなら短縮 URL ではなく元 URL を使う
						if (opt_full_url
						 && newurl.find("…") != std::string::npos)
						{
							newurl = string_replace(expd_url, "http://", "");
						}
						SetUrl(richtext, start, end, newurl);

						// 外部画像サービスを解析
						MediaInfo minfo;
#if 0
						if (format_image_url(&minfo, expd_url, disp_url)) {
							(*mediainfo).emplace_back(minfo);
						}
#endif
					}
				}
			}
		}
	}

	// メディア情報を展開
	if (media_entities != NULL && (*media_entities).contains("media")) {
		const Json& media = (*media_entities)["media"];
		for (const Json& m : media) {
			// 本文の短縮 URL を差し替える
			const std::string& disp_url = m.value("display_url", "");
			if (m.contains("indices")) {
				const Json& indices = m["indices"];
				if (indices.is_array() && indices.size() == 2) {
					int start = indices[0].get<int>();
					int end   = indices[1].get<int>();
					SetUrl(richtext, start, end, disp_url);
				}
			}

			// 画像展開に使う
			//   url         本文中の短縮 URL (twitterから)
			//   display_url 差し替えて表示用の URL (twitterから)
			//   media_url   指定の実ファイル URL (twitterから)
			//   target_url  それを元に実際に使う URL (こちらで付加)
			//   width       幅指定。ピクセルか割合で (こちらで付加)
			const std::string& media_url = m.value("media_url", "");
			std::string target_url = media_url + ":small";
			MediaInfo minfo(target_url, disp_url);
			(*mediainfo).emplace_back(minfo);
		}
	}

#if defined(DEBUG_FORMAT)
	if (__predict_false(opt_debug_format)) {
		printf("%s", richtext.dump().c_str());
		printf("display_end = %d\n", display_end);
	}
#endif

	// RichString を UString に変換。
	// ついでに HTML unescape と 改行を処理。
	UString new_text;
	int i;
	for (i = 0; i < display_end; i++) {
		auto& c = richtext[i];

		// 直前に差し込むエスケープシーケンスがあれば先に処理
		if (__predict_false(!c.altesc.empty())) {
			new_text.Append(c.altesc);
			// FALL THROUGH
		}

		// URL があれば展開
		if (__predict_false(!c.alturl.empty())) {
			new_text.Append(UString::FromUTF8(c.alturl));
			// FALL THROUGH
		}

		// 文字を展開
		// ついでに簡単なテキスト置換も同時にやってしまう。

		// URL 展開元の文字は負数にしてあるのでその部分は無視する
		if (__predict_false((int32_t)c.code < 0)) {
			continue;
		}
		// '\r' は無視
		if (__predict_false(c.code == '\r')) {
			continue;
		}
		// もう一度文字列にするのもあほらしいので、なんだかなあとは
		// 思うけど、ここでついでに unescape() もやってしまう。
		// "&amp;" -> "&"
		// "&lt;"  -> "<"
		// "&gt;"  -> ">"
		if (__predict_false(c.code == '&')) {
			if (richtext[i + 1].code == 'a' &&
			    richtext[i + 2].code == 'm' &&
			    richtext[i + 3].code == 'p' &&
			    richtext[i + 4].code == ';'   )
			{
				new_text.Append('&');
				i += 4;
				continue;
			}
			if (richtext[i + 1].code == 'l' &&
			    richtext[i + 2].code == 't' &&
			    richtext[i + 3].code == ';'   )
			{
				new_text.Append('<');
				i += 3;
				continue;
			}
			if (richtext[i + 1].code == 'g' &&
			    richtext[i + 2].code == 't' &&
			    richtext[i + 3].code == ';'   )
			{
				new_text.Append('>');
				i += 3;
				continue;
			}
		}

		new_text.Append(c.code);
	}

	// 表示する最後の文字の直後のエスケープシーケンスを出力
	if (!richtext[i].altesc.empty()) {
		new_text.Append(richtext[i].altesc);
	}

	return new_text;
}

// formatmsg() の下請け。
// list からタグ情報を取り出して tags にセットする。
// ハッシュタグとユーザメンションがまったく同じ構造なので。
//
// "hashtag": [
//   { "indices": [
//	     <start> … 開始位置、1文字目からなら 0
//       <end>   … 終了位置。この1文字前まで
//     ],
//     "...": 他のキーもあるかも知れないがここでは見ない
//   }, ...
// ]
static void
SetTag(RichString& richtext, const Json& list, Color color)
{
	if (list.is_array() == false) {
		return;
	}

	for (const Json& t : list) {
		if (t.contains("indices")) {
			const Json& indices = t["indices"];
			if (indices.is_array() && indices.size() == 2) {
				int start = indices[0].get<int>();
				int end   = indices[1].get<int>();
#if defined(DEBUG_FORMAT)
				if (__predict_false(opt_debug_format)) {
					printf("SetTag [%d,%d)\n", start, end);
				}
#endif
				richtext[start].altesc += ColorBegin(color);
				richtext[end].altesc += ColorEnd(color);
			}
		}
	}
}

// formatmsg() の下請け。
// text の [start, end) を url で差し替える(ための処理をする)。
static void
SetUrl_main(RichString& text, int start, int end, const std::string& url)
{
	int i = start;

	// すでにあれば何もしない (もうちょっとチェックしたほうがいいかも)
	if (!text[i].alturl.empty()) {
		return;
	}

	// 開始位置に URL を覚えておく
	text[i].alturl = url;
	text[i].altesc = ColorBegin(Color::Url);
	// この範囲の元の文字列を非表示にマークする
	for (; i < end; i++) {
		auto& c = text[i];
		c.code = -c.code;
	}
	// (この手前で)終了
	text[i].altesc = ColorEnd(Color::Url);
}

// 現在行に user のアイコンを表示。
// 呼び出し時点でカーソルは行頭にあるため、必要なインデントを行う。
// アイコン表示後にカーソル位置を表示前の位置に戻す。
static void
show_icon(const Json& user)
{
	const std::array<std::string, 2> urls = {
		"profile_image_url",
		"profile_image_url_https",
	};
	std::string screen_name;

	// 改行x3 + カーソル上移動x3 を行ってあらかじめスクロールを
	// 発生させ、アイコン表示時にスクロールしないようにしてから
	// カーソル位置を保存する
	// (スクロールするとカーソル位置復元時に位置が合わない)
	printf("\n\n\n" CSI "3A" ESC "7");

	// インデント。
	// CSI."0C" は0文字でなく1文字になってしまうので、必要な時だけ。
	if (indent_depth > 0) {
		int left = indent_cols * indent_depth;
		printf(CSI "%dC", left);
	}

	bool shown = false;
	if (__predict_false(use_sixel == UseSixel::No)) {
		goto done;
	}

	screen_name = unescape(user.value("screen_name", ""));
	// http, https の順で試す
	for (const auto& url : urls) {
		if (user.contains(url)) {
			const Json& image_url_json = user[url];
			const std::string& image_url = image_url_json.get<std::string>();

			// URL のファイル名部分をキャッシュのキーにする
			auto p = image_url.rfind('/');
			if (__predict_true(p >= 0)) {
				auto img_file = string_format("icon-%dx%d-%s-%s",
					iconsize, iconsize, screen_name.c_str(),
					image_url.c_str() + p + 1);
				if (show_image(img_file, image_url, iconsize, -1)) {
					shown = true;
					goto done;
				}
			}
		}
	}

 done:
	if (__predict_true(shown)) {
		// アイコン表示後、カーソル位置を復帰
		printf("\r");
		// カーソル位置保存/復元に対応していない端末でも動作するように
		// カーソル位置復元前にカーソル上移動x3を行う
		printf(CSI "3A" ESC "8");
	} else {
		// アイコンを表示してない場合はここで代替アイコンを表示。
		printf(" *");
		// これだけで復帰できるはず
		printf("\r");
	}
}

// index は画像の番号 (位置決めに使用する)
static bool
show_photo(const std::string& img_url, int resize_width, int index)
{
	auto img_file = img_url;

	for (auto p = 0;
		(p = img_file.find_first_of(":/()? ", p)) != std::string::npos;
		p++)
	{
		img_file[p] = '_';
	}

	return show_image(img_file, img_url, resize_width, index);
}

// 画像をキャッシュして表示
//  img_file はキャッシュディレクトリ内でのファイル名
//  img_url は画像の URL
//  resize_width はリサイズ後の画像の幅。ピクセルで指定。0 を指定すると
//  リサイズせずオリジナルのサイズ。
//  index は -1 ならアイコン、0 以上なら添付写真の何枚目かを表す。
//  どちらも位置決めなどのために使用する。
// 表示できれば真を返す。
static bool
show_image(const std::string& img_file, const std::string& img_url,
	int resize_width, int index)
{
	if (use_sixel == UseSixel::No)
		return false;

	std::string img_path = cachedir + PATH_SEPARATOR + img_file;

	Debug(diagImage, "%s: img_url=%s", __func__, img_url.c_str());
	Debug(diagImage, "%s: img_path=%s", __func__, img_path.c_str());
	auto cache_filename = img_path + ".sixel";
	AutoFILE cache_file = fopen(cache_filename.c_str(), "r");
	if (!cache_file.Valid()) {
		// キャッシュファイルがないので、画像を取得
		Debug(diagImage, "sixel cache is not found");
		cache_file = fetch_image(cache_filename, img_url, resize_width);
		if (!cache_file.Valid()) {
			return false;
		}
	}

	// SIXEL の先頭付近から幅と高さを取得
	auto sx_width = 0;
	auto sx_height = 0;
	char buf[4096];
	char *ep;
	auto n = fread(buf, 1, sizeof(buf), cache_file);
	if (n < 32) {
		return false;
	}
	// " <Pan>; <Pad>; <Ph>; <Pv>
	int i;
	// Search "
	for (i = 0; i < n && buf[i] != '\x22'; i++)
		;
	// Skip Pan;
	for (i++; i < n && buf[i] != ';'; i++)
		;
	// Skip Pad
	for (i++; i < n && buf[i] != ';'; i++)
		;
	// Ph
	i++;
	sx_width = stou32def(buf + i, -1, &ep);
	if (sx_width < 0) {
		return false;
	}
	// Pv
	i = ep - buf;
	i++;
	sx_height = stou32def(buf + i, -1);
	if (sx_height < 0) {
		return false;
	}

	// この画像が占める文字数
	auto image_rows = (sx_height + fontheight - 1) / fontheight;
	auto image_cols = (sx_width + fontwidth - 1) / fontwidth;

	if (index < 0) {
		// アイコンの場合は呼び出し側で実施。
	} else {
		// 添付画像の場合、表示位置などを計算。
		auto indent = (indent_depth + 1) * indent_cols;
		if ((max_image_count > 0 && image_count >= max_image_count) ||
		    (indent + image_next_cols + image_cols >= screen_cols))
		{
			// 指定された枚数を超えるか、画像が入らない場合は折り返す
			printf("\r");
			printf(CSI "%dC", indent);
			image_count = 0;
			image_max_rows = 0;
			image_next_cols = 0;
		} else {
			// 前の画像の横に並べる
			if (image_count > 0) {
				if (image_max_rows > 0) {
					printf(CSI "%dA", image_max_rows);
				}
				if (image_next_cols > 0) {
					printf(CSI "%dC", image_next_cols);
				}
			}
		}
	}

	// 最初の1回はすでに buf に入っているのでまず出力して、
	// 次からは順次読みながら最後まで出力。
	do {
		in_sixel = true;
		fwrite(buf, 1, n, stdout);
		fflush(stdout);
		in_sixel = false;

		n = fread(buf, 1, sizeof(buf), cache_file);
	} while (n > 0);

	if (index < 0) {
		// アイコンの場合は呼び出し側で実施。
	} else {
		// 添付画像の場合
		image_count++;
		image_next_cols += image_cols;

		// カーソル位置は同じ列に表示した画像の中で最長のものの下端に揃える
		if (image_max_rows > image_rows) {
			printf(CSI "%dB", image_max_rows - image_rows);
		} else {
			image_max_rows = image_rows;
		}
	}
	return true;
}

// ユーザ一覧を読み込む(共通)。
// フォロー(friends)、ブロックユーザ、ミュートユーザは同じ形式。
// 読み込んだリストを Dictionary 形式で返す。エラーなら終了する。
// funcname はエラー時の表示用。
static StringDictionary
get_paged_list(const std::string& api, const char *funcname)
{
	// ユーザ一覧は一度に全部送られてくるとは限らず、
	// next_cursor{,_str} が 0 なら最終ページ、そうでなければ
	// これを cursor に指定してもう一度リクエストを送る。

	std::string cursor = "-1";
	StringDictionary list;

	do {
		StringDictionary options;
		options["cursor"] = cursor;

		// JSON を取得
		auto json = API2Json("GET", APIROOT, api, options);
		if (json.is_null()) {
			errx(1, "%s API2Json failed", funcname);
		}
		Debug(diag, "json=|%s|", json.dump().c_str());
		if (json.contains("errors")) {
			errx(1, "%s failed: %s", funcname, errors2string(json).c_str());
		}

		auto users = json["ids"];
		for (auto u : users) {
			auto id = u.get<Json::number_integer_t>();
			auto id_str = std::to_string(id);
			list[id_str] = id_str;
		}

		cursor = json.value("next_cursor_str", "");
		Debug(diag, "cursor=|%s|", cursor.c_str());
		if (__predict_false(cursor.empty())) {
			cursor = "0";
		}
	} while (cursor != "0");

	return list;
}

// フォローユーザ一覧の読み込み
StringDictionary
get_follow_list()
{
	return get_paged_list("friends/ids", __func__);
}

// ブロックユーザ一覧の読み込み
StringDictionary
get_block_list()
{
	return get_paged_list("blocks/ids", __func__);
}

// ミュートユーザ一覧の読み込み
StringDictionary
get_mute_list()
{
	return get_paged_list("mutes/users/ids", __func__);
}

// RT非表示ユーザ一覧の読み込み
StringDictionary
get_nort_list()
{
	// ミュートユーザ一覧等とは違って、リスト一発で送られてくるっぽい。
	// ただの数値の配列 [1,2,3,4] の形式。
	// なんであっちこっちで仕様が違うんだよ…。

	StringDictionary nortlist;
	nortlist.clear();

	// JSON を取得
	auto json = API2Json("GET", APIROOT,
		"friendships/no_retweets/ids", {});
	if (json.is_null()) {
		errx(1, "get_nort_list API2Json failed");
	}
	Debug(diag, "json=|%s|", json.dump().c_str());

	if (!json.is_array()) {
		// どうするかね
		return nortlist;
	}

	for (const auto& u : json) {
		auto id = u.get<Json::number_integer_t>();
		auto id_str = std::to_string(id);
		nortlist[id_str] = id_str;
	}

	return nortlist;
}

// API に接続し、その HttpClient を返す。
// エラーが起きた場合は空の unique_ptr を返す。
static std::unique_ptr<HttpClient>
API(const std::string& method, const std::string& apiRoot,
	const std::string& api, const StringDictionary& options)
{
	oauth.AdditionalParams.clear();

	if (!options.empty()) {
		for (const auto& [key, val] : options) {
			oauth.AdditionalParams[key] = val;
		}
	}

	Trace(diag, "CreateHttp call");
	auto client = oauth.CreateHttp(method, apiRoot + api + ".json");
	Trace(diag, "CreateHttp return");
	if ((bool)client == false) {
		return client;
	}

	// Ciphers 指定があれば指示
	if (!opt_ciphers.empty()) {
		client->SetCiphers(opt_ciphers);
	}

	return client;
}

// API に接続し、結果の JSON を返す。
// 接続が失敗、あるいは JSON が正しく受け取れなかった場合は {} を返す。
// recvp が指定されていれば受信ヘッダを返す。
static Json
API2Json(const std::string& method, const std::string& apiRoot,
	const std::string& api, const StringDictionary& options,
	std::vector<std::string> *recvp)
{
	InputStream *stream = NULL;
	std::string line;
	Json json;

	std::unique_ptr<HttpClient> client = API(method, apiRoot, api, options);
	if ((bool)client == false) {
		Debug(diag, "%s: API failed", api.c_str());
		return json;
	}

	Trace(diag, "client.Act call");
	stream = client->Act(method);
	Trace(diag, "client.Act return");
	if (stream == NULL) {
		Debug(diag, "%s: API %s failed", api.c_str(), method.c_str());
		return json;
	}

	if (recvp) {
		*recvp = client->RecvHeaders;
	}

	auto r = stream->ReadLine(&line);
	if (__predict_false(r < 0)) {
		Debug(diag, "%s: ReadLine failed: %s", api.c_str(), strerrno());
		return json;
	}
	Debug(diag, "ReadLine |%s|", line.c_str());

	if (line.empty()) {
		return json;
	}

	return Json::parse(line);
}


// ツイートを保存する
static void
record(const Json& obj)
{
	FILE *fp;

	fp = fopen(record_file.c_str(), "a+");
	if (fp == NULL) {
		return;
	}
	fputs(obj.dump().c_str(), fp);
	fputs("\n", fp);
	fclose(fp);
}

// 古いキャッシュを破棄する
static void
invalidate_cache()
{
	char cmd[1024];

	// アイコンは1か月分くらいか
	snprintf(cmd, sizeof(cmd),
		"find %s -name icon-\\* -type f -atime +30 -exec rm {} +",
		cachedir.c_str());
	system(cmd);

	// 写真は2日分くらいか
	snprintf(cmd, sizeof(cmd),
		"find %s -name http\\* -type f -atime +2 -exec rm {} +",
		cachedir.c_str());
	system(cmd);
}

// API2Json の応答がエラーだった時に表示用文字列に整形して返す。
// if (json.contains("errors")) {
//   auto msg = errors2string(json);
// のように呼び出す。
static std::string
errors2string(const Json& json)
{
	const Json& errors = json["errors"];
	if (errors.is_array()) {
		// エラーが複数返ってきたらどうするかね
		const Json& error = errors[0];
		auto code = error.value("code", 0);
		auto message = error.value("message", "");
		return string_format(": %s(%d)", message.c_str(), code);
	}
	return "";
}
