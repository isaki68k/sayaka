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
#include "Display.h"
#include "FileStream.h"
#include "JsonInc.h"
#include "Misskey.h"
#include "Print.h"
#include "StringUtil.h"
#include "TLSHandle.h"
#if defined(USE_MBEDTLS)
#include "TLSHandle_mbedtls.h"
#endif
#if defined(USE_TWITTER)
#include "Twitter.h"
#endif
#include "UString.h"
#include "eaw_code.h"
#include "subr.h"
#include "term.h"
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

enum SayakaCmd {
	Noop = 0,
	Stream,
	Play,
#if 0
	NgwordAdd,
	NgwordDel,
	NgwordList,
#endif
	Version,
};

static void progress(const char *msg);
static std::string GetHomeDir();
static void init();
static void init_screen();
static void invalidate_cache();
static void signal_handler(int signo);
static void sigwinch();
#if 0
static void cmd_ngword_add();
static void cmd_ngword_del();
static void cmd_ngword_list();
#endif
static void cmd_version();
[[noreturn]] static void usage();

static const char version[] = "3.7.5 (2024/03/12)";

static const int DEFAULT_FONT_WIDTH = 7;
static const int DEFAULT_FONT_HEIGHT = 14;

int  address_family;			// AF_INET*
int  color_mode;				// 色数もしくはカラーモード
bool opt_protect;
Diag diag;						// デバッグ (無分類)
Diag diagHttp;					// デバッグ (HTTP コネクション)
Diag diagImage;					// デバッグ (画像周り)
Diag diagShow;					// デバッグ (メッセージ表示判定)
bool opt_debug_format;			// デバッグフラグ (formatmsg 周り)
int  opt_debug_sixel;			// デバッグレベル (SIXEL変換周り)
int  screen_cols;				// 画面の桁数
static int opt_fontwidth;		// オプション指定のフォント幅
static int opt_fontheight;		// オプション指定のフォント高さ
int  fontwidth;					// フォントの幅(ドット数)
int  fontheight;				// フォントの高さ(ドット数)
int  iconsize;					// アイコンの大きさ(正方形、ドット数)
int  imagesize;					// 画像の大きさ(どこ?)
int  indent_cols;				// インデント1階層分の桁数
int  indent_depth;				// インデント深さ
#if 0
bool opt_show_ng;				// NG ツイートを隠さない
std::string opt_ngword;			// NG ワード (追加削除コマンド用)
std::string opt_ngword_user;	// NG 対象ユーザ (追加コマンド用)
NGWordList ngword_list;			// NG ワードリスト
#endif
static std::string record_file;	// 記録用ファイルパス
std::string last_id;			// 直前に表示したツイート
int  last_id_count;				// 連続回数
int  last_id_max;				// 連続回数の上限
std::string opt_ciphers;		// 暗号スイート
bool opt_progress;				// 起動時の途中経過表示
bool opt_ormode;				// SIXEL ORmode で出力するなら true
bool opt_output_palette;		// SIXEL にパレット情報を出力するなら true
int  opt_record_mode;			// 0:保存しない 1:表示のみ 2:全部保存
bool opt_nocombine;				// Combining Enclosing Keycap を表示しない
Proto opt_proto;				// プロトコル
StreamMode opt_stream;			// ストリーム種別
std::string opt_server;			// 接続先サーバ名
std::string basedir;
std::string cachedir;

#if defined(USE_TWITTER)
std::string myid;				// 自身の user id
StringDictionary followlist;	// フォロー氏リスト
StringDictionary blocklist;		// ブロック氏リスト
StringDictionary mutelist;		// ミュート氏リスト
StringDictionary nortlist;		// RT非表示氏リスト
#endif

// enum は getopt() の1文字のオプションと衝突しなければいいので
// 適当に 0x80 から始めておく。
enum {
	OPT_ciphers = 0x80,
	OPT_color,
	OPT_dark,
	OPT_debug,
	OPT_debug_format,
	OPT_debug_http,
	OPT_debug_image,
	OPT_debug_mbedtls,
	OPT_debug_show,
	OPT_debug_sixel,
	OPT_debug_tls,
	OPT_eaw_a,
	OPT_eaw_n,
	OPT_euc_jp,
	OPT_font,
	OPT_force_sixel,
	OPT_full_url,
	OPT_home,
	OPT_jis,
	OPT_light,
	OPT_local,
	OPT_mathalpha,
	OPT_max_cont,
	OPT_max_image_cols,
	OPT_misskey,
#if 0
	OPT_ngword_add,
	OPT_ngword_del,
	OPT_ngword_list,
	OPT_ngword_user,
#endif
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
	OPT_show_cw,
	OPT_show_nsfw,
#if 0
	OPT_show_ng,
#endif
	OPT_timeout_image,
	OPT_twitter,
	OPT_version,
	OPT_x68k,
};

