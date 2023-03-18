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

#include "StringUtil.h"
#include "UString.h"
#include "eaw_code.h"
#include "main.h"
#include "term.h"
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

static const char version[] = "3.6.0 (2023/03/18)";

#define CONSUMER_KEY		"jPY9PU5lvwb6s9mqx3KjRA"
#define CONSUMER_SECRET		"faGcW9MMmU0O6qTrsHgcUchAiqxDcU9UjDW2Zw"

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

static const int DEFAULT_FONT_WIDTH = 7;
static const int DEFAULT_FONT_HEIGHT = 14;

static std::string GetHomeDir();
static void init();
static void init_stream();
static void get_access_token();
static void signal_handler(int signo);
static void sigwinch();
static void cmd_users_list(const StringDictionary& list);
static void cmd_followlist();
static void cmd_blocklist();
static void cmd_mutelist();
static void cmd_nortlist();
static void cmd_ngword_add();
static void cmd_ngword_del();
static void cmd_ngword_list();
static void cmd_version();
[[noreturn]] static void usage();

// enum は getopt() の1文字のオプションと衝突しなければいいので
// 適当に 0x80 から始めておく。
enum {
	OPT_black = 0x80,
	OPT_blocklist,
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
	OPT_filter,
	OPT_followlist,
	OPT_font,
	OPT_force_sixel,
	OPT_full_url,
	OPT_home,
	OPT_jis,
	OPT_mathalpha,
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
	{ "debug-format",	no_argument,		NULL,	OPT_debug_format },
	{ "debug-http",		required_argument,	NULL,	OPT_debug_http },
	{ "debug-image",	required_argument,	NULL,	OPT_debug_image },
	{ "debug-show",		required_argument,	NULL,	OPT_debug_show },
	{ "debug-sixel",	required_argument,	NULL,	OPT_debug_sixel },
	{ "eaw-a",			required_argument,	NULL,	OPT_eaw_a },
	{ "eaw-n",			required_argument,	NULL,	OPT_eaw_n },
	{ "euc-jp",			no_argument,		NULL,	OPT_euc_jp },
	{ "filter",			required_argument,	NULL,	OPT_filter },
	{ "followlist",		no_argument,		NULL,	OPT_followlist },
	{ "font",			required_argument,	NULL,	OPT_font },
	{ "force-sixel",	no_argument,		NULL,	OPT_force_sixel },
	{ "full-url",		no_argument,		NULL,	OPT_full_url },
	{ "home",			no_argument,		NULL,	OPT_home },
	{ "jis",			no_argument,		NULL,	OPT_jis },
	{ "mathalpha",		no_argument,		NULL,	OPT_mathalpha },
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
	{ "timeout-image",	required_argument,	NULL,	OPT_timeout_image },
	{ "token",			required_argument,	NULL,	OPT_token },
	{ "version",		no_argument,		NULL,	OPT_version },
	{ "white",			no_argument,		NULL,	OPT_white },
	{ "x68k",			no_argument,		NULL,	OPT_x68k },
	{ "help",			no_argument,		NULL,	'h' },
	{ NULL },
};

