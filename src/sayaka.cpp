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

#include "sayaka.h"
#include "FileStream.h"
#include "JsonInc.h"
#include "MathAlphaSymbols.h"
#include "ProtoMisskey.h"
#include "ProtoTwitter.h"
#include "RichString.h"
#include "StringUtil.h"
#include "UString.h"
#include "autofd.h"
#include "eaw_code.h"
#include "fetch_image.h"
#include "subr.h"
#include "term.h"
#include <memory>
#include <cstdio>
#include <cstring>
#include <err.h>
#include <getopt.h>
#include <signal.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#if defined(HAVE_SYS_TTYCOM_H)
#include <sys/ttycom.h>
#endif

#if !defined(PATH_SEPARATOR)
#define PATH_SEPARATOR "/"
#endif

enum SayakaCmd {
	Noop = 0,
	Stream,
	Play,
	NgwordAdd,
	NgwordDel,
	NgwordList,
	Version,
};

static void progress(const char *msg);
static bool showobject(const std::string& line);
static std::string str_join(const std::string& sep,
	const std::string& s1, const std::string& s2);
static std::string GetHomeDir();
static void init();
static void init_screen();
static void invalidate_cache();
static void signal_handler(int signo);
static void sigwinch();
static void cmd_ngword_add();
static void cmd_ngword_del();
static void cmd_ngword_list();
static void cmd_version();
[[noreturn]] static void usage();

static const char version[] = "3.6.5 (2023/07/09)";

static const int DEFAULT_FONT_WIDTH = 7;
static const int DEFAULT_FONT_HEIGHT = 14;

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
bool opt_protect;
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
StringDictionary followlist;	// フォロー氏リスト
StringDictionary blocklist;		// ブロック氏リスト
StringDictionary mutelist;		// ミュート氏リスト
StringDictionary nortlist;		// RT非表示氏リスト
bool opt_show_ng;				// NG ツイートを隠さない
std::string opt_ngword;			// NG ワード (追加削除コマンド用)
std::string opt_ngword_user;	// NG 対象ユーザ (追加コマンド用)
std::string record_file;		// 記録用ファイルパス
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
std::string myid;				// 自身の user id
bool opt_nocolor;				// テキストに(色)属性を一切付けない
int  opt_record_mode;			// 0:保存しない 1:表示のみ 2:全部保存
bool opt_mathalpha;				// Mathematical AlphaNumeric を全角英数字に変換
bool opt_nocombine;				// Combining Enclosing Keycap を表示しない
Proto proto;					// プロトコル
StreamMode opt_stream;			// ストリーム種別
std::string basedir;
std::string cachedir;
std::string colormapdir;

static std::array<UString, Color::Max> color2esc;	// 色エスケープ文字列

// enum は getopt() の1文字のオプションと衝突しなければいいので
// 適当に 0x80 から始めておく。
enum {
	OPT_black = 0x80,
	OPT_ciphers,
	OPT_color,
	OPT_debug,
	OPT_debug_format,
	OPT_debug_http,
	OPT_debug_image,
	OPT_debug_show,
	OPT_debug_sixel,
	OPT_eaw_a,
	OPT_eaw_n,
	OPT_euc_jp,
	OPT_font,
	OPT_force_sixel,
	OPT_full_url,
	OPT_home,
	OPT_jis,
	OPT_mathalpha,
	OPT_max_cont,
	OPT_max_image_cols,
	OPT_misskey,
	OPT_ngword_add,
	OPT_ngword_del,
	OPT_ngword_list,
	OPT_ngword_user,
	OPT_no_color,
	OPT_no_combine,
	OPT_no_image,
	OPT_ormode,
	OPT_palette,
	OPT_play,
	OPT_progress,
	OPT_protect,
	OPT_record,
	OPT_record_all,
	OPT_show_ng,
	OPT_timeout_image,
	OPT_twitter,
	OPT_version,
	OPT_white,
	OPT_x68k,
};