static const struct option longopts[] = {
	{ "ciphers",		required_argument,	NULL,	OPT_ciphers },
	{ "color",			required_argument,	NULL,	OPT_color },
	{ "dark",			no_argument,		NULL,	OPT_dark },
	{ "debug",			required_argument,	NULL,	OPT_debug },
	{ "debug-format",	no_argument,		NULL,	OPT_debug_format },
	{ "debug-http",		required_argument,	NULL,	OPT_debug_http },
	{ "debug-image",	required_argument,	NULL,	OPT_debug_image },
	{ "debug-mbedtls",	required_argument,	NULL,	OPT_debug_mbedtls },
	{ "debug-show",		required_argument,	NULL,	OPT_debug_show },
	{ "debug-sixel",	required_argument,	NULL,	OPT_debug_sixel },
	{ "debug-tls",		required_argument,	NULL,	OPT_debug_tls },
	{ "eaw-a",			required_argument,	NULL,	OPT_eaw_a },
	{ "eaw-n",			required_argument,	NULL,	OPT_eaw_n },
	{ "euc-jp",			no_argument,		NULL,	OPT_euc_jp },
	{ "font",			required_argument,	NULL,	OPT_font },
	{ "force-sixel",	no_argument,		NULL,	OPT_force_sixel },
	{ "full-url",		no_argument,		NULL,	OPT_full_url },
//	{ "home",			no_argument,		NULL,	OPT_home },
	{ "jis",			no_argument,		NULL,	OPT_jis },
	{ "light",			no_argument,		NULL,	OPT_light },
	{ "local",			required_argument,	NULL,	OPT_local },
	{ "mathalpha",		no_argument,		NULL,	OPT_mathalpha },
	{ "max-cont",		required_argument,	NULL,	OPT_max_cont },
	{ "max-image-cols",	required_argument,	NULL,	OPT_max_image_cols },
	{ "misskey",		no_argument,		NULL,	OPT_misskey, },
#if 0
	{ "ngword-add",		required_argument,	NULL,	OPT_ngword_add },
	{ "ngword-del",		required_argument,	NULL,	OPT_ngword_del },
	{ "ngword-list",	no_argument,		NULL,	OPT_ngword_list },
	{ "ngword-user",	required_argument,	NULL,	OPT_ngword_user },
#endif
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
	{ "show-cw",		no_argument,		NULL,	OPT_show_cw },
	{ "show-nsfw",		no_argument,		NULL,	OPT_show_nsfw },
#if 0
	{ "show-ng",		no_argument,		NULL,	OPT_show_ng },
#endif
	{ "timeout-image",	required_argument,	NULL,	OPT_timeout_image },
	{ "twitter",		no_argument,		NULL,	OPT_twitter },
	{ "version",		no_argument,		NULL,	OPT_version },
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
	// トークンのデフォルトファイル名は API version によって変わる
	// ので、デフォルト empty のままにしておく。

#if 0
	ngword_list.SetFileName(basedir + "ngword.json");
#endif

	address_family = AF_UNSPEC;
	opt_bgtheme = BG_NONE;
	color_mode = 256;
#if 0
	opt_show_ng = false;
#endif
	last_id = "";
	last_id_count = 0;
	last_id_max = 10;
	opt_progress = false;
	opt_ormode = false;
	opt_output_palette = true;
	opt_timeout_image = 3000;
	opt_eaw_a = 2;
	opt_eaw_n = 1;
	use_sixel = UseSixel::AutoDetect;
	opt_stream = StreamMode::Home;
	opt_proto = Proto::Misskey;

	while ((c = getopt_long(ac, av, "46h", longopts, NULL)) != -1) {
		switch (c) {
		 case '4':
			address_family = AF_INET;
			break;
		 case '6':
			address_family = AF_INET6;
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
		 case OPT_dark:
			opt_bgtheme = BG_DARK;
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
		 case OPT_debug_mbedtls:
#if defined(USE_MBEDTLS)
			val = stou32def(optarg, -1);
			if (val < 0 || val > 4) {
				errx(1, "--debug-mbedtls %s: debug level must be 0..4", optarg);
			}
			TLSHandle_mbedtls::SetLevel(val);
#endif
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
		 case OPT_debug_tls:
			val = stou32def(optarg, -1);
			if (val < 0 || val > 2) {
				errx(1, "--debug-tls %s: debug level must be 0..2", optarg);
			}
			TLSHandleBase::SetLevel(val);
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
#if defined(USE_TWITTER)
			opt_full_url = true;
#else
			errx(1, "--full-url is only supported with --twitter");
#endif
			break;
		 case OPT_home:
			cmd = SayakaCmd::Stream;
			opt_stream = StreamMode::Home;
			break;
		 case OPT_jis:
			output_codeset = "iso-2022-jp";
			break;
		 case OPT_light:
			opt_bgtheme = BG_LIGHT;
			break;
		 case OPT_local:
			cmd = SayakaCmd::Stream;
			opt_stream = StreamMode::Local;
			opt_server = optarg;
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
			opt_proto = Proto::Misskey;
			break;
#if 0
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
#endif
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
#if defined(USE_TWITTER)
			opt_protect = true;
#else
			errx(1, "--protect is only supported with --twitter");
#endif
			break;
		 case OPT_record:
			opt_record_mode = 1;
			record_file = optarg;
			break;
		 case OPT_record_all:
			opt_record_mode = 2;
			record_file = optarg;
			break;
		 case OPT_show_cw:
			opt_show_cw = true;
			break;
		 case OPT_show_nsfw:
			opt_show_nsfw = true;
			break;
#if 0
		 case OPT_show_ng:
			opt_show_ng = true;
			break;
#endif
		 case OPT_timeout_image:
			opt_timeout_image = stou32def(optarg, -1);
			if (opt_timeout_image < 0) {
				errno = EINVAL;
				err(1, "--timeout-image %s", optarg);
			}
			break;
		 case OPT_twitter:
#if defined(USE_TWITTER)
			opt_proto = Proto::Twitter;
#else
			errx(1, "Twitter support was not compiled.  See ./configure");
#endif
			break;
		 case OPT_version:
			cmd = SayakaCmd::Version;
			break;
		 case OPT_x68k:
			// 以下を指定したのと同じ
			color_mode = ColorFixedX68k;
			opt_fontwidth = 8;
			opt_fontheight = 16;
			output_codeset = "iso-2022-jp";
			opt_bgtheme = BG_DARK;
			opt_progress = true;
			opt_ormode = true;
			opt_output_palette = false;
			break;
		 case 'h':
			usage();
			break;
		 default:
			// 知らない引数はエラー。
			// getopt_long() がエラーを表示している。
			exit(1);
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
				printf(" --color x68k --font 8x16 --jis --dark"
				       " --progress --ormode on --palette on");
			} else {
				printf(" %s", av[i]);
			}
		}
		printf("\n");
	}

	// usage() は init() より前のほうがいいか。
	if (cmd == SayakaCmd::Noop) {
		warnx("No command option specified.");
		usage();
	}

	init();

	// コマンド別処理
	switch (cmd) {
	 case SayakaCmd::Stream:
		if (opt_proto == Proto::Misskey) {
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
		if (opt_proto == Proto::None) {
			errx(1, "--play must be used with --twitter or --misskey");
		}
		init_screen();
		cmd_play();
		break;
#if 0
	 case SayakaCmd::NgwordAdd:
		cmd_ngword_add();
		break;
	 case SayakaCmd::NgwordDel:
		cmd_ngword_del();
		break;
	 case SayakaCmd::NgwordList:
		cmd_ngword_list();
		break;
#endif
	 case SayakaCmd::Version:
		cmd_version();
		break;
	 default:
		errx(1, "Unknown command? cmd=%d", (int)cmd);
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
	if (opt_bgtheme == BG_NONE) {
		progress("Checking bgcolor of the terminal...");
		opt_bgtheme = terminal_bgtheme();
		progress("done\n");
		if (opt_bgtheme == BG_NONE) {
			printf("Terminal doesn't support control sequence, "
			       "switch to --light\n");
			opt_bgtheme = BG_LIGHT;
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

#if 0
	// NG ワード取得
	ngword_list.ReadFile();
#endif
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

#if 0
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
#endif

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
   command option:
	--local <server> : show <server>'s local timeline.
	--play : read JSON from stdin.
   other options:
	--color <n> : color mode { 2 .. 256 or x68k }. default 256.
	--font <width>x<height> : font size. default 7x14
	--full-url : display full URL even if the URL is abbreviated. (twitter)
	--light / --dark : Use light/dark theme. (default: auto detect)
	--no-color : disable all text color sequences
	--no-image : force disable (SIXEL) images.
	--force-sixel : force enable SIXEL images.
	--jis / --eucjp : Set output encoding.
	--progress: show startup progress (for very slow machines).
	--protect : don't display protected user's tweet. (twitter)
	--record <file> : record JSON to file.
	--record-all <file> : record all received JSON to file.
	--timeout-image <msec>
	--version
	--x68k : preset options for x68k (with SIXEL kernel).

	-4 / -6                         --ciphers <ciphers>
	--debug       <0-2>             --debug-format
	--debug-http  <0-2>             --debug-image <0-1>
	--debug-mbedtls <0-4>
	--debug-sixel <0-2>             --debug-show  <0-2>
	--mathalpha                     --no-combine
	--max-cont <n>                  --max-image-cols <n>
)"
#if 0
	--ngword-add                    --ngword-del
	--ngword-list                   --ngword-user
	--show-ng
#endif
R"(	--ormode <on|off> (default off) --palette <on|off> (default on)
)"
	);
	exit(0);
}

// 再生モード
void
cmd_play()
{
	FileStream stdinstream(stdin, false);

	for (;;) {
		std::string line;
		auto r = stdinstream.ReadLine(&line);
		if (__predict_false(r <= 0)) {
			break;
		}
		switch (opt_proto) {
		 case Proto::Twitter:
#if defined(USE_TWITTER)
			if (twitter_show_line(line) == false) {
				return;
			}
#else
			assert(false); // 来ないはず
#endif
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