int
main(int ac, char *av[])
{
	int c;
	int val;

	diagHttp.SetClassname("HttpClient");

	SayakaCmd cmd = SayakaCmd::Noop;
	basedir     = GetHomeDir() + "/.sayaka/";
	cachedir    = basedir + "cache";
	tokenfile   = basedir + "token.json";
	colormapdir = basedir;
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
		 case OPT_filter:
			cmd = SayakaCmd::Stream;
			opt_filter.emplace_back(optarg);
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
			opt_pseudo_home = true;
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
			use_sixel = UseSixel::No;
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
		 case OPT_post:
			cmd = SayakaCmd::Tweet;
			break;
		 case OPT_progress:
			opt_progress = true;
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

	// どのコマンドでもなくキーワードだけならフィルタモード
	if (optind < ac && cmd == SayakaCmd::Noop) {
		cmd = SayakaCmd::Stream;
		for (int i = optind; i < ac; i++) {
			opt_filter.emplace_back(av[i]);
		}
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

	// 暫定
	if (opt_pseudo_home == false) {
		warnx("--home is mandatory for now...");
		usage();
	}

	if (opt_pseudo_home) {
		if (!opt_filter.empty()) {
			warnx("filter keyword and --home must be exclusive.");
			usage();
		}
	}

	Debug(diag, "tokenfile = %s", tokenfile.c_str());
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

// ストリームモードのための準備
void
init_stream()
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

// OAuth オブジェクトを初期化
void
InitOAuth()
{
	// XXX まだこの再入チェックいるかな?
	if (oauth.ConsumerKey.empty()) {
		oauth.SetDiag(diagHttp);
		oauth.ConsumerKey    = CONSUMER_KEY;
		oauth.ConsumerSecret = CONSUMER_SECRET;

		// ファイルからトークンを取得
		// なければトークンを取得してファイルに保存
		bool r = oauth.LoadTokenFromFile(tokenfile);
		if (r == false) {
			get_access_token();
		}
	}
}

// アクセストークンを取得する。
// 取得できなければ errx(3) で終了する。
static void
get_access_token()
{
	oauth.AdditionalParams.clear();

	Debug(diag, "----- Request Token -----");
	oauth.RequestToken(REQUEST_TOKEN_URL);

	printf("Please go to:\n"
		AUTHORIZE_URL "?oauth_token=%s\n", oauth.AccessToken.c_str());
	printf("\n");
	printf("And input PIN code: ");
	fflush(stdout);

	char pin_str[1024];
	fgets(pin_str, sizeof(pin_str), stdin);

	Debug(diag, "----- Access Token -----");

	oauth.AdditionalParams["oauth_verifier"] = pin_str;
	oauth.RequestToken(ACCESS_TOKEN_URL);

	if (oauth.AccessToken.empty()) {
		errx(1, "GIVE UP");
	}

	bool r = oauth.SaveTokenToFile(tokenfile);
	if (r == false) {
		errx(1, "Token save failed");
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

// ユーザ一覧を表示するコマンド(共通部分)
static void
cmd_users_list(const StringDictionary& list)
{
	for (const auto& kv : list) {
		const auto& key = kv.first;
		printf("%s\n", key.c_str());
	}
}

// フォローユーザの一覧を取得して表示するコマンド
static void
cmd_followlist()
{
	InitOAuth();
	cmd_users_list(get_follow_list());
}

// ブロックユーザの一覧を取得して表示するコマンド
static void
cmd_blocklist()
{
	InitOAuth();
	cmd_users_list(get_block_list());
}

// ミュートユーザの一覧を取得して表示するコマンド
static void
cmd_mutelist()
{
	InitOAuth();
	cmd_users_list(get_mute_list());
}

// RT 非表示ユーザの一覧を取得して表示するコマンド
static void
cmd_nortlist()
{
	InitOAuth();
	cmd_users_list(get_nort_list());
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
R"(usage: sayaka [<options>...] --home
       sayaka [<options>...] <keyword>
	--color <n> : color mode { 2 .. 256 or x68k }. default 256.
	--font <width>x<height> : (default: 7x14)
	--filter <keyword>
	--full-url : display full URL even if the URL is abbreviated.
	--white / --black : darken/lighten the text color. (default: autodetect)
	--no-color : disable all text color sequences
	--no-image : force disable (SIXEL) images.
	--force-sixel : force enable SIXEL images.
	--jis
	--eucjp
	--play : read JSON from stdin.
	--post : post tweet from stdin (utf-8 is expected).
	--progress: show startup progress (for very slow machines).
	--record <file> : record JSON to file.
	--record-all <file> : record all received JSON to file.
	--show-ng
	--timeout-image <msec>
	--token <file> : token file (default: ~/.sayaka/token.json)
	--version
	--x68k : preset options for x68k (with SIXEL kernel).

	-4
	-6
	--blocklist
	--ciphers <ciphers>
	--debug       <0-2>
	--debug-http  <0-2>
	--debug-show  <0-2>
	--debug-image <0-1>
	--debug-sixel <0-2>
	--debug-format
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