static const struct option longopts[] = {
	{ "black",			no_argument,		NULL,	OPT_black },
	{ "ciphers",		required_argument,	NULL,	OPT_ciphers },
	{ "color",			required_argument,	NULL,	OPT_color },
	{ "debug",			required_argument,	NULL,	OPT_debug },
	{ "debug-format",	no_argument,		NULL,	OPT_debug_format },
	{ "debug-http",		required_argument,	NULL,	OPT_debug_http },
	{ "debug-image",	required_argument,	NULL,	OPT_debug_image },
	{ "debug-show",		required_argument,	NULL,	OPT_debug_show },
	{ "debug-sixel",	required_argument,	NULL,	OPT_debug_sixel },
	{ "eaw-a",			required_argument,	NULL,	OPT_eaw_a },
	{ "eaw-n",			required_argument,	NULL,	OPT_eaw_n },
	{ "euc-jp",			no_argument,		NULL,	OPT_euc_jp },
	{ "font",			required_argument,	NULL,	OPT_font },
	{ "force-sixel",	no_argument,		NULL,	OPT_force_sixel },
	{ "full-url",		no_argument,		NULL,	OPT_full_url },
	{ "home",			no_argument,		NULL,	OPT_home },
	{ "jis",			no_argument,		NULL,	OPT_jis },
	{ "mathalpha",		no_argument,		NULL,	OPT_mathalpha },
	{ "max-cont",		required_argument,	NULL,	OPT_max_cont },
	{ "max-image-cols",	required_argument,	NULL,	OPT_max_image_cols },
	{ "misskey",		no_argument,		NULL,	OPT_misskey, },
	{ "ngword-add",		required_argument,	NULL,	OPT_ngword_add },
	{ "ngword-del",		required_argument,	NULL,	OPT_ngword_del },
	{ "ngword-list",	no_argument,		NULL,	OPT_ngword_list },
	{ "ngword-user",	required_argument,	NULL,	OPT_ngword_user },
	{ "no-color",		no_argument,		NULL,	OPT_no_color },
	{ "no-combine",		no_argument,		NULL,	OPT_no_combine },
	{ "no-image",		no_argument,		NULL,	OPT_no_image },
	{ "ormode",			required_argument,	NULL,	OPT_ormode },
	{ "palette",		required_argument,	NULL,	OPT_palette },
	{ "play",			no_argument,		NULL,	OPT_play },
	{ "progress",		no_argument,		NULL,	OPT_progress },
	{ "protect",		no_argument,		NULL,	OPT_protect },
	{ "record",			required_argument,	NULL,	OPT_record },
	{ "record-all",		required_argument,	NULL,	OPT_record_all },
	{ "show-ng",		no_argument,		NULL,	OPT_show_ng },
	{ "timeout-image",	required_argument,	NULL,	OPT_timeout_image },
	{ "twitter",		no_argument,		NULL,	OPT_twitter },
	{ "version",		no_argument,		NULL,	OPT_version },
	{ "white",			no_argument,		NULL,	OPT_white },
	{ "x68k",			no_argument,		NULL,	OPT_x68k },
	{ "help",			no_argument,		NULL,	'h' },
	{ NULL },
};

// 起動経過を表示 (遅マシン用)
static void
progress(const char *msg)
{
	if (__predict_false(diag >= 1) || __predict_false(opt_progress)) {
		fputs(msg, stdout);
		fflush(stdout);
	}
}

