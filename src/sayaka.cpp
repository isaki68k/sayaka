/*
 * sayaka - twitter client
 */
/*
 * Copyright (C) 2014-2021 Tetsuya Isaki
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

#include "sayaka.h"
#include "Diag.h"
#include "FileInputStream.h"
#include "NGWord.h"
#include "StringUtil.h"
#include "Twitter.h"
#include "subr.h"
#include "term.h"
#include <array>
#include <memory>
#include <cstdio>
#include <cstring>
#include <string>
#include <err.h>
#include <getopt.h>
#include <signal.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/ttycom.h>
#include <sys/ioctl.h>

static const char version[] = "3.5.x (2021/01/09)";

#if !defined(PATH_SEPARATOR)
#define PATH_SEPARATOR "/"
#endif

class MediaInfo
{
 public:
	MediaInfo(const std::string& target_url_, const std::string& display_url_)
	{
		target_url = target_url_;
		display_url = display_url_;
	}

	std::string target_url;
	std::string display_url;
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
	Protected,
	NG,
	Max,
};

enum SayakaCmd {
	Noop = 0,
	Stream,
	Play,
	Tweet,
	Followlist,
	Mutelist,
	NgwordAdd,
	NgwordDel,
	NgwordList,
	Nortlist,
	Blocklist,
	Version,
};

#define CAN "\x18"
#define ESC "\x1b"
#define CSI ESC "["

static const int DEFAULT_FONT_WIDTH = 7;
static const int DEFAULT_FONT_HEIGHT = 14;

static std::string GetHomeDir();
static void init();
static void cmd_tweet();
static void init_stream();
static void progress(const char *msg);
static void cmd_stream();
static void cmd_play();
static void CreateTwitter();
static void get_access_token();
static bool showobject(const std::string& line);
static bool acl(const Json& status, bool is_quoted);
static bool acl_me(const std::string& user_id, const std::string& user_name,
	const StringDictionary& replies);
static bool acl_home(const Json& status, const std::string& user_id,
	const std::string& user_name);
static StringDictionary GetReplies(const Json& status,
	const std::string& user_id, const std::string& user_name);
static bool showstatus(const Json& status, bool is_quoted);
static std::string format_rt_owner(const Json& s);
static std::string format_rt_cnt(const Json& s);
static std::string format_fav_cnt(const Json& s);
static void print_(const std::string& text);
static void init_color();
static std::string str_join(const std::string& sep,
	const std::string& s1, const std::string& s2);
static std::string coloring(const std::string& text, Color col);
class TextTag;
std::string formatmsg(const Json& s, const std::vector<MediaInfo>& mediainfo);
static void show_icon(const Json& user);
static bool show_photo(const std::string& img_url, int resize_width, int index);
static bool show_image(const std::string& img_file, const std::string& img_url,
	int resize_width, int index);
static FILE *fetch_image(const std::string& cache_filename,
	const std::string& img_url, int resize_width);
static void get_credentials();
static StringDictionary get_paged_list(const std::string& api,
	const char *funcname);
static void cmd_users_list(const StringDictionary& list);
static void get_follow_list();
static void cmd_followlist();
static void get_block_list();
static void cmd_blocklist();
static void get_mute_list();
static void cmd_mutelist();
static void get_nort_list();
static void cmd_nortlist();
static void record(const Json& obj);
static void invalidate_cache();
static void cmd_ngword_add();
static void cmd_ngword_del();
static void cmd_ngword_list();
static void signal_handler(int signo);
static void sigwinch();
static void cmd_version();
[[noreturn]] static void usage();

#if defined(SELFTEST)
extern void test_showstatus_acl();
#endif

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

// enum は getopt() の1文字のオプションと衝突しなければいいので
// 適当に 0x80 から始めておく。
enum {
	OPT_black = 0x80,
	OPT_blocklist,
	OPT_ciphers,
	OPT_color,
	OPT_debug,
	OPT_debug_http,
	OPT_debug_image,
	OPT_debug_show,
	OPT_debug_sixel,
	OPT_euc_jp,
	OPT_filter,
	OPT_followlist,
	OPT_font,
	OPT_full_url,
	OPT_home,
	OPT_jis,
	OPT_max_cont,
	OPT_max_image_cols,
	OPT_mutelist,
	OPT_ngword_add,
	OPT_ngword_del,
	OPT_ngword_list,
	OPT_ngword_user,
	OPT_no_color,
	OPT_no_image,
	OPT_no_rest,
	OPT_nortlist,
	OPT_ormode,
	OPT_palette,
	OPT_play,
	OPT_post,
	OPT_progress,
	OPT_record,
	OPT_record_all,
	OPT_show_ng,
	OPT_support_evs,
	OPT_timeout_image,
	OPT_token,
	OPT_version,
	OPT_white,
	OPT_x68k,
};

static const struct option longopts[] = {
	{ "black",			no_argument,		NULL,	OPT_black },
	{ "blocklist",		no_argument,		NULL,	OPT_blocklist },
	{ "ciphers",		required_argument,	NULL,	OPT_ciphers },
	{ "color",			required_argument,	NULL,	OPT_color },
	{ "debug",			required_argument,	NULL,	OPT_debug },
	{ "debug-http",		required_argument,	NULL,	OPT_debug_http },
	{ "debug-image",	required_argument,	NULL,	OPT_debug_image },
	{ "debug-show",		required_argument,	NULL,	OPT_debug_show },
	{ "debug-sixel",	required_argument,	NULL,	OPT_debug_sixel },
	{ "euc-jp",			no_argument,		NULL,	OPT_euc_jp },
	{ "filter",			required_argument,	NULL,	OPT_filter },
	{ "followlist",		no_argument,		NULL,	OPT_followlist },
	{ "font",			required_argument,	NULL,	OPT_font },
	{ "full-url",		no_argument,		NULL,	OPT_full_url },
	{ "home",			no_argument,		NULL,	OPT_home },
	{ "jis",			no_argument,		NULL,	OPT_jis },
	{ "max-cont",		required_argument,	NULL,	OPT_max_cont },
	{ "max-image-cols",	required_argument,	NULL,	OPT_max_image_cols },
	{ "mutelist",		no_argument,		NULL,	OPT_mutelist },
	{ "ngword-add",		required_argument,	NULL,	OPT_ngword_add },
	{ "ngword-del",		required_argument,	NULL,	OPT_ngword_del },
	{ "ngword-list",	no_argument,		NULL,	OPT_ngword_list },
	{ "ngword-user",	required_argument,	NULL,	OPT_ngword_user },
	{ "no-color",		no_argument,		NULL,	OPT_no_color },
	{ "no-image",		no_argument,		NULL,	OPT_no_image },
	{ "no-rest",		no_argument,		NULL,	OPT_no_rest },
	{ "nortlist",		no_argument,		NULL,	OPT_nortlist },
	{ "ormode",			required_argument,	NULL,	OPT_ormode },
	{ "palette",		required_argument,	NULL,	OPT_palette },
	{ "play",			no_argument,		NULL,	OPT_play },
	{ "post",			no_argument,		NULL,	OPT_post },
	{ "progress",		no_argument,		NULL,	OPT_progress },
	{ "record",			required_argument,	NULL,	OPT_record },
	{ "record-all",		required_argument,	NULL,	OPT_record_all },
	{ "show-ng",		no_argument,		NULL,	OPT_show_ng },
	{ "support-evs",	no_argument,		NULL,	OPT_support_evs },
	{ "timeout-image",	required_argument,	NULL,	OPT_timeout_image },
	{ "token",			required_argument,	NULL,	OPT_token },
	{ "version",		no_argument,		NULL,	OPT_version },
	{ "white",			no_argument,		NULL,	OPT_white },
	{ "x68k",			no_argument,		NULL,	OPT_x68k },
	{ "help",			no_argument,		NULL,	'h' },
	{ NULL },
};

int  address_family;			// AF_INET*
bool opt_noimage;				// 画像を表示しないなら true
int  color_mode;				// 色数もしくはカラーモード
Diag diag;						// デバッグ (無分類)
Diag diagHttp;					// デバッグ (HTTP コネクション)
Diag diagImage;					// デバッグ (画像周り)
Diag diagShow;					// デバッグ (メッセージ表示判定)
int  opt_debug_sixel;			// デバッグレベル (SIXEL変換周り)
bool opt_debug;					// デバッグオプション (後方互換用)
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
bool bg_white;					// 明るい背景用に暗い文字色を使う場合は true
std::string iconv_tocode;		// 出力文字コード
std::array<std::string, Color::Max> color2esc;	// 色エスケープ文字列
Twitter tw;
StringDictionary followlist;	// フォロー氏リスト
StringDictionary blocklist;		// ブロック氏リスト
StringDictionary mutelist;		// ミュート氏リスト
StringDictionary nortlist;		// RT非表示氏リスト
bool opt_norest;				// REST API を発行しない
bool opt_evs;					// EVS を使用する
bool opt_show_ng;				// NG ツイートを隠さない
std::string opt_ngword;			// NG ワード (追加削除コマンド用)
std::string opt_ngword_user;	// NG 対象ユーザ (追加コマンド用)
std::string record_file;		// 記録用ファイルパス
std::string opt_filter;			// フィルタキーワード
std::string last_id;			// 直前に表示したツイート
int  last_id_count;				// 連続回数
int  last_id_max;				// 連続回数の上限
bool in_sixel;					// SIXEL 出力中なら true
std::string opt_ciphers;		// 暗号スイート
bool opt_full_url;				// URL を省略表示しない
bool opt_progress;				// 起動時の途中経過表示
NGWord ngword;					// NG ワード
bool opt_ormode;				// SIXEL ORmode で出力するなら true
bool opt_output_palette;		// SIXEL にパレット情報を出力するなら true
int  opt_timeout_image;			// 画像取得の(接続)タイムアウト [msec]
bool opt_pseudo_home;			// 疑似ホームタイムライン
std::string myid;				// 自身の user id
bool opt_nocolor;				// テキストに(色)属性を一切付けない
int  opt_record_mode;			// 0:保存しない 1:表示のみ 2:全部保存
std::string basedir;
std::string cachedir;
std::string tokenfile;
std::string colormapdir;

int
main(int ac, char *av[])
{
	int c;

	diagHttp.SetClassname("HttpClient");

	SayakaCmd cmd = SayakaCmd::Noop;
	basedir     = GetHomeDir() + "/.sayaka/";
	cachedir    = basedir + "cache";
	tokenfile   = basedir + "token.json";
	colormapdir = basedir;
	ngword.SetFileName(basedir + "ngword.json");

	address_family = AF_UNSPEC;
	color_mode = 256;
	bg_white = true;
	opt_evs = false;
	opt_show_ng = false;
	opt_filter = "";
	last_id = "";
	last_id_count = 0;
	last_id_max = 10;
	opt_full_url = false;
	opt_progress = false;
	opt_ormode = false;
	opt_output_palette = true;
	opt_timeout_image = 3000;

	while ((c = getopt_long(ac, av, "46h", longopts, NULL)) != -1) {
		switch (c) {
		 case '4':
			address_family = AF_INET;
			break;
		 case '6':
			address_family = AF_INET6;
			break;
		 case OPT_black:
			bg_white = false;
			break;
		 case OPT_blocklist:
			cmd = SayakaCmd::Blocklist;
			break;
		 case OPT_ciphers:
			opt_ciphers = optarg;
			break;
		 case OPT_color:
			if (strcmp(optarg, "x68k") == 0) {
				color_mode = ColorFixedX68k;
			} else {
				color_mode = atoi(optarg);
			}
			break;
		 case OPT_debug:
			diag.SetLevel(atoi(optarg));
			// とりあえず後方互換
			opt_debug = (diag > 0);
			break;
		 case OPT_debug_http:
			diagHttp.SetLevel(atoi(optarg));
			break;
		 case OPT_debug_image:
			diagImage.SetLevel(atoi(optarg));
			break;
		 case OPT_debug_show:
			diagShow.SetLevel(atoi(optarg));
			break;
		 case OPT_debug_sixel:
			opt_debug_sixel = atoi(optarg);
			max_image_count = 1;
			break;
		 case OPT_euc_jp:
			iconv_tocode = "euc-jp";
			break;
		 case OPT_filter:
			cmd = SayakaCmd::Stream;
			opt_filter = optarg;
			break;
		 case OPT_followlist:
			cmd = SayakaCmd::Followlist;
			break;
		 case OPT_font:
		 {
			// "7x14" のような形式
			std::string str(optarg);
			auto metric = Split(str, "x");
			if (metric.size() != 2) {
				usage();
			}
			opt_fontwidth = std::stoi(metric[0]);
			opt_fontheight = std::stoi(metric[1]);
			break;
		 }
		 case OPT_full_url:
			opt_full_url = true;
			break;
		 case OPT_home:
			cmd = SayakaCmd::Stream;
			opt_pseudo_home = true;
			break;
		 case OPT_jis:
			iconv_tocode = "iso-2022-jp";
			break;
		 case OPT_max_cont:
			last_id_max = atoi(optarg);
			break;
		 case OPT_max_image_cols:
			max_image_count = atoi(optarg);
			if (max_image_count < 1) {
				max_image_count = 0;
			}
			break;
		 case OPT_mutelist:
			cmd = SayakaCmd::Mutelist;
			break;
		 case OPT_ngword_add:
			cmd = SayakaCmd::NgwordAdd;
			opt_ngword = optarg;
			break;
		 case OPT_ngword_del:
			cmd = SayakaCmd::NgwordDel;
			opt_ngword = optarg;
			break;
		 case OPT_ngword_list:
			cmd = SayakaCmd::NgwordList;
			break;
		 case OPT_ngword_user:
			opt_ngword_user = optarg;
			break;
		 case OPT_no_color:
			opt_nocolor = true;
			break;
		 case OPT_no_image:
			opt_noimage = true;
			break;
		 case OPT_no_rest:
			opt_norest = true;
			break;
		 case OPT_nortlist:
			cmd = SayakaCmd::Nortlist;
			break;
		 case OPT_ormode:
			if (strcmp(optarg, "on") == 0) {
				opt_ormode = true;
			} else if (strcmp(optarg, "off") == 0) {
				opt_ormode = false;
			} else {
				usage();
			}
			break;
		 case OPT_palette:
			if (strcmp(optarg, "on") == 0) {
				opt_output_palette = true;
			} else if (strcmp(optarg, "off") == 0) {
				opt_output_palette = false;
			} else {
				usage();
			}
			break;
		 case OPT_play:
			cmd = SayakaCmd::Play;
			break;
		 case OPT_post:
			cmd = SayakaCmd::Tweet;
			break;
		 case OPT_progress:
			opt_progress = true;
			break;
		 case OPT_record:
			if (opt_record_mode != 0) {
				usage();
			}
			opt_record_mode = 1;
			record_file = optarg;
			break;
		 case OPT_record_all:
			if (opt_record_mode != 0) {
				usage();
			}
			opt_record_mode = 2;
			record_file = optarg;
			break;
		 case OPT_show_ng:
			opt_show_ng = true;
			break;
		 case OPT_support_evs:
			opt_evs = true;
			break;
		 case OPT_timeout_image:
			opt_timeout_image = atoi(optarg) * 1000;
			break;
		 case OPT_token:
		 {
			std::string path(optarg);
			if (path.find('/') != std::string::npos) {
				tokenfile = path;
			} else {
				tokenfile = basedir + path;
			}
			break;
		 }
		 case OPT_version:
			cmd = SayakaCmd::Version;
			break;
		 case OPT_white:
			bg_white = true;
			break;
		 case OPT_x68k:
			// 以下を指定したのと同じ
			color_mode = ColorFixedX68k;
			fontwidth = 8;
			fontheight = 16;
			iconv_tocode = "iso-2022-jp";
			bg_white = false;
			opt_progress = true;
			opt_ormode = true;
			opt_output_palette = false;
			break;
		 case 'h':
			usage();
			break;
		 default:
			// 知らない引数はエラー
			usage();
		}
	}
	if (ac > optind) {
		cmd = SayakaCmd::Stream;
		opt_filter = av[optind];
	}

	// --progress ならそれを展開したコマンドラインを表示してみるか
	if (opt_progress) {
		printf("%s", av[0]);
		for (int i = 1; i < ac; i++) {
			if (strcmp(av[i], "--x68k") == 0) {
				printf(" --color x68k --font 8x16 --jis --black"
				       " --progress --ormode on --palette on");
			} else {
				printf(" %s", av[i]);
			}
		}
		printf("\n");
	}

	// usage() は init() より前のほうがいいか。
	if (cmd == SayakaCmd::Noop) {
		usage();
	}

	if (opt_pseudo_home && !opt_filter.empty()) {
		warnx("filter keyword and --home must be exclusive.");
		usage();
	}

	diag.Debug("tokenfile = %s", tokenfile.c_str());
	init();

	// コマンド別処理
	switch (cmd) {
	 case SayakaCmd::Stream:
		init_stream();
		cmd_stream();
		break;
	 case SayakaCmd::Play:
		init_stream();
		cmd_play();
		break;
	 case SayakaCmd::Followlist:
		cmd_followlist();
		break;
	 case SayakaCmd::Mutelist:
		cmd_mutelist();
		break;
	 case SayakaCmd::NgwordAdd:
		cmd_ngword_add();
		break;
	 case SayakaCmd::NgwordDel:
		cmd_ngword_del();
		break;
	 case SayakaCmd::NgwordList:
		cmd_ngword_list();
		break;
	 case SayakaCmd::Nortlist:
		cmd_nortlist();
		break;
	 case SayakaCmd::Blocklist:
		cmd_blocklist();
		break;
	 case SayakaCmd::Tweet:
		cmd_tweet();
		break;
	 case SayakaCmd::Version:
		cmd_version();
		break;
	 default:
		usage();
	}

	return 0;
}

// 初期化
static void
init()
{
	struct stat st;
	int r;

	// ~/.sayaka がなければ作る
	const char *c_basedir = basedir.c_str();
	r = stat(c_basedir, &st);
	if (r < 0 && errno == ENOENT) {
		r = mkdir(c_basedir, 0755);
		if (r < 0) {
			err(1, "init: mkdir %s failed", c_basedir);
		}
		warnx("init: %s is created.", c_basedir);
	}

	// キャッシュディレクトリを作る
	const char *c_cachedir = cachedir.c_str();
	r = stat(c_cachedir, &st);
	if (r < 0 && errno == ENOENT) {
		r = mkdir(c_cachedir, 0755);
		if (r < 0) {
			err(1, "init: mkdir %s failed", c_cachedir);
		}
		warnx("init: %s is created.", c_cachedir);
	}

	// シグナルハンドラを設定
	signal(SIGINT,    signal_handler);
	signal(SIGHUP,    signal_handler);
	signal(SIGPIPE,   signal_handler);
	signal(SIGALRM,   signal_handler);
	signal(SIGXCPU,   signal_handler);
	signal(SIGXFSZ,   signal_handler);
	signal(SIGVTALRM, signal_handler);
	signal(SIGPROF,   signal_handler);
	signal(SIGUSR1,   signal_handler);
	signal(SIGUSR2,   signal_handler);
	// SIGWINCH は *BSD では SA_RESTART が立っていて
	// Linux では立っていないらしい。とりあえず立てておく。
	struct sigaction act;
	memset(&act, 0, sizeof(act));
	act.sa_handler = signal_handler;
	act.sa_flags = SA_RESTART;
	sigaction(SIGWINCH, &act, NULL);
}

// ホームディレクトリを std::string で返す
static std::string
GetHomeDir()
{
	char *home;

	home = getenv("HOME");
	if (home) {
		return home;
	} else {
		return "";
	}
}

// 投稿する
static void
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
	CreateTwitter();

	// 投稿するパラメータを用意
	StringDictionary options;
	options.AddOrUpdate("status", text);
	options.AddOrUpdate("trim_user", "1");

	// 投稿
	auto json = tw.API2Json("POST", Twitter::APIRoot, "statuses/update",
		options);
	if (json.is_null()) {
		errx(1, "statuses/update API2Json failed");
	}
	if (json.contains("errors")) {
		auto errorlist = json["errors"];
		// TODO: エラーが複数返ってきたらどうするかね
		auto code = errorlist[0]["code"].get<int>();
		auto message = errorlist[0]["message"].get<std::string>();
		errx(1, "statuses/update failed: %s(%d)", message.c_str(), code);
	}
	printf("Posted.\n");
}

// ストリームモードのための準備
void
init_stream()
{
	// 端末が SIXEL をサポートしてなければ画像オフ
	if (terminal_support_sixel() == false) {
		if (opt_noimage == false) {
			printf("Terminal doesn't support sixel, switch to --no-image\n");
		}
		opt_noimage = true;
	}

#if notyet
	// 色の初期化
	init_color();

	// 一度手動で呼び出して桁数を取得
	sigwinch();
#endif

	// NG ワード取得
	ngword.ParseFile();
}

// 起動経過を表示 (遅マシン用)
static void
progress(const char *msg)
{
	if (opt_debug || opt_progress) {
		fputs(msg, stdout);
		fflush(stdout);
	}
}

// フィルタストリーム
static void
cmd_stream()
{
	InputStream *stream = NULL;

	// 古いキャッシュを削除
	progress("Deleting expired cache files...");
#if notyet
	invalidate_cache();
	progress("done\n");

	// アクセストークンを取得
	CreateTwitter();

	if (opt_norest == false) {
		if (opt_pseudo_home) {
			// 疑似タイムライン用に自分の ID 取得
			progress("Getting credentials...");
			get_credentials();
			progress("done\n");

			// 疑似タイムライン用にフォローユーザ取得
			progress("Getting follow list...");
			get_follow_list();
			progress("done\n");

			// ストリームの場合だけフォローの中に自身を入れておく。
			// 表示するかどうかの判定はほぼフォローと同じなので。
			followlist.AddOrUpdate(myid, myid);
		}

		// ブロックユーザ取得
		progress("Getting block users list...");
		get_block_list();
		progress("done\n");

		// ミュートユーザ取得
		progress("Getting mute users list...");
		get_mute_list();
		progress("done\n");

		// RT非表示ユーザ取得
		progress("Getting nort users list...");
		get_nort_list();
		progress("done\n");
	}
#endif

	printf("Ready..");
	fflush(stdout);

	// ストリーミング開始
	diag.Debug("PostAPI call");
	{
		StringDictionary dict;
		if (opt_pseudo_home) {
			// 疑似ホームタイムライン
			std::string liststr;
			for (const auto& kv : followlist) {
				const auto& key = kv.first;
				liststr += key + ",";
			}
			// followlist には自分自身を加えているので必ず1人以上いるので、
			// 最後の ',' だけ取り除けば join(",") 相当になる。
			liststr.pop_back();
			dict.AddOrUpdate("follow", liststr);
		} else {
			// キーワード検索
			dict.AddOrUpdate("track", opt_filter);
		}
		stream = tw.PostAPI(Twitter::PublicAPIRoot, "statuses/filter", dict);
		if (stream == NULL) {
			errx(1, "statuses/filter failed");
		}
	}
	printf("Connected.\n");

	for (;;) {
		std::string line;
		if (stream->ReadLine(&line) == false) {
			errx(1, "statuses/filter: ReadLine failed");
		}
		// XXX EOF が分からないね

		if (showobject(line) == false) {
			break;
		}
	}
}

// 再生モード
static void
cmd_play()
{
	FileInputStream stdinstream(stdin, false);
	std::string line;

	while (stdinstream.ReadLine(&line)) {
		if (showobject(line) == false) {
			break;
		}
	}
}

// Twitter オブジェクトを初期化
static void
CreateTwitter()
{
	static bool initialized = false;

	// XXX 元はここで tw を必要なら new していたからこうなっている
	if (!initialized) {
		initialized = true;

		tw.SetDiag(diagHttp);
		get_access_token();

		if (!opt_ciphers.empty()) {
			tw.SetCiphers(opt_ciphers);
		}
	}
}

// アクセストークンを取得する
static void
get_access_token()
{
	bool r;

	// ファイルからトークンを取得
	r = tw.AccessToken.LoadFromFile(tokenfile);
	if (r == false) {
		// なければトークンを取得してファイルに保存
		tw.GetAccessToken();
		if (tw.AccessToken.Token.empty()) {
			errx(1, "GIVE UP");
		}

		r = tw.AccessToken.SaveToFile(tokenfile);
		if (r == false) {
			errx(1, "Token save failed");
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
		diag.Debug("empty line");
		return true;
	}

	// line (文字列) から obj (JSON) に。
	Json obj = Json::parse(line);
	if (obj.is_null()) {
		warnx("%s: Json parser failed.\n"
			"There may be something wrong with twitter.", __func__);
		return false;
	}

	// 全ツイートを録画
	if (opt_record_mode == 2) {
		record(obj);
	}

	if (obj.contains("text")) {
		// 通常のツイート
		bool crlf = showstatus(obj, false);
		if (crlf) {
			printf("\n");
		}
	} else {
		// それ以外はとりあえず無視
	}
	return true;
}

// 表示判定のおおまかなルール
//
// ブロック氏: false
// 俺氏      : true
// * to 俺氏 : true
// ミュート氏: false
// * rt 俺氏: true
// * rt (ブロック to 俺氏): false
// * rt (* to 俺氏): true
//
// if (ホームTL) {
//   RT非表示氏 rt *: false
//   他人氏: false
//   ; これ以降残ってるのはフォロー氏のみ
//   フォロー to 他人: false
// }
//
// * to ブロック: false
// * to ミュート: false
// * rt ブロック: false
// * rt ミュート: false
// * rt (* to ブロック): false
// * rt (* to ミュート): false
// * rt *: true
// *: true

// このツイートを表示するか判定する。表示するなら true。
// NG ワード判定はここではない。
bool
acl(const Json& status, bool is_quoted)
{
	// このツイートの発言者
	const auto& user_id = status["user"]["id_str"].get<std::string>();
	std::string user_name;
	if (diagShow > 0) {
		user_name = status["user"]["screen_name"].get<std::string>();
	}

	// ブロック氏の発言はすべて非表示
	if (blocklist.ContainsKey(user_id)) {
		diagShow.Print(3, "acl: block(@%s) -> false", user_name.c_str());
		return false;
	}

	// このツイートの返信周りを先に洗い出す。
	// (俺氏宛てのために先にここで使うけど、
	// 後からもフォロー同士の関係性を調べるためにまた使う)
	auto replies = GetReplies(status, user_id, user_name);

	// 俺氏発と俺氏宛てはすべて表示
	if (acl_me(user_id, user_name, replies)) {
		return true;
	}

	// 俺氏宛てを表示した後でミュート氏の発言はすべて非表示
	if (mutelist.ContainsKey(user_id)) {
		// フォローしていれば Lv1 で表示する
		// フォローしてなければ Lv3 のみで表示する
		if (diagShow > 0) {
			int lv = followlist.ContainsKey(user_id) ? 1 : 3;
			diagShow.Print(lv, "acl: mute(@%s) -> false", user_name.c_str());
		}
		return false;
	}

	// リツイートを持っていればその中の俺氏関係分だけ表示
	// 俺氏関係分なのでRT非表示氏や他人氏でも可。
	if (status.contains("retweeted_status")) {
		const auto rt_status   = status["retweeted_status"];
		const auto rt_user     = rt_status["user"];
		const auto rt_user_id  = rt_user["id_str"].get<std::string>();
		std::string rt_user_name;
		if (diagShow > 0) {
			rt_user_name = rt_user["screen_name"].get<std::string>();
		}
		const auto rt_replies = GetReplies(rt_status, rt_user_id, rt_user_name);
		if (acl_me(rt_user_id, rt_user_name, rt_replies)) {
			return true;
		}
	}

	// ホーム TL 用の判定
	if (is_quoted == false && opt_pseudo_home) {
		if (acl_home(status, user_id, user_name) == false) {
			return false;
		}
	}

	// ここからはホームでもフィルタでも
	// ブロック氏かミュート氏がどこかに登場するツイートをひたすら弾く。

	// 他人氏を弾いたのでここで返信先関係のデバッグメッセージを表示
	if (diagShow >= 1) {
		diagShow.Print("%s", replies[""].c_str());
		replies.Remove("");
	}

	// ブロック氏宛て、ミュート氏宛てを弾く。
	auto reply_to_follow = false;
	for (const auto& kv : replies) {
		const auto& id = kv.first;
		const auto& name = kv.second;

		if (blocklist.ContainsKey(id)) {
			diagShow.Print(1, "acl: @%s replies block(@%s) -> false",
				user_name.c_str(), name.c_str());
			return false;
		}
		if (mutelist.ContainsKey(id)) {
			diagShow.Print(1, "acl: @%s replies mute(@%s) -> false",
				user_name.c_str(), name.c_str());
			return false;
		}
		if (followlist.ContainsKey(id)) {
			reply_to_follow = true;
		}
	}
	// ホーム TL なら、フォロー氏から他人氏宛てのリプを弾く。
	// この時点で生き残ってる発言者はホーム TL ならフォロー氏だけ。
	if (is_quoted == false && opt_pseudo_home) {
		// 宛先があって、かつ、フォロー氏が一人も含まれてなければ
		if (!replies.empty() && reply_to_follow == false) {
			if (diagShow >= 1) {
				std::string names;
				for (const auto& kv : replies) {
					const auto& name = kv.second;
					if (!names.empty()) {
						names += ",";
					}
					names += "@" + name;
				}
				diagShow.Print("acl: @%s replies others(%s) -> false",
					user_name.c_str(), names.c_str());
			}
			return false;
		}
	}

	// リツイートがあれば
	if (status.contains("retweeted_status")) {
		const auto rt_status = status["retweeted_status"];

		const auto rt_user = rt_status["user"];
		const std::string rt_user_id = rt_user["id_str"].get<std::string>();
		std::string rt_user_name;
		if (diagShow > 0) {
			rt_user_name = rt_user["screen_name"].get<std::string>();
		}

		// RT 先発言者がブロック氏かミュート氏なら弾く
		if (blocklist.ContainsKey(rt_user_id)) {
			diagShow.Print(1, "acl: @%s retweets block(@%s) -> false",
				user_name.c_str(), rt_user_name.c_str());
			return false;
		}
		if (mutelist.ContainsKey(rt_user_id)) {
			diagShow.Print(1, "acl: @%s retweets mute(@%s) -> false",
				user_name.c_str(), rt_user_name.c_str());
			return false;
		}

		// RT 先のリプ先がブロック氏かミュート氏なら弾く
		replies = GetReplies(rt_status, rt_user_id, rt_user_name);
		if (diagShow >= 2) {
			diagShow.Print("%s", replies[""].c_str());
			replies.Remove("");
		}
		for (const auto& kv : replies) {
			const auto& id = kv.first;
			const auto& name = kv.second;
			if (blocklist.ContainsKey(id)) {
				diagShow.Print(1,
					"acl: @%s retweets (* to block(@%s)) -> false",
					user_name.c_str(), name.c_str());
				return false;
			}
			if (mutelist.ContainsKey(id)) {
				diagShow.Print(1,
					"acl: @%s retweets (* to mute(@%s)) -> false",
					user_name.c_str(), name.c_str());
				return false;
			}
		}
	}

	// それ以外のツイートは表示してよい
	return true;
}

// このツイートが俺氏発か俺氏宛てで表示するなら true。
// (本ツイートとリツイート先とから呼ばれる)
static bool
acl_me(const std::string& user_id, const std::string& user_name,
	const StringDictionary& replies)
{
	// user_id(, user_name) はこのツイートの発言者
	// replies はこのツイートの返信関係の情報


	// 俺氏の発言はすべて表示
	if (user_id == myid) {
		diagShow.Print(1, "acl_me: myid -> true");
		return true;
	}

	// 俺氏宛てはブロック以外からは表示
	for (const auto& kv : replies) {
		const auto& id = kv.first;
		if (id == myid) {
			if (blocklist.ContainsKey(user_id)) {
				diagShow.Print(1, "acl_me: block(@%s) to myid -> false",
					user_name.c_str());
				return false;
			}
			if (diagShow >= 2 && replies.ContainsKey("")) {
				diagShow.Print("%s", replies.at("").c_str());
			}
			diagShow.Print(1, "acl_me: * to myid -> true");
			return true;
		}
	}

	return false;
}

// ホーム TL のみで行う追加判定。
static bool
acl_home(const Json& status, const std::string& user_id,
	const std::string& user_name)
{
	// user_id(, user_name) はこのツイートの発言者

	// RT非表示氏のリツイートは弾く。
	if (status.contains("retweeted_status") &&
	    nortlist.ContainsKey(user_id)) {
		diagShow.Print(1, "acl_home: nort(@%s) retweet -> false",
			user_name.c_str());
		return false;
	}

	// 他人氏の発言はもう全部弾いてよい
	if (!followlist.ContainsKey(user_id)) {
		diagShow.Print(3, "acl_home: others(@%s) -> false", user_name.c_str());
		return false;
	}

	// これ以降は、
	// フォロー氏、フォロー氏から誰か宛て、フォロー氏がRT、
	// RT非表示氏、RT非表示氏から誰か宛て、
	// だけになっているので、ホーム/フィルタ共通の判定に戻る。
	return true;
}

// リプライ + ユーザメンションの宛先リストを返す。
//
// 誰か宛てのツイートは
// ・in_reply_to_user_id なし、本文前ユーザメンションあり
//   (誰か宛ての最初のツイート)
// ・in_reply_to_user_id があるけど発言者自身宛て
//   (通常のツリー発言)
// ・in_reply_to_user_id あり、本文前ユーザメンションなし?
//   (返信?)
// ・in_reply_to_user_id あり、本文前ユーザメンションあり
//   (返信?、もしくは複数人との会話)
// が考えられるため、in_reply_to_user_id のユーザだけ見たのではいけない。
//
// in_reply_to_user_id に本文前ユーザメンションの全員を加えてここから
// 発言者本人を引いた集合が、おそらく知りたい宛先リスト。
//
// 戻り値は Dictionary<string,string> で user_id => screen_name に
// なっている (デバッグレベル 0 なら screen_name は "")。
// またデバッグレベル 1 以上なら、"" => debug message というエントリを
// 紛れ込ませてあるので列挙する前に取り除くか、列挙中で避けるかすること。
//
// XXX 本文前ではなく本文内の先頭から始まるメンションはテキスト上
// 見分けが付かないけどこれは無理というか仕様バグでは…。
static StringDictionary
GetReplies(const Json& status,
	const std::string& user_id, const std::string& user_name)
{
	// user_id(, user_name) はこのツイートの発言者

	// display_text_range の一つ目だけ取得。これより前が本文前。
	// なければとりあえず全域としておくか。
	int text_start = 0;
	if (status.contains("display_text_range")) {
		auto text_range = status["display_text_range"];
		text_start = text_range[0].get<int>();
	}

	// ユーザメンション(entities.user_mentions)、なければ空配列
	Json user_mentions = Json::array();
	if (status.contains("entities")) {
		auto entities = status["entities"];
		if (entities.contains("user_mentions")) {
			user_mentions = entities["user_mentions"];
		}
	}
	// screen_name は判定自体には不要なのでデバッグ表示の時だけ有効。
	StringDictionary dict;
	for (const auto& um : user_mentions) {
		// ここで um は user_mentions[] の1人分
		// {
		//   "id":..,
		//   "id_str":"...",
		//   "indices":[start,end],
		//   "name":"...",
		//   "screen_name":"...",
		// }
		int um_start = 0;
		// このユーザメンションの開始位置が
		if (um.contains("indices")) {
			auto indices = um["indices"];
			um_start = indices[0].get<int>();
		}
		// 本文以降なら、これは宛先ではないという認識
		if (um_start >= text_start) {
			continue;
		}

		// dict に追加
		auto id_str = um["id_str"].get<std::string>();
		std::string screen_name;
		if (diagShow > 0) {
			screen_name = um["screen_name"].get<std::string>();
		}
		dict.AddOrUpdate(id_str, screen_name);
	}
	// デバッグメッセージ
	std::string msg;
	std::string msgdict;
	if (diagShow >= 2) {
		msg = "user=@" + user_name;
		for (const auto& kv : dict) {
			const auto& name = kv.second;
			if (!msgdict.empty()) {
				msgdict += ",";
			}
			msgdict += "@" + name;
		}
	}

	// in_reply_to_user_id を追加
	// フィールド自体があって null ということもあるようなので
	// Has() は使わず、最初から文字列評価する (なければ "" になる)。
	auto replyto_id = status["in_reply_to_user_id_str"].get<std::string>();
	if (!replyto_id.empty()) {
		std::string replyto_name;
		if (diagShow > 0) {
			replyto_name = status["in_reply_to_screen_name"].get<std::string>();
			msg += " reply_to=@" + replyto_name;
		}
		dict.AddOrUpdate(replyto_id, replyto_name);
	}

	// デバッグメッセージ
	if (diagShow >= 2) {
		if (!msgdict.empty()) {
			msg += " mention=" + msgdict;
		}
		dict.AddOrUpdate("", msg);
	}

	// ここから発言者自身を引く
	dict.Remove(user_id);

	return dict;
}

#if defined(SELFTEST)
void
test_showstatus_acl()
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
	// h---, f--- はテストしないことを示す。俺氏とブロック氏とに同時に
	// 返信された場合のように判定不能なケースをとりあえず。
	var table = new string[] {
		// 平文
		"{id:1,        home,filt}",		// 俺氏
		"{id:2,        home,filt}",		// フォロー氏
		"{id:4,                 }",		// ミュート氏
		"{id:5,        home,filt}",		// RT非表示氏
		"{id:6,                 }",		// ブロック氏
		"{id:8,             filt}",		// 他人氏

		// 俺氏がリプ
		"{id:1,reply:1,home,filt}",		// 自分自身へ
		"{id:1,reply:2,home,filt}",		// フォローへ
		"{id:1,reply:4,home,filt}",		// ミュートへ
		"{id:1,reply:5,home,filt}",		// RT非表示へ
		"{id:1,reply:6,home,filt}",		// ブロックへ
		"{id:1,reply:8,home,filt}",		// 他人へ

		// フォロー氏がリプ (RT非表示氏も同じになるはずなので以下参照)
		"{id:2,reply:1,home,filt}",		// 自分へ
		"{id:2,reply:2,home,filt}",		// フォローへ
		"{id:2,reply:4,         }",		// ミュートへ
		"{id:2,reply:5,home,filt}",		// RT非表示へ
		"{id:2,reply:6,         }",		// ブロックへ
		"{id:2,reply:8,     filt}",		// 他人へ

		// ミュート氏がリプ
		"{id:4,reply:1,home,filt}",		// 自分へ
		"{id:4,reply:2,         }",		// フォローへ
		"{id:4,reply:4,         }",		// ミュートへ
		"{id:4,reply:5,         }",		// RT非表示へ
		"{id:4,reply:6,         }",		// ブロックへ
		"{id:4,reply:8,         }",		// 他人へ

		// RT非表示氏がリプ (リプはフォロー氏発言と同じ扱いでよいはず)
		"{id:5,reply:1,home,filt}",		// 自分へ
		"{id:5,reply:2,home,filt}",		// フォローへ
		"{id:5,reply:4,         }",		// ミュートへ
		"{id:5,reply:5,home,filt}",		// RT非表示へ
		"{id:5,reply:6,         }",		// ブロックへ
		"{id:5,reply:8,     filt}",		// 他人へ

		// ブロック氏がリプ
		"{id:6,reply:1,         }",		// 自分へ
		"{id:6,reply:2,         }",		// フォローへ
		"{id:6,reply:4,         }",		// ミュートへ
		"{id:6,reply:5,         }",		// RT非表示へ
		"{id:6,reply:6,         }",		// ブロックへ
		"{id:6,reply:8,         }",		// 他人へ

		// 他人氏がリプ
		"{id:8,reply:1,home,filt}",		// 自分へ
		"{id:8,reply:2,     filt}",		// フォローへ
		"{id:8,reply:4,         }",		// ミュートへ
		"{id:8,reply:5,     filt}",		// RT非表示へ
		"{id:8,reply:6,         }",		// ブロックへ
		"{id:8,reply:8,     filt}",		// 他人へ

		// 俺氏、メンションのみ
		"{id:1,ment:1,home,filt}",			// リプなし、UM 俺氏
		"{id:1,ment:2,home,filt}",			// リプなし、UM フォロー氏
		"{id:1,ment:4,home,filt}",			// リプなし、UM ミュート氏
		"{id:1,ment:5,home,filt}",			// リプなし、UM RT非表示氏
		"{id:1,ment:6,home,filt}",			// リプなし、UM ブロック氏
		"{id:1,ment:8,home,filt}",			// リプなし、UM 他人氏

		// フォロー氏、メンションのみ
		"{id:2,ment:1,home,filt}",			// リプなし、UM 俺氏
		"{id:2,ment:2,home,filt}",			// リプなし、UM フォロー氏自
		"{id:2,ment:3,home,filt}",			// リプなし、UM フォロー氏他
		"{id:2,ment:4,         }",			// リプなし、UM ミュート氏
		"{id:2,ment:5,home,filt}",			// リプなし、UM RT非表示氏
		"{id:2,ment:6,         }",			// リプなし、UM ブロック氏
		"{id:2,ment:8,     filt}",			// リプなし、UM 他人氏

		// ミュート氏、メンションのみ
		"{id:4,ment:1,home,filt}",			// リプなし、UM 俺氏
		"{id:4,ment:2,         }",			// リプなし、UM フォロー氏
		"{id:4,ment:4,         }",			// リプなし、UM ミュート氏
		"{id:4,ment:5,         }",			// リプなし、UM RT非表示氏
		"{id:4,ment:6,         }",			// リプなし、UM ブロック氏
		"{id:4,ment:8,         }",			// リプなし、UM 他人氏

		// RT非表示氏、メンションのみ (フォロー氏と同じになる)
		"{id:5,ment:1,home,filt}",			// リプなし、UM 俺氏
		"{id:5,ment:2,home,filt}",			// リプなし、UM フォロー氏
		"{id:5,ment:4,         }",			// リプなし、UM ミュート氏
		"{id:5,ment:5,home,filt}",			// リプなし、UM RT非表示氏
		"{id:5,ment:6,         }",			// リプなし、UM ブロック氏
		"{id:5,ment:8,     filt}",			// リプなし、UM 他人氏

		// ブロック氏、メンションのみ
		"{id:6,ment:1,         }",			// リプなし、UM 俺氏
		"{id:6,ment:2,         }",			// リプなし、UM フォロー氏
		"{id:6,ment:4,         }",			// リプなし、UM ミュート氏
		"{id:6,ment:5,         }",			// リプなし、UM RT非表示氏
		"{id:6,ment:6,         }",			// リプなし、UM ブロック氏
		"{id:6,ment:8,         }",			// リプなし、UM 他人氏

		// 他人氏、メンションのみ
		"{id:8,ment:1,home,filt}",			// リプなし、UM 俺氏
		"{id:8,ment:2,     filt}",			// リプなし、UM フォロー氏
		"{id:8,ment:4,         }",			// リプなし、UM ミュート氏
		"{id:8,ment:5,     filt}",			// リプなし、UM RT非表示氏
		"{id:8,ment:6,         }",			// リプなし、UM ブロック氏
		"{id:8,ment:8,     filt}",			// リプなし、UM 他人氏

		// 俺氏、リプ+メンション
		"{id:1,reply:1,ment:1,home,filt}",	// rep俺氏、UM 俺氏
		"{id:1,reply:1,ment:2,home,filt}",	// rep俺氏、UM フォロー氏
		"{id:1,reply:1,ment:4,home,filt}",	// rep俺氏、UM ミュート氏
		"{id:1,reply:1,ment:5,home,filt}",	// rep俺氏、UM RT非表示氏
		"{id:1,reply:1,ment:6,home,filt}",	// rep俺氏、UM ブロック氏
		"{id:1,reply:1,ment:8,home,filt}",	// rep俺氏、UM 他人氏
		"{id:1,reply:2,ment:1,home,filt}",	// repフォロー氏、UM 俺氏
		"{id:1,reply:2,ment:2,home,filt}",	// repフォロー氏、UM フォロー氏
		"{id:1,reply:2,ment:4,home,filt}",	// repフォロー氏、UM ミュート氏
		"{id:1,reply:2,ment:5,home,filt}",	// repフォロー氏、UM RT非表示氏
		"{id:1,reply:2,ment:6,home,filt}",	// repフォロー氏、UM ブロック氏
		"{id:1,reply:2,ment:8,home,filt}",	// repフォロー氏、UM 他人氏
		"{id:1,reply:4,ment:1,home,filt}",	// repミュート氏、UM 俺氏
		"{id:1,reply:4,ment:2,home,filt}",	// repミュート氏、UM フォロー氏
		"{id:1,reply:4,ment:4,home,filt}",	// repミュート氏、UM ミュート氏
		"{id:1,reply:4,ment:5,home,filt}",	// repミュート氏、UM RT非表示氏
		"{id:1,reply:4,ment:6,home,filt}",	// repミュート氏、UM ブロック氏
		"{id:1,reply:4,ment:8,home,filt}",	// repミュート氏、UM 他人氏
		"{id:1,reply:5,ment:1,home,filt}",	// repRT非表示氏、UM 俺氏
		"{id:1,reply:5,ment:2,home,filt}",	// repRT非表示氏、UM フォロー氏
		"{id:1,reply:5,ment:4,home,filt}",	// repRT非表示氏、UM ミュート氏
		"{id:1,reply:5,ment:5,home,filt}",	// repRT非表示氏、UM RT非表示氏
		"{id:1,reply:5,ment:6,home,filt}",	// repRT非表示氏、UM ブロック氏
		"{id:1,reply:5,ment:8,home,filt}",	// repRT非表示氏、UM 他人氏
		"{id:1,reply:6,ment:1,home,filt}",	// repブロック氏、UM 俺氏
		"{id:1,reply:6,ment:2,home,filt}",	// repブロック氏、UM フォロー氏
		"{id:1,reply:6,ment:4,home,filt}",	// repブロック氏、UM ミュート氏
		"{id:1,reply:6,ment:5,home,filt}",	// repブロック氏、UM RT非表示氏
		"{id:1,reply:6,ment:6,home,filt}",	// repブロック氏、UM ブロック氏
		"{id:1,reply:6,ment:8,home,filt}",	// repブロック氏、UM 他人氏
		"{id:1,reply:8,ment:1,home,filt}",	// rep他人氏、UM 俺氏
		"{id:1,reply:8,ment:2,home,filt}",	// rep他人氏、UM フォロー氏
		"{id:1,reply:8,ment:4,home,filt}",	// rep他人氏、UM ミュート氏
		"{id:1,reply:8,ment:5,home,filt}",	// rep他人氏、UM RT非表示氏
		"{id:1,reply:8,ment:6,home,filt}",	// rep他人氏、UM ブロック氏
		"{id:1,reply:8,ment:8,home,filt}",	// rep他人氏、UM 他人氏

		// フォロー氏、リプ+メンション
		"{id:2,reply:1,ment:1,home,filt}",	// rep俺氏、UM 俺氏
		"{id:2,reply:1,ment:2,home,filt}",	// rep俺氏、UM フォロー氏
		"{id:2,reply:1,ment:4,home,filt}",	// rep俺氏、UM ミュート氏
		"{id:2,reply:1,ment:5,home,filt}",	// rep俺氏、UM RT非表示氏
		"{id:2,reply:1,ment:6,h---,f---}",	// rep俺氏、UM ブロック氏
		"{id:2,reply:1,ment:8,home,filt}",	// rep俺氏、UM 他人氏
		"{id:2,reply:2,ment:1,home,filt}",	// repフォロー氏、UM 俺氏
		"{id:2,reply:2,ment:2,home,filt}",	// repフォロー自、UM フォロー氏
		"{id:2,reply:2,ment:3,home,filt}",	// repフォロー他、UM フォロー氏
		"{id:2,reply:2,ment:4,         }",	// repフォロー自、UM ミュート氏
		"{id:2,reply:2,ment:5,home,filt}",	// repフォロー自、UM RT非表示氏
		"{id:2,reply:2,ment:6,         }",	// repフォロー自、UM ブロック氏
		"{id:2,reply:2,ment:8,     filt}",	// repフォロー自、UM 他人氏
		"{id:2,reply:4,ment:1,home,filt}",	// repミュート氏、UM 俺氏
		"{id:2,reply:4,ment:2,         }",	// repミュート氏、UM フォロー自
		"{id:2,reply:4,ment:3,         }",	// repミュート氏、UM フォロー他
		"{id:2,reply:4,ment:4,         }",	// repミュート氏、UM ミュート氏
		"{id:2,reply:4,ment:5,         }",	// repミュート氏、UM RT非表示氏
		"{id:2,reply:4,ment:6,         }",	// repミュート氏、UM ブロック氏
		"{id:2,reply:4,ment:8,         }",	// repミュート氏、UM 他人氏
		"{id:2,reply:5,ment:1,home,filt}",	// repRT非表示氏、UM 俺氏
		"{id:2,reply:5,ment:2,home,filt}",	// repRT非表示氏、UM フォロー氏
		"{id:2,reply:5,ment:4,         }",	// repRT非表示氏、UM ミュート氏
		"{id:2,reply:5,ment:5,home,filt}",	// repRT非表示氏、UM RT非表示氏
		"{id:2,reply:5,ment:6,         }",	// repRT非表示氏、UM ブロック氏
		"{id:2,reply:5,ment:8,home,filt}",	// repRT非表示氏、UM 他人氏
		"{id:2,reply:6,ment:1,h---,f---}",	// repブロック氏、UM 俺氏
		"{id:2,reply:6,ment:2,         }",	// repブロック氏、UM フォロー自
		"{id:2,reply:6,ment:3,         }",	// repブロック氏、UM フォロー他
		"{id:2,reply:6,ment:4,         }",	// repブロック氏、UM ミュート氏
		"{id:2,reply:6,ment:5,         }",	// repブロック氏、UM RT非表示氏
		"{id:2,reply:6,ment:6,         }",	// repブロック氏、UM ブロック氏
		"{id:2,reply:6,ment:8,         }",	// repブロック氏、UM 他人氏
		"{id:2,reply:8,ment:1,home,filt}",	// rep他人氏、UM 俺氏
		"{id:2,reply:8,ment:2,     filt}",	// rep他人氏、UM フォロー氏
		"{id:2,reply:8,ment:4,         }",	// rep他人氏、UM ミュート氏
		"{id:2,reply:8,ment:5,home,filt}",	// rep他人氏、UM RT非表示氏
		"{id:2,reply:8,ment:6,         }",	// rep他人氏、UM ブロック氏
		"{id:2,reply:8,ment:8,     filt}",	// rep他人氏、UM 他人氏

		// ミュート氏、リプ+メンション
		"{id:4,reply:1,ment:1,home,filt}",	// rep俺氏、UM 俺氏
		"{id:4,reply:1,ment:2,home,filt}",	// rep俺氏、UM フォロー氏
		"{id:4,reply:1,ment:4,home,filt}",	// rep俺氏、UM ミュート氏
		"{id:4,reply:1,ment:5,home,filt}",	// rep俺氏、UM RT非表示氏
		"{id:4,reply:1,ment:6,h---,f---}",	// rep俺氏、UM ブロック氏
		"{id:4,reply:1,ment:8,home,filt}",	// rep俺氏、UM 他人氏
		"{id:4,reply:2,ment:1,home,filt}",	// repフォロー氏、UM 俺氏
		"{id:4,reply:2,ment:2,         }",	// repフォロー氏、UM フォロー氏
		"{id:4,reply:2,ment:4,         }",	// repフォロー氏、UM ミュート氏
		"{id:4,reply:2,ment:5,         }",	// repフォロー氏、UM RT非表示氏
		"{id:4,reply:2,ment:6,         }",	// repフォロー氏、UM ブロック氏
		"{id:4,reply:2,ment:8,         }",	// repフォロー氏、UM 他人氏
		"{id:4,reply:4,ment:1,home,filt}",	// repミュート氏、UM 俺氏
		"{id:4,reply:4,ment:2,         }",	// repミュート氏、UM フォロー氏
		"{id:4,reply:4,ment:4,         }",	// repミュート氏、UM ミュート氏
		"{id:4,reply:4,ment:5,         }",	// repミュート氏、UM RT非表示氏
		"{id:4,reply:4,ment:6,         }",	// repミュート氏、UM ブロック氏
		"{id:4,reply:4,ment:8,         }",	// repミュート氏、UM 他人氏
		"{id:4,reply:5,ment:1,home,filt}",	// repRT非表示氏、UM 俺氏
		"{id:4,reply:5,ment:2,         }",	// repRT非表示氏、UM フォロー氏
		"{id:4,reply:5,ment:4,         }",	// repRT非表示氏、UM ミュート氏
		"{id:4,reply:5,ment:5,         }",	// repRT非表示氏、UM RT非表示氏
		"{id:4,reply:5,ment:6,         }",	// repRT非表示氏、UM ブロック氏
		"{id:4,reply:5,ment:8,         }",	// repRT非表示氏、UM 他人氏
		"{id:4,reply:6,ment:1,h---,f---}",	// repブロック氏、UM 俺氏
		"{id:4,reply:6,ment:2,         }",	// repブロック氏、UM フォロー氏
		"{id:4,reply:6,ment:4,         }",	// repブロック氏、UM ミュート氏
		"{id:4,reply:6,ment:5,         }",	// repブロック氏、UM RT非表示氏
		"{id:4,reply:6,ment:6,         }",	// repブロック氏、UM ブロック氏
		"{id:4,reply:6,ment:8,         }",	// repブロック氏、UM 他人氏
		"{id:4,reply:8,ment:1,home,filt}",	// rep他人氏、UM 俺氏
		"{id:4,reply:8,ment:2,         }",	// rep他人氏、UM フォロー氏
		"{id:4,reply:8,ment:4,         }",	// rep他人氏、UM ミュート氏
		"{id:4,reply:8,ment:5,         }",	// rep他人氏、UM RT非表示氏
		"{id:4,reply:8,ment:6,         }",	// rep他人氏、UM ブロック氏
		"{id:4,reply:8,ment:8,         }",	// rep他人氏、UM 他人氏

		// RT非表示氏、リプ+メンション
		"{id:5,reply:1,ment:1,home,filt}",	// rep俺氏、UM 俺氏
		"{id:5,reply:1,ment:2,home,filt}",	// rep俺氏、UM フォロー氏
		"{id:5,reply:1,ment:4,home,filt}",	// rep俺氏、UM ミュート氏
		"{id:5,reply:1,ment:5,home,filt}",	// rep俺氏、UM RT非表示氏
		"{id:5,reply:1,ment:6,h---,f---}",	// rep俺氏、UM ブロック氏
		"{id:5,reply:1,ment:8,home,filt}",	// rep俺氏、UM 他人氏
		"{id:5,reply:2,ment:1,home,filt}",	// repフォロー氏、UM 俺氏
		"{id:5,reply:2,ment:2,home,filt}",	// repフォロー氏、UM フォロー氏
		"{id:5,reply:2,ment:4,         }",	// repフォロー氏、UM ミュート氏
		"{id:5,reply:2,ment:5,home,filt}",	// repフォロー氏、UM RT非表示氏
		"{id:5,reply:2,ment:6,         }",	// repフォロー氏、UM ブロック氏
		"{id:5,reply:2,ment:8,home,filt}",	// repフォロー氏、UM 他人氏
		"{id:5,reply:4,ment:1,home,filt}",	// repミュート氏、UM 俺氏
		"{id:5,reply:4,ment:2,         }",	// repミュート氏、UM フォロー氏
		"{id:5,reply:4,ment:4,         }",	// repミュート氏、UM ミュート氏
		"{id:5,reply:4,ment:5,         }",	// repミュート氏、UM RT非表示氏
		"{id:5,reply:4,ment:6,         }",	// repミュート氏、UM ブロック氏
		"{id:5,reply:4,ment:8,         }",	// repミュート氏、UM 他人氏
		"{id:5,reply:5,ment:1,home,filt}",	// repRT非表示氏、UM 俺氏
		"{id:5,reply:5,ment:2,home,filt}",	// repRT非表示氏、UM フォロー氏
		"{id:5,reply:5,ment:4,         }",	// repRT非表示氏、UM ミュート氏
		"{id:5,reply:5,ment:5,home,filt}",	// repRT非表示氏、UM RT非表示氏
		"{id:5,reply:5,ment:6,         }",	// repRT非表示氏、UM ブロック氏
		"{id:5,reply:5,ment:8,     filt}",	// repRT非表示氏、UM 他人氏
		"{id:5,reply:6,ment:1,h---,f---}",	// repブロック氏、UM 俺氏
		"{id:5,reply:6,ment:2,         }",	// repブロック氏、UM フォロー氏
		"{id:5,reply:6,ment:4,         }",	// repブロック氏、UM ミュート氏
		"{id:5,reply:6,ment:5,         }",	// repブロック氏、UM RT非表示氏
		"{id:5,reply:6,ment:6,         }",	// repブロック氏、UM ブロック氏
		"{id:5,reply:6,ment:8,         }",	// repブロック氏、UM 他人氏
		"{id:5,reply:8,ment:1,home,filt}",	// rep他人氏、UM 俺氏
		"{id:5,reply:8,ment:2,home,filt}",	// rep他人氏、UM フォロー氏
		"{id:5,reply:8,ment:4,         }",	// rep他人氏、UM ミュート氏
		"{id:5,reply:8,ment:5,     filt}",	// rep他人氏、UM RT非表示氏
		"{id:5,reply:8,ment:6,         }",	// rep他人氏、UM ブロック氏
		"{id:5,reply:8,ment:8,     filt}",	// rep他人氏、UM 他人氏

		// ブロック氏、リプ+メンション
		"{id:6,reply:1,ment:1,h---,f---}",	// rep俺氏、UM 俺氏
		"{id:6,reply:1,ment:2,h---,f---}",	// rep俺氏、UM フォロー氏
		"{id:6,reply:1,ment:4,h---,f---}",	// rep俺氏、UM ミュート氏
		"{id:6,reply:1,ment:5,h---,f---}",	// rep俺氏、UM RT非表示氏
		"{id:6,reply:1,ment:6,h---,f---}",	// rep俺氏、UM ブロック氏
		"{id:6,reply:1,ment:8,h---,f---}",	// rep俺氏、UM 他人氏
		"{id:6,reply:2,ment:1,h---,f---}",	// repフォロー氏、UM 俺氏
		"{id:6,reply:2,ment:2,         }",	// repフォロー自、UM フォロー氏
		"{id:6,reply:2,ment:3,         }",	// repフォロー他、UM フォロー氏
		"{id:6,reply:2,ment:4,         }",	// repフォロー自、UM ミュート氏
		"{id:6,reply:2,ment:5,         }",	// repフォロー自、UM RT非表示氏
		"{id:6,reply:2,ment:6,         }",	// repフォロー自、UM ブロック氏
		"{id:6,reply:2,ment:8,         }",	// repフォロー自、UM 他人氏
		"{id:6,reply:4,ment:1,h---,f---}",	// repミュート氏、UM 俺氏
		"{id:6,reply:4,ment:2,         }",	// repミュート氏、UM フォロー氏
		"{id:6,reply:4,ment:4,         }",	// repミュート氏、UM ミュート氏
		"{id:6,reply:4,ment:5,         }",	// repミュート氏、UM RT非表示氏
		"{id:6,reply:4,ment:6,         }",	// repミュート氏、UM ブロック氏
		"{id:6,reply:4,ment:8,         }",	// repミュート氏、UM 他人氏
		"{id:6,reply:5,ment:1,h---,f---}",	// repRT非表示氏、UM 俺氏
		"{id:6,reply:5,ment:2,         }",	// repRT非表示氏、UM フォロー氏
		"{id:6,reply:5,ment:4,         }",	// repRT非表示氏、UM ミュート氏
		"{id:6,reply:5,ment:5,         }",	// repRT非表示氏、UM RT非表示氏
		"{id:6,reply:5,ment:6,         }",	// repRT非表示氏、UM ブロック氏
		"{id:6,reply:5,ment:8,         }",	// repRT非表示氏、UM 他人氏
		"{id:6,reply:6,ment:1,h---,f---}",	// repブロック氏、UM 俺氏
		"{id:6,reply:6,ment:2,         }",	// repブロック氏、UM フォロー氏
		"{id:6,reply:6,ment:4,         }",	// repブロック氏、UM ミュート氏
		"{id:6,reply:6,ment:5,         }",	// repブロック氏、UM RT非表示氏
		"{id:6,reply:6,ment:6,         }",	// repブロック氏、UM ブロック氏
		"{id:6,reply:6,ment:8,         }",	// repブロック氏、UM 他人氏
		"{id:6,reply:8,ment:1,h---,f---}",	// rep他人氏、UM 俺氏
		"{id:6,reply:8,ment:2,         }",	// rep他人氏、UM フォロー氏
		"{id:6,reply:8,ment:4,         }",	// rep他人氏、UM ミュート氏
		"{id:6,reply:8,ment:5,         }",	// rep他人氏、UM RT非表示氏
		"{id:6,reply:8,ment:6,         }",	// rep他人氏、UM ブロック氏
		"{id:6,reply:8,ment:8,         }",	// rep他人氏、UM 他人氏

		// 他人氏、リプ+メンション
		"{id:8,reply:1,ment:1,home,filt}",	// rep俺氏、UM 俺氏
		"{id:8,reply:1,ment:2,home,filt}",	// rep俺氏、UM フォロー氏
		"{id:8,reply:1,ment:4,home,filt}",	// rep俺氏、UM ミュート氏
		"{id:8,reply:1,ment:5,home,filt}",	// rep俺氏、UM RT非表示氏
		"{id:8,reply:1,ment:6,h---,f---}",	// rep俺氏、UM ブロック氏
		"{id:8,reply:1,ment:8,home,filt}",	// rep俺氏、UM 他人氏
		"{id:8,reply:2,ment:1,home,filt}",	// repフォロー氏、UM 俺氏
		"{id:8,reply:2,ment:2,     filt}",	// repフォロー氏、UM フォロー氏
		"{id:8,reply:2,ment:4,         }",	// repフォロー氏、UM ミュート氏
		"{id:8,reply:2,ment:5,     filt}",	// repフォロー氏、UM RT非表示氏
		"{id:8,reply:2,ment:6,         }",	// repフォロー氏、UM ブロック氏
		"{id:8,reply:2,ment:8,     filt}",	// repフォロー氏、UM 他人氏
		"{id:8,reply:4,ment:1,home,filt}",	// repミュート氏、UM 俺氏
		"{id:8,reply:4,ment:2,         }",	// repミュート氏、UM フォロー氏
		"{id:8,reply:4,ment:4,         }",	// repミュート氏、UM ミュート氏
		"{id:8,reply:4,ment:5,         }",	// repミュート氏、UM RT非表示氏
		"{id:8,reply:4,ment:6,         }",	// repミュート氏、UM ブロック氏
		"{id:8,reply:4,ment:8,         }",	// repミュート氏、UM 他人氏
		"{id:8,reply:5,ment:1,home,filt}",	// repRT非表示氏、UM 俺氏
		"{id:8,reply:5,ment:2,     filt}",	// repRT非表示氏、UM フォロー氏
		"{id:8,reply:5,ment:4,         }",	// repRT非表示氏、UM ミュート氏
		"{id:8,reply:5,ment:5,     filt}",	// repRT非表示氏、UM RT非表示氏
		"{id:8,reply:5,ment:6,         }",	// repRT非表示氏、UM ブロック氏
		"{id:8,reply:5,ment:8,     filt}",	// repRT非表示氏、UM 他人氏
		"{id:8,reply:6,ment:1,h---,f---}",	// repブロック氏、UM 俺氏
		"{id:8,reply:6,ment:2,         }",	// repブロック氏、UM フォロー氏
		"{id:8,reply:6,ment:4,         }",	// repブロック氏、UM ミュート氏
		"{id:8,reply:6,ment:5,         }",	// repブロック氏、UM RT非表示氏
		"{id:8,reply:6,ment:6,         }",	// repブロック氏、UM ブロック氏
		"{id:8,reply:6,ment:8,         }",	// repブロック氏、UM 他人氏
		"{id:8,reply:8,ment:1,home,filt}",	// rep他人氏、UM 俺氏
		"{id:8,reply:8,ment:2,     filt}",	// rep他人氏、UM フォロー氏
		"{id:8,reply:8,ment:4,         }",	// rep他人氏、UM ミュート氏
		"{id:8,reply:8,ment:5,     filt}",	// rep他人氏、UM RT非表示氏
		"{id:8,reply:8,ment:6,         }",	// rep他人氏、UM ブロック氏
		"{id:8,reply:8,ment:8,     filt}",	// rep他人氏、UM 他人氏

		// 俺氏がリツイート
		"{id:1,rt:1,home,filt}",		// 自分自身を
		"{id:1,rt:2,home,filt}",		// フォローを
		"{id:1,rt:4,home,filt}",		// ミュートを
		"{id:1,rt:5,home,filt}",		// RT非表示を
		"{id:1,rt:6,home,filt}",		// ブロックを
		"{id:1,rt:8,home,filt}",		// 他人を

		// フォロー氏がリツイート
		"{id:2,rt:1,home,filt}",		// 自分を
		"{id:2,rt:2,home,filt}",		// フォローを
		"{id:2,rt:4,         }",		// ミュートを
		"{id:2,rt:5,home,filt}",		// RT非表示を
		"{id:2,rt:6,         }",		// ブロックを
		"{id:2,rt:8,home,filt}",		// 他人を

		// ミュート氏がリツイート
		"{id:4,rt:1,         }",		// 自分を
		"{id:4,rt:2,         }",		// フォローを
		"{id:4,rt:4,         }",		// ミュートを
		"{id:4,rt:5,         }",		// RT非表示を
		"{id:4,rt:6,         }",		// ブロックを
		"{id:4,rt:8,         }",		// 他人を

		// RT非表示氏がリツイート
		// 自分の発言をリツイートは表示してもいいだろう
		// フィルタストリームなら表示してもいいだろうか
		"{id:5,rt:1,home,filt}",		// 自分を
		"{id:5,rt:2,     filt}",		// フォローを
		"{id:5,rt:4,         }",		// ミュートを
		"{id:5,rt:5,     filt}",		// RT非表示を
		"{id:5,rt:6,         }",		// ブロックを
		"{id:5,rt:8,     filt}",		// 他人を

		// ブロック氏がリツイート (そもそも来ないような気がするけど一応)
		"{id:6,rt:1,         }",		// 自分を
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
		"{id:2,rt:1,rt_rep:6,home,filt}",	// 俺氏からブロック宛リプ
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
		"{id:4,rt:1,rt_rep:1,         }",	// 俺氏から俺氏宛リプ
		"{id:4,rt:1,rt_rep:2,         }",	// 俺氏からフォロー宛リプ
		"{id:4,rt:1,rt_rep:4,         }",	// 俺氏からミュート宛リプ
		"{id:4,rt:1,rt_rep:5,         }",	// 俺氏からRT非表示宛リプ
		"{id:4,rt:1,rt_rep:6,         }",	// 俺氏からブロック宛リプ
		"{id:4,rt:1,rt_rep:8,         }",	// 俺氏から他人宛リプ
		"{id:4,rt:2,rt_rep:1,         }",	// フォローから俺氏宛リプ
		"{id:4,rt:2,rt_rep:2,         }",	// フォローからフォロー宛リプ
		"{id:4,rt:2,rt_rep:4,         }",	// フォローからミュート宛リプ
		"{id:4,rt:2,rt_rep:5,         }",	// フォローからRT非表示宛リプ
		"{id:4,rt:2,rt_rep:6,         }",	// フォローからブロック宛リプ
		"{id:4,rt:2,rt_rep:8,         }",	// フォローから他人宛リプ
		"{id:4,rt:4,rt_rep:1,         }",	// ミュートから俺氏宛リプ
		"{id:4,rt:4,rt_rep:2,         }",	// ミュートからフォロー宛リプ
		"{id:4,rt:4,rt_rep:4,         }",	// ミュートからミュート宛リプ
		"{id:4,rt:4,rt_rep:5,         }",	// ミュートからRT非表示宛リプ
		"{id:4,rt:4,rt_rep:6,         }",	// ミュートからブロック宛リプ
		"{id:4,rt:4,rt_rep:8,         }",	// ミュートから他人宛リプ
		"{id:4,rt:5,rt_rep:1,         }",	// RT非表示から俺氏宛リプ
		"{id:4,rt:5,rt_rep:2,         }",	// RT非表示からフォロー宛リプ
		"{id:4,rt:5,rt_rep:4,         }",	// RT非表示からミュート宛リプ
		"{id:4,rt:5,rt_rep:5,         }",	// RT非表示からRT非表示宛リプ
		"{id:4,rt:5,rt_rep:6,         }",	// RT非表示からブロック宛リプ
		"{id:4,rt:5,rt_rep:8,         }",	// RT非表示から他人宛リプ
		"{id:4,rt:6,rt_rep:1,         }",	// ブロックから俺氏宛リプ
		"{id:4,rt:6,rt_rep:2,         }",	// ブロックからフォロー宛リプ
		"{id:4,rt:6,rt_rep:4,         }",	// ブロックからブロック宛リプ
		"{id:4,rt:6,rt_rep:5,         }",	// ブロックからRT非表示宛リプ
		"{id:4,rt:6,rt_rep:6,         }",	// ブロックからブロック宛リプ
		"{id:4,rt:6,rt_rep:8,         }",	// ブロックから他人宛リプ
		"{id:4,rt:8,rt_rep:1,         }",	// 他人から俺氏宛リプ
		"{id:4,rt:8,rt_rep:2,         }",	// 他人からフォロー宛リプ
		"{id:4,rt:8,rt_rep:4,         }",	// 他人からブロック宛リプ
		"{id:4,rt:8,rt_rep:5,         }",	// 他人からRT非表示宛リプ
		"{id:4,rt:8,rt_rep:6,         }",	// 他人からブロック宛リプ
		"{id:4,rt:8,rt_rep:8,         }",	// 他人から他人宛リプ
		// 他人がリツイート
		"{id:8,rt:1,rt_rep:1,home,filt}",	// 俺氏から俺氏宛リプ
		"{id:8,rt:1,rt_rep:2,home,filt}",	// 俺氏からフォロー宛リプ
		"{id:8,rt:1,rt_rep:4,home,filt}",	// 俺氏からミュート宛リプ
		"{id:8,rt:1,rt_rep:5,home,filt}",	// 俺氏からRT非表示宛リプ
		"{id:8,rt:1,rt_rep:6,home,filt}",	// 俺氏からブロック宛リプ
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
	int ntest = 0;
	int nfail = 0;
	for (const auto& input_sq : table) {
		auto input_str = input_sq.replace(" ", "")
			.replace("id:",		"\"id\":")
			.replace("reply:",	"\"reply\":")
			.replace("rt:",		"\"rt\":")
			.replace("rt_rep:",	"\"rt_rep\":")
			.replace("ment:",	"\"ment\":")
			.replace("home",	"\"home\":1")
			.replace("filt",	"\"filt\":1")
			.replace("h---",	"\"home\":-1")
			.replace("f---",	"\"filt\":-1")
			// 末尾カンマは許容しておいてここで消すほうが楽
			.replace(",}",		"}")
		;
		Json input = Json::parse(input_str);
		if (input.is_null()) {
			printf("Json::parse(%s) failed\n", input_str.c_str());
			exit(1);
		}

		// それらから status をでっちあげる
		Json status;
		// user
		int id = input["id"].get<int>();
		auto id_str = std::to_string(id);
		Json user;
		user["id_str"] = id_str;
		user["screen_name"] = id_str;
		status["user"] = user;
		// in_reply_to_user_id_str
		if (input.contains("reply")) {
			int reply = input["reply"].get<int>();
			auto reply_str = std::to_string(reply);
			status["in_reply_to_user_id_str"] = reply_str;
			status["in_reply_to_screen_name"] = reply_str;
		}
		// retweeted_status.user.id_str
		if (input.contains("rt")) {
			Json rt;

			int rtid = input["rt"].get<int>();
			auto rtid_str = std::to_string(rtid);
			Json rtuser;
			rtuser["id_str"] = rtid_str;
			rtuser["screen_name"] = rtid_str;
			rt["user"] = rtuser;

			// retweeted_status.in_reply_to_user_id_str
			if (input.contains("rt_rep")) {
				int rtrep = input["rt_rep"].get<int>();
				auto rtrep_str = std::to_string(rtrep);
				rt["in_reply_to_user_id_str"] = rtrep_str;
				rt["in_reply_to_screen_name"] = rtrep_str;
			}

			status["retweeted_status"] = rt;
		}
		// entities.user_mentions[]
		if (input.contains("ment")) {
			int umid = input["ment"].get<int>();
			auto umid_str = std::to_string(umid);
			Json um;
			um["id_str"] = umid_str;
			um["screen_name"] = umid_str;
			um["indices"] = { 0, 2 };
			status["entities"]["user_mentions"] = { um };

			// display_text_range
			status["display_text_range"] = { 3, 5 };
		}	

		// 期待値 (入力は 1=true, 0=false, -1 ならテストしない)
		std::optional<bool> expected_home;
		std::optional<bool> expected_filt;
		auto expected_home_int = input["home"].get<int>();
		auto expected_filt_int = input["filt"].get<int>();
		if (expected_home_int != -1)
			expected_home = (bool)expected_home_int;
		if (expected_filt_int != -1)
			expected_filt = (bool)expected_filt_int;

		if (diagShow > 0) {
			// 歴史的経緯により Diag は stderr、テスト結果は stdout に出るので
			// おそらく |& とかして表示しないといけないことになる。
			fprintf(stderr, "%s\n", input_str.c_str());
		}

		if (expected_home) {
			// テスト (home)
			ntest++;
			opt_pseudo_home = true;
			auto result = aclt(status, false);
			if (result != expected_home.value()) {
				fprintf(stderr, "%s (for home) expects '%s' but '%s'\n",
					input_str.c_str(), expected_home.value(), result);
				nfail++;
			}
		}

		if (expected_filt) {
			// テスト (home/quoted)
			ntest++;
			opt_pseudo_home = true;
			auto result = acl(status, true);
			if (result != expected_filt.value()) {
				fprintf(stderr, "%s (for home/quoted) expectes '%s' but '%s'\n",
					input_str.c_str(), expected_filt.value(), result);
				nfail++;
			}

			// テスト (filter)
			ntest++;
			opt_pseudo_home = false;
			result = acl(status, false);
			if (result != expected_filt.value()) {
				fprintf(stderr, "%s (for filter) expectes '%s' but '%s'\n",
					input_str.c_str(), expected_filt.value(), result);
				nfail++;
			}

			// テスト (filter)
			ntest++;
			opt_pseudo_home = false;
			result = acl(status, true);
			if (result != expected_filt.value()) {
				fprintf(stderr,
					"%s (for filter/quoted) expectes '%s' but '%s'\n",
					input_str.c_str(), expected_filt.value(), result);
				nfail++;
			}
		}
	}
	printf("%d tests, %d passed", ntest, ntest - nfail);
	if (nfail > 0) {
		printf(", %s FAILED!", nfail);
	}
	printf("\n");
}
#endif // SELFTEST

// 1ツイートを表示。
// true なら戻ったところで1行空ける改行。ツイートとツイートの間は1行
// 空けるがここで判定の結果何も表示しなかったら空けないなど。
static bool
showstatus(const Json& status, bool is_quoted)
{
	Json obj = status["object"];

	// このツイートを表示するかどうかの判定。
	// これは、このツイートがリツイートを持っているかどうかも含めた判定を
	// 行うのでリツイート分離前に行う。
	if (acl(status, is_quoted) == false) {
		return false;
	}

	// 表示範囲だけ録画ならここで保存。
	// 実際にはここから NG ワードと鍵垢の非表示判定があるけど
	// もういいだろう。
	if (opt_record_mode == 1 && is_quoted == false) {
		record(status);
	}

	// NGワード
	NGStatus ngstat;
	bool match = ngword.Match(&ngstat, status);
	if (match) {
		// マッチしたらここで表示
		diagShow.Print(1, "showstatus: ng -> false");
		if (opt_show_ng) {
			auto userid = coloring(formatid(ngstat.screen_name), Color::NG);
			auto name = coloring(formatname(ngstat.name), Color::NG);
			auto time = coloring(ngstat.time, Color::NG);
			auto msg = coloring("NG:" + ngstat.ngword, Color::NG);

			print_(name + " " + userid + "\n"
			     + time + " " + msg + "\n");
			return true;
		}
		return false;
	}

	// RT なら、RT 元を status に、RT先を s に。
	Json s = status;
	bool has_retweet = false;
	if (status.contains("retweeted_status")) {
		s = status["retweeted_status"];
		has_retweet = true;
	}

	// 簡略表示の判定。QT 側では行わない
	if (is_quoted == false) {
		if (has_retweet) {
			auto rt_id = s["id_str"].get<std::string>();

			// 直前のツイートが (フォロー氏による) 元ツイートで
			// 続けてこれがそれを RT したツイートなら簡略表示だが、
			// この二者は別なので1行空けたまま表示。
			if (rt_id == last_id) {
				if (last_id_count++ < last_id_max) {
					auto rtmsg = format_rt_owner(status);
					auto rtcnt = format_rt_cnt(s);
					auto favcnt = format_fav_cnt(s);
					print_(rtmsg + rtcnt + favcnt + "\n");
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
					auto rtmsg = format_rt_owner(status);
					auto rtcnt = format_rt_cnt(s);
					auto favcnt = format_fav_cnt(s);
					printf(CSI "1A");
					print_(rtmsg + rtcnt + favcnt + "\n");
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
			last_id = s["id_str"].get<std::string>() + "_RT";
		} else {
			last_id = status["id_str"].get<std::string>();
		}
		last_id_count = 0;
	}

	auto s_user = s["user"];
	auto userid = coloring(formatid(s_user["screen_name"].get<std::string>()),
		Color::UserId);
	auto name = coloring(formatname(s_user["name"].get<std::string>()),
		Color::Username);
	auto src = coloring(unescape(strip_tags(s["source"].get<std::string>()))
		+ "から", Color::Source);
	auto time = coloring(formattime(s), Color::Time);
	auto verified = s_user["verified"].get<bool>()
		? coloring(" ●", Color::Verified)
		: "";

	std::vector<MediaInfo> mediainfo;
	auto msg = formatmsg(s, mediainfo);

	show_icon(s_user);
	print_(name + " " + userid + verified);
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
	if (s.contains("quoted_status")) {
		// この中はインデントを一つ下げる
		printf("\n");
		indent_depth++;
		showstatus(s["quoted_status"], true);
		indent_depth--;
		// 引用表示後のここは改行しない
	}

	// このステータスの既 RT、既ふぁぼ数
	auto rtmsg = format_rt_cnt(s);
	auto favmsg = format_fav_cnt(s);
	print_(time + " " + src + rtmsg + favmsg);
	printf("\n");

	// リツイート元
	if (has_retweet) {
		print_(format_rt_owner(status));
		printf("\n");
	}

	// ふぁぼはもう飛んでこない

	return true;
}

// リツイート元通知を整形して返す
static std::string
format_rt_owner(const Json& status)
{
	auto user = status["user"];
	auto rt_time   = formattime(status);
	auto rt_userid = formatid(user["screen_name"].get<std::string>());
	auto rt_name   = formatname(user["name"].get<std::string>());
	auto str = coloring(string_format("%s %s %s がリツイート",
		rt_time.c_str(), rt_name.c_str(), rt_userid.c_str()), Color::Retweet);
	return str;
}

// リツイート数を整形して返す
static std::string
format_rt_cnt(const Json& s)
{
	auto rtcnt = s["retweet_count"].get<int>();
	return (rtcnt > 0)
		? coloring(string_format(" %dRT", rtcnt), Color::Retweet)
		: "";
}

// ふぁぼ数を整形して返す
static std::string
format_fav_cnt(const Json& s)
{
	auto favcnt = s["favorite_count"].get<int>();
	return (favcnt > 0)
		? coloring(string_format(" %dFav", favcnt), Color::Favorite)
		: "";
}

// インデントを付けて文字列を表示する
static void
print_(const std::string& text)
{
	printf("%s not implemented\n", __func__);
}

static void
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

	color2esc[Color::Username]	= username;
	color2esc[Color::UserId]	= blue;
	color2esc[Color::Time]		= gray;
	color2esc[Color::Source]	= gray;

	color2esc[Color::Retweet]	= str_join(";", BOLD, green);
	color2esc[Color::Favorite]	= str_join(";", BOLD, fav);
	color2esc[Color::Url]		= str_join(";", UNDERSCORE, blue);
	color2esc[Color::Tag]		= blue;
	color2esc[Color::Verified]	= verified;
	color2esc[Color::Protected]	= gray;
	color2esc[Color::NG]		= str_join(";", STRIKE, gray);
}

// 文字列を sep で結合した文字列を返します。
// ただし (glib の) string.join() と異なり、(null と)空文字列の要素は排除
// した後に結合を行います。
// XXX 今の所、引数は2つのケースしかないので手抜き。
// 例)
//   string.join(";", "AA", "") -> "AA;"
//   str_join(";", "AA", "")    -> "AA"
static std::string
str_join(const std::string& sep, const std::string& s1, const std::string& s2)
{
	if (s1 == "" || s2 == "") {
		return s1 + s2;
	} else {
		return s1 + sep + s2;
	}
}

static std::string
coloring(const std::string& text, Color col)
{
	std::string rv;

	if (opt_nocolor) {
		// --nocolor なら一切属性を付けない
		rv = text;
	} else if (color2esc.empty()) {
		// ポカ避け
		rv = string_format("Coloring(%s,%d)", text.c_str(), col);
	} else {
		rv = CSI + color2esc[col] + "m" + text + CSI + "0m";
	}
	return rv;
}

class TextTag
{
 public:
	int Start;
	int End;
	Color Type;
	std::string Text;

	TextTag(int start_, int end_, Color type_, const std::string& text_)
	{
		Start = start_;
		End = end_;
		Type = type_;
		Text = text_;
	}

	int length() { return End - Start; }

	std::string to_string()
	{
		return string_format("(%d, %d, %d)", Start, End, (int)Type);
	}
};

// 本文を整形して返す
std::string
formatmsg(const Json& s, const std::vector<MediaInfo>& mediainfo)
{
printf("%s not implemented\n", __func__);
}

// 現在行に user のアイコンを表示。
// 呼び出し時点でカーソルは行頭にあるため、必要なインデントを行う。
// アイコン表示後にカーソル位置を表示前の位置に戻す。
static void
show_icon(const Json& user)
{
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

	if (opt_noimage) {
		printf(" *");
	} else {
		auto screen_name = unescape(user["screen_name"].get<std::string>());
		auto image_url = user["profile_image_url_https"].get<std::string>();

		// URL のファイル名部分をキャッシュのキーにする
		auto p = image_url.rfind('/');
		auto img_file = string_format("icon-%dx%d-%s-%s",
			iconsize, iconsize, screen_name.c_str(), image_url.c_str() + p + 1);

		show_image(img_file, image_url, iconsize, -1);
	}

	// カーソル位置を復帰
	printf("\r");
	// カーソル位置保存/復元に対応していない端末でも動作するように
	// カーソル位置復元前にカーソル上移動x3を行う
	printf(CSI "3A" ESC "8");
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
	if (opt_noimage)
		return false;

	std::string img_path = cachedir + PATH_SEPARATOR + img_file;

	diagImage.Debug("show_image: img_url=%s", img_url.c_str());
	diagImage.Debug("show_image: img_path=%s", img_path.c_str());
	auto cache_filename = img_path + ".sixel";
	FILE *cache_file = fopen(cache_filename.c_str(), "r");
	if (cache_file == NULL) {
		// キャッシュファイルがないので、画像を取得
		diagImage.Debug("sixel cache is not found");
		cache_file = fetch_image(cache_filename, img_url, resize_width);
		if (cache_file == NULL) {
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
	errno = 0;
	sx_width = strtol(buf + i, &ep, 10);
	if (ep == buf + i || (*ep != ';' && *ep != ' ') || errno == ERANGE) {
		sx_width = 0;
	}
	// Pv
	i = ep - buf;
	i++;
	errno = 0;
	sx_height = strtol(buf + i, &ep, 10);
	if (ep == buf + i || errno == ERANGE) {
		sx_height = 0;
	}

	if (sx_width == 0 || sx_height == 0)
		return false;

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

// 画像をダウンロードして SIXEL に変換してキャッシュする。
// 成功すれば、書き出したキャッシュファイルの FILE* (位置は先頭) を返す。
// 失敗すれば NULL を返す。
// cache_filename はキャッシュするファイルのファイル名
// img_url は画像 URL
// resize_width はリサイズすべき幅を指定、0 ならリサイズしない
static
FILE *fetch_image(const std::string& cache_filename, const std::string& img_url,
	int resize_width)
{
	printf("%s not implemented\n", __func__);
	return NULL;
}

// 自分の ID を取得
static void
get_credentials()
{
	CreateTwitter();

	StringDictionary options;
	options["include_entities"] = "false";
	options["include_email"] = "false";
	auto json = tw.API2Json("GET", Twitter::APIRoot,
		"account/verify_credentials", options);
	if (json.is_null()) {
		errx(1, "get_credentials API2Json failed");
	}
	diag.Debug("json=|%s|", json.dump());
	if (json.contains("errors")) {
		// エラーのフォーマットがこれかどうかは分からんけど
		auto errorlist = json["errors"];
		// エラーが複数返ってきたらどうするかね
		auto error = errorlist[0];
		auto code = error["code"].get<int>();
		auto message = error["message"].get<std::string>();
		errx(1, "get_credentials failed: %s(%d)", message.c_str(), code);
	}

	myid = json["id_str"].get<std::string>();
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
		auto json = tw.API2Json("GET", Twitter::APIRoot, api, options);
		if (json.is_null()) {
			errx(1, "%s API2Json failed", funcname);
		}
		diag.Debug("json=|%s|", json.dump());
		if (json.contains("errors")) {
			auto errorlist = json["errors"];
			// エラーが複数返ってきたらどうするかね
			auto error = errorlist[0];
			auto code = error["code"].get<int>();
			auto message = error["message"].get<std::string>();
			errx(1, "%s failed: %s(%d)", funcname, message.c_str(), code);
		}

		auto users = json["ids"];
		for (auto u : users) {
			auto id_str = u.get<std::string>();
			list[id_str] = id_str;
		}

		cursor = json["next_cursor_str"].get<std::string>();
		diag.Debug("cursor=|%s|", cursor.c_str());
	} while (cursor != "0");

	return list;
}

// ユーザ一覧を表示するコマンド(共通部分)
static void
cmd_users_list(const StringDictionary& list)
{
	for (const auto& kv : list) {
		const auto& key = kv.first;
		printf("%s\n", key.c_str());
	}
}

// フォローユーザ一覧の読み込み
static void
get_follow_list()
{
	followlist = get_paged_list("friends/ids", __func__);
}

// フォローユーザの一覧を取得して表示するコマンド
static void
cmd_followlist()
{
	CreateTwitter();
	get_follow_list();
	cmd_users_list(followlist);
}

// ブロックユーザ一覧の読み込み
static void
get_block_list()
{
	blocklist = get_paged_list("blocks/ids", __func__);
}

// ブロックユーザの一覧を取得して表示するコマンド
static void
cmd_blocklist()
{
	CreateTwitter();
	get_block_list();
	cmd_users_list(blocklist);
}

// ミュートユーザ一覧の読み込み
static void
get_mute_list()
{
	mutelist = get_paged_list("mutes/users/ids", __func__);
}

// ミュートユーザの一覧を取得して表示するコマンド
static void
cmd_mutelist()
{
	CreateTwitter();
	get_mute_list();
	cmd_users_list(mutelist);
}

// RT非表示ユーザ一覧の読み込み
static void
get_nort_list()
{
	// ミュートユーザ一覧等とは違って、リスト一発で送られてくるっぽい。
	// なんであっちこっちで仕様が違うんだよ…。

	nortlist.clear();

	// JSON を取得
	auto json = tw.API2Json("GET", Twitter::APIRoot,
		"friendships/no_retweets/ids", {});
	if (json.is_null()) {
		errx(1, "get_nort_list API2Json failed");
	}
	diag.Debug("json=|%s|", json.dump());

	if (!json.is_array()) {
		// どうするかね
		return;
	}

	for (const auto& u : json) {
		auto id_str = u.get<std::string>();
		nortlist[id_str] = id_str;
	}
}

// RT 非表示ユーザの一覧を取得して表示するコマンド
static void
cmd_nortlist()
{
	CreateTwitter();
	get_nort_list();
	cmd_users_list(nortlist);
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

	// アイコンは7日分くらいか
	snprintf(cmd, sizeof(cmd),
		"find %s -name icon-\\* -type f -atime +7 -exec rm {} +",
		cachedir.c_str());
	system(cmd);

	// 写真は24時間分くらいか
	snprintf(cmd, sizeof(cmd),
		"find %s -name http\\* -type f -atime +1 -exec rm {} +",
		cachedir.c_str());
	system(cmd);
}

// NG ワードを追加するコマンド
static void
cmd_ngword_add()
{
	ngword.CmdAdd(opt_ngword, opt_ngword_user);
}

// NG ワードを削除するコマンド
static void
cmd_ngword_del()
{
	ngword.CmdDel(opt_ngword);
}

// NG ワード一覧を表示するコマンド
static void
cmd_ngword_list()
{
	ngword.CmdList();
}

static void
signal_handler(int signo)
{
	switch (signo) {
	 case SIGINT:
		// SIXEL 出力中なら中断する (CAN + ST)
		if (in_sixel) {
			printf(CAN ESC "\\");
			fflush(stdout);
		} else {
			exit(0);
		}
		break;

	 case SIGWINCH:
		sigwinch();
		break;

	 default:
		warnx("caught signal %d", signo);
		break;
	}
}

// SIGWINCH の処理
static void
sigwinch()
{
	int ws_cols = 0;
	int ws_width = 0;
	int ws_height = 0;

	struct winsize ws;
	int r = ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws);
	if (r != 0) {
		warn("TIOCGWINSZ failed");
	} else {
		ws_cols = ws.ws_col;

		if (ws.ws_col != 0) {
			ws_width = ws.ws_xpixel / ws.ws_col;
		}
		if (ws.ws_row != 0) {
			ws_height = ws.ws_ypixel / ws.ws_row;
		}
	}

	const char *msg_cols;
	const char *msg_width;
	const char *msg_height;

	// 画面幅は常に更新
	if (ws_cols > 0) {
		screen_cols = ws_cols;
		msg_cols = " (from ioctl)";
	} else {
		screen_cols = 0;
		msg_cols = " (not detected)";
	}

	// フォント幅と高さは指定されてない時だけ取得した値を使う
	auto use_default_font = false;
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
		printf("sayaka: Fontsize not detected. "
			"Application default %dx%d is used.", fontwidth, fontheight);
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

	diag.Debug("screen columns=%d%s", screen_cols, msg_cols);
	diag.Debug("font height=%d%s", fontheight, msg_height);
	diag.Debug("font width=%d%s", fontwidth, msg_width);
	diag.Debug("iconsize=%d", iconsize);
	diag.Debug("indent columns=%d", indent_cols);
	diag.Debug("imagesize=%d", imagesize);
}

static void
cmd_version()
{
	printf("sayaka version %s\n", version);
}

static void
usage()
{
	printf(
R"(usage: sayaka [<options>...] --home
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
	--record <file> : record JSON to file.
	--record-all <file> : record all received JSON to file.
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
)"
	);
	exit(0);
}