int
main(int ac, char *av[])
{
	int c;
	int val;

	diagHttp.SetClassname("HttpClient");

	SayakaCmd cmd = SayakaCmd::Noop;
	basedir     = GetHomeDir() + "/.sayaka/";
	cachedir    = basedir + "cache";
	colormapdir = basedir;
	// トークンのデフォルトファイル名は API version によって変わる
	// ので、デフォルト empty のままにしておく。

	ngword_list.SetFileName(basedir + "ngword.json");

	address_family = AF_UNSPEC;
	bgcolor = BG_NONE;
	color_mode = 256;
	opt_show_ng = false;
	last_id = "";
	last_id_count = 0;
	last_id_max = 10;
	opt_full_url = false;
	opt_progress = false;
	opt_ormode = false;
	opt_output_palette = true;
	opt_timeout_image = 3000;
	opt_eaw_a = 2;
	opt_eaw_n = 1;
	use_sixel = UseSixel::AutoDetect;
	opt_stream = StreamMode::Home;
	proto = Proto::Misskey;

	while ((c = getopt_long(ac, av, "46h", longopts, NULL)) != -1) {
		switch (c) {
		 case '4':
			address_family = AF_INET;
			break;
		 case '6':
			address_family = AF_INET6;
			break;
		 case OPT_black:
			bgcolor = BG_BLACK;
			break;
		 case OPT_ciphers:
			opt_ciphers = optarg;
			break;
		 case OPT_color:
			if (strcmp(optarg, "x68k") == 0) {
				color_mode = ColorFixedX68k;
			} else {
				color_mode = stou32def(optarg, -1);
				if (color_mode < 0) {
					errno = EINVAL;
					err(1, "--color %s", optarg);
				}
			}
			break;
		 case OPT_debug:
			val = stou32def(optarg, -1);
			if (val < 0 || val > 2) {
				errx(1, "--debug %s: debug level must be 0..2", optarg);
			}
			diag.SetLevel(val);
			break;
		 case OPT_debug_format:
#if !defined(DEBUG_FORMAT)
			warnx("DEBUG_FORMAT is not compiled. ignored.");
#endif
			opt_debug_format = true;
			break;
		 case OPT_debug_http:
			val = stou32def(optarg, -1);
			if (val < 0 || val > 2) {
				errx(1, "--debug-http %s: debug level must be 0..2", optarg);
			}
			diagHttp.SetLevel(val);
			break;
		 case OPT_debug_image:
			val = stou32def(optarg, -1);
			if (val < 0 || val > 1) {
				errx(1, "--debug-image %s: debug level must be 0..1", optarg);
			}
			diagImage.SetLevel(val);
			break;
		 case OPT_debug_show:
			val = stou32def(optarg, -1);
			if (val < 0 || val > 2) {
				errx(1, "--debug-show %s: debug level must be 0..2", optarg);
			}
			diagShow.SetLevel(val);
			break;
		 case OPT_debug_sixel:
			val = stou32def(optarg, -1);
			if (val < 0 || val > 2) {
				errx(1, "--debug-sixel %s: debug level must be 0..2", optarg);
			}
			opt_debug_sixel = val;
			max_image_count = 1;
			break;
		 case OPT_eaw_a:
			opt_eaw_a = stou32def(optarg, -1);
			if (opt_eaw_a < 1 || opt_eaw_a > 2) {
				errx(1, "--eaw-a %s: must be either 1 or 2", optarg);
			}
			break;
		 case OPT_eaw_n:
			opt_eaw_n = stou32def(optarg, -1);
			if (opt_eaw_n < 1 || opt_eaw_n > 2) {
				errx(1, "--eaw-n %s: must be either 1 or 2", optarg);
			}
			break;
		 case OPT_euc_jp:
			output_codeset = "euc-jp";
			break;
		 case OPT_font:
		 {
			// "7x14" のような形式
			std::string str(optarg);
			auto metric = Split(str, "x");
			if (metric.size() != 2) {
				errx(1, "--font %s: argument must be <W>x<H>", optarg);
			}
			opt_fontwidth = stou32def(metric[0], 0);
			opt_fontheight = stou32def(metric[1], 0);
			if (opt_fontwidth < 1 || opt_fontheight < 1) {
				errno = EINVAL;
				err(1, "--font %s", optarg);
			}
			break;
		 }
		 case OPT_force_sixel:
			use_sixel = UseSixel::Yes;
			break;
		 case OPT_full_url:
			opt_full_url = true;
			break;
		 case OPT_home:
			cmd = SayakaCmd::Stream;
			opt_stream = StreamMode::Home;
			break;
		 case OPT_jis:
			output_codeset = "iso-2022-jp";
			break;
		 case OPT_mathalpha:
			opt_mathalpha = true;
			break;
		 case OPT_max_cont:
			last_id_max = stou32def(optarg, -1);
			if (last_id_max < 0) {
				errno = EINVAL;
				err(1, "--max-cont %s", optarg);
			}
			break;
		 case OPT_max_image_cols:
			max_image_count = stou32def(optarg, -1);
			if (max_image_count < 0) {
				errno = EINVAL;
				err(1, "--max-image-cols %s", optarg);
			}
			break;
		 case OPT_misskey:
			proto = Proto::Misskey;
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
		 case OPT_no_combine:
			opt_nocombine = true;
			break;
		 case OPT_no_image:
			use_sixel = UseSixel::No;
			break;
		 case OPT_ormode:
			if (strcmp(optarg, "on") == 0) {
				opt_ormode = true;
			} else if (strcmp(optarg, "off") == 0) {
				opt_ormode = false;
			} else {
				errx(1, "--ormode %s: must be either 'on' or 'off'", optarg);
			}
			break;
		 case OPT_palette:
			if (strcmp(optarg, "on") == 0) {
				opt_output_palette = true;
			} else if (strcmp(optarg, "off") == 0) {
				opt_output_palette = false;
			} else {
				errx(1, "--palette %s: must be either 'on' or 'off'", optarg);
			}
			break;
		 case OPT_play:
			cmd = SayakaCmd::Play;
			break;
		 case OPT_progress:
			opt_progress = true;
			break;
		 case OPT_protect:
			opt_protect = true;
			break;
		 case OPT_record:
			opt_record_mode = 1;
			record_file = optarg;
			break;
		 case OPT_record_all:
			opt_record_mode = 2;
			record_file = optarg;
			break;
		 case OPT_show_ng:
			opt_show_ng = true;
			break;
		 case OPT_timeout_image:
			opt_timeout_image = stou32def(optarg, -1);
			if (opt_timeout_image < 0) {
				errno = EINVAL;
				err(1, "--timeout-image %s", optarg);
			}
			break;
		 case OPT_twitter:
			proto = Proto::Twitter;
			break;
		 case OPT_version:
			cmd = SayakaCmd::Version;
			break;
		 case OPT_white:
			bgcolor = BG_WHITE;
			break;
		 case OPT_x68k:
			// 以下を指定したのと同じ
			color_mode = ColorFixedX68k;
			opt_fontwidth = 8;
			opt_fontheight = 16;
			output_codeset = "iso-2022-jp";
			bgcolor = BG_BLACK;
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

#if 0
	// どのコマンドでもなくキーワードだけならフィルタモード
	if (optind < ac && cmd == SayakaCmd::Noop) {
		cmd = SayakaCmd::Stream;
		for (int i = optind; i < ac; i++) {
			opt_filter.emplace_back(av[i]);
		}
	}
#endif

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

	init();

	// コマンド別処理
	switch (cmd) {
	 case SayakaCmd::Stream:
		if (proto == Proto::Misskey) {
			init_screen();

			// 古いキャッシュを削除
			progress("Deleting expired cache files...");
			invalidate_cache();
			progress("done\n");

			cmd_misskey_stream();
		} else {
			errx(1, "stream mode can only be used with --misskey");
		}
		break;
	 case SayakaCmd::Play:
		if (proto == Proto::None) {
			errx(1, "--play must be used with --twitter or --misskey");
		}
		init_screen();
		cmd_play();
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
	signal(SIGPIPE,   SIG_IGN);
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

// 表示周りの初期化
void
init_screen()
{
	bool r;

	// 端末の背景色を調べる (オプションで指定されてなければ)。
	// 判定できなければ背景色白をデフォルトにしておく。
	if (bgcolor == BG_NONE) {
		progress("Checking bgcolor of the terminal...");
		bgcolor = terminal_bgcolor();
		progress("done\n");
		if (bgcolor == BG_NONE) {
			printf("Terminal doesn't support control sequence, "
			       "switch to --white\n");
			bgcolor = BG_WHITE;
		}
	}

	// 端末が SIXEL をサポートしているか。
	//
	//             termianl_support_sixel() ?
	// use_sixel \  true	false
	// -----------+--------	------
	// AutoDetect | ->Yes	-> No
	// No         | No		No
	// Yes        | Yes		Yes
	if (use_sixel == UseSixel::AutoDetect) {
		progress("Checking whether the terminal supports sixel...");
		r = terminal_support_sixel();
		if (r) {
			progress("yes\n");
			use_sixel = UseSixel::Yes;
		} else {
			progress("no\n");
			use_sixel = UseSixel::No;
		}
	}

	// 文字コードの初期化
	UString::Init(output_codeset);

	// 色の初期化
	init_color();

	// 一度手動で呼び出して桁数を取得
	sigwinch();

	// NG ワード取得
	ngword_list.ReadFile();
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
	if (system(cmd) < 0) {
		warn("system(find icon)");
	}

	// 写真は2日分くらいか
	snprintf(cmd, sizeof(cmd),
		"find %s -name http\\* -type f -atime +2 -exec rm {} +",
		cachedir.c_str());
	if (system(cmd) < 0) {
		warn("system(find photo)");
	}
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

	const char *msg_cols = "";
	const char *msg_width = "";
	const char *msg_height = "";

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
			"Application default %dx%d is used.\n", fontwidth, fontheight);
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

	Debug(diag, "screen columns=%d%s", screen_cols, msg_cols);
	Debug(diag, "font height=%d%s", fontheight, msg_height);
	Debug(diag, "font width=%d%s", fontwidth, msg_width);
	Debug(diag, "iconsize=%d", iconsize);
	Debug(diag, "indent columns=%d", indent_cols);
	Debug(diag, "imagesize=%d", imagesize);
}

// NG ワードを追加するコマンド
static void
cmd_ngword_add()
{
	ngword_list.CmdAdd(opt_ngword, opt_ngword_user);
}

// NG ワードを削除するコマンド
static void
cmd_ngword_del()
{
	ngword_list.CmdDel(opt_ngword);
}

// NG ワード一覧を表示するコマンド
static void
cmd_ngword_list()
{
	ngword_list.CmdList();
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
R"(usage: sayaka [<options>...]
	--color <n> : color mode { 2 .. 256 or x68k }. default 256.
	--font <width>x<height> : font size. default 7x14
	--full-url : display full URL even if the URL is abbreviated.
	--white / --black : darken/lighten the text color. (default: autodetect)
	--no-color : disable all text color sequences
	--no-image : force disable (SIXEL) images.
	--force-sixel : force enable SIXEL images.
	--jis / --eucjp : Set output encoding.
	--play : read JSON from stdin.
	--progress: show startup progress (for very slow machines).
	--protect : don't display protected user's tweet.
	--record <file> : record JSON to file.
	--record-all <file> : record all received JSON to file.
	--timeout-image <msec>
	--version
	--x68k : preset options for x68k (with SIXEL kernel).

	-4 / -6                         --ciphers <ciphers>
	--debug       <0-2>             --debug-format
	--debug-http  <0-2>             --debug-image <0-1>
	--debug-sixel <0-2>             --debug-show  <0-2>
	--mathalpha                     --no-combine
	--max-cont <n>                  --max-image-cols <n>
	--ngword-add                    --ngword-del
	--ngword-list                   --ngword-user
	                                --ormode <on|off> (default off)
	--show-ng                       --palette <on|off> (default on)
)"
	);
	exit(0);
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
		switch (proto) {
		 case Proto::Twitter:
			if (showobject(line) == false) {
				return;
			}
			break;
		 case Proto::Misskey:
			if (misskey_show_object(line) == false) {
				return;
			}
			break;
		 default:
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

// UString をインデントを付けて文字列を表示する
void
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

		// --no-combine なら Combining Enclosing * (U+20DD-U+20E4) の前に
		// スペースを入れて、囲まれるはずだった文字とは独立させる。
		// 前の文字(たいていただの ASCII 数字)が潰されて読めなくなるのを
		// 防ぐため。
		// U+20E1 は「上に左右矢印を前の文字につける」で囲みではないが
		// 面倒なので混ぜておく。なぜ間に入れたのか…。
		if (__predict_false(0x20dd <= uni && uni <= 0x20e4) && opt_nocombine) {
			utext.Append(0x20);
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

	if (0) {
		for (int i = 0; i < utext.size(); i++) {
			printf("print_[%d] %02x\n", i, utext[i]);
		}
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
	color2esc[Color::Protected]	= UString(gray);
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
UString
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
UString
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
UString
coloring(const std::string& text, Color col)
{
	UString utext;

	utext += ColorBegin(col);
	utext += UString::FromUTF8(text);
	utext += ColorEnd(col);

	return utext;
}

// 画像をキャッシュして表示
//  img_file はキャッシュディレクトリ内でのファイル名
//  img_url は画像の URL
//  resize_width はリサイズ後の画像の幅。ピクセルで指定。0 を指定すると
//  リサイズせずオリジナルのサイズ。
//  index は -1 ならアイコン、0 以上なら添付写真の何枚目かを表す。
//  どちらも位置決めなどのために使用する。
// 表示できれば真を返す。
bool
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


// ツイートを保存する
void
record(const char *str)
{
	FILE *fp;

	fp = fopen(record_file.c_str(), "a+");
	if (fp == NULL) {
		return;
	}
	fputs(str, fp);
	fputs("\n", fp);
	fclose(fp);
}

void
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
