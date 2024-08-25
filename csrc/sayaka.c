/* vi:set ts=4: */
/*
 * Copyright (C) 2014-2024 Tetsuya Isaki
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

//
// sayaka
//

#include "sayaka.h"
#include "image.h"
#include <err.h>
#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <signal.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

enum {
	DEFAULT_FONT_WIDTH	= 7,
	DEFAULT_FONT_HEIGHT = 14,
};

enum {
	Cmd_none = 0,
	Cmd_stream,
	Cmd_play,
};

static void version(void);
static void usage(void);
static void help_all(void);
static bool init(void);
static void mkdir_if(const char *);
static void progress(const char *);
static void init_screen(void);
static void invalidate_cache(void);
static void signal_handler(int);
static void sigwinch(bool);

static const char *basedir;
const char *cachedir;
uint colormode;						// テキストの色数(モード)
char colorname[16];					// キャッシュファイルに使う色名
diag *diag_image;
diag *diag_json;
diag *diag_net;
diag *diag_term;
uint fontwidth;						// 使用するフォント幅   (ドット数)
uint fontheight;					// 使用するフォント高さ (ドット数)
uint iconsize;						// アイコンの大きさ (正方形、ドット数)
image_opt imageopt;					// 画像関係のオプション
uint imagesize;						// 画像の大きさ
uint indent_cols;					// インデント1階層の桁数
bool in_sixel;						// SIXEL 出力中か。
struct netstream_opt netopt;		// ネットワーク関係のオプション
int opt_bgtheme;					// -1:自動判別 0:Dark 1:Light
static uint opt_fontwidth;			// --font 指定の幅   (指定なしなら 0)
static uint opt_fontheight;			// --font 指定の高さ (指定なしなら 0)
uint opt_nsfw;						// NSFW コンテンツの表示方法
static bool opt_progress;
const char *opt_record_file;		// 録画ファイル名 (NULL なら録画しない)
bool opt_show_cw;					// CW を表示するか。
int opt_show_image;					// -1:自動判別 0:出力しない 1:出力する
uint screen_cols;					// 画面の桁数

enum {
	OPT__start = 0x7f,
	OPT_dark,
	OPT_debug_image,
	OPT_debug_json,
	OPT_debug_net,
	OPT_debug_term,
	OPT_font,
	OPT_help_all,
	OPT_light,
	OPT_max_image_cols,
	OPT_no_image,	// backward compatibility
	OPT_nsfw,
	OPT_play,
	OPT_progress,
	OPT_record,
	OPT_show_cw,
	OPT_show_image,
};

static const struct option longopts[] = {
	{ "color",			required_argument,	NULL,	'c' },
	{ "dark",			no_argument,		NULL,	OPT_dark },
	{ "debug-image",	required_argument,	NULL,	OPT_debug_image },
	{ "debug-json",		required_argument,	NULL,	OPT_debug_json },
	{ "debug-net",		required_argument,	NULL,	OPT_debug_net },
	{ "debug-term",		required_argument,	NULL,	OPT_debug_term },
	{ "font",			required_argument,	NULL,	OPT_font },
	{ "help-all",		no_argument,		NULL,	OPT_help_all },
	{ "light",			no_argument,		NULL,	OPT_light },
	{ "max-image-cols",	required_argument,	NULL,	OPT_max_image_cols },
	{ "no-image",		no_argument,		NULL,	OPT_no_image },
	{ "nsfw",			required_argument,	NULL,	OPT_nsfw },
	{ "play",			required_argument,	NULL,	OPT_play },
//	{ "progress",		no_argument,		NULL,	OPT_progress },
	{ "record",			required_argument,	NULL,	OPT_record },
	{ "show-cw",		no_argument,		NULL,	OPT_show_cw },
	{ "show-image",		required_argument,	NULL,	OPT_show_image },
	{ NULL },
};

static const struct optmap map_nsfw[] = {
	{ "hide",		NSFW_HIDE },
	{ "alt",		NSFW_ALT },
	{ "blur",		NSFW_BLUR },
	{ "show",		NSFW_SHOW },
	{ NULL },
};

#define SET_DIAG_LEVEL(name)	\
	 {	\
		int lv = stou32def(optarg, -1, NULL);	\
		if (lv < 0)	\
			errx(1, "invalid debug level: %s", optarg);	\
		diag_set_level(name, lv);	\
		break;	\
	 }

int
main(int ac, char *av[])
{
	int c;
	uint cmd;
	const char *playfile;

	diag_image= diag_alloc();
	diag_json = diag_alloc();
	diag_net  = diag_alloc();
	diag_term = diag_alloc();

	cmd = Cmd_none;
	image_opt_init(&imageopt);
	netstream_opt_init(&netopt);
	colormode = 256;
	opt_bgtheme = BG_AUTO;
	opt_fontwidth = 0;
	opt_fontheight = 0;
	opt_nsfw = NSFW_BLUR;
	opt_progress = false;
	opt_show_image = -1;
	playfile = NULL;

	imageopt.color   = ReductorColor_Fixed256;
	imageopt.method  = ReductorMethod_HighQuality;
	imageopt.diffuse = RDM_FS;

	while ((c = getopt_long(ac, av, "c:v", longopts, NULL)) != -1) {
		switch (c) {
		 case 'c':
		 {
			// ここは元々色数を指定しているのではなく、色モード指定。
			// -c 2 は、画像はモノクロで、テキストはボールドのみ飾り付けを行う。
			// -c 1 は、画像はモノクロで、テキストはボールドも含めて一切の
			// 飾り付けを行わない (ボールドがつらい端末を救済するため)。
			int n;
			if (strcmp(optarg, "1") == 0) {
				colormode = 1;
				imageopt.color = ReductorColor_GrayLevel(2);
			} else if (strcmp(optarg, "2") == 0) {
				colormode = 2;
				imageopt.color = ReductorColor_GrayLevel(2);
			} else if (strcmp(optarg, "8") == 0) {
				colormode = 8;
				imageopt.color = ReductorColor_Fixed8;
			} else if (strcmp(optarg, "16") == 0) {
				colormode = 16;
				imageopt.color = ReductorColor_ANSI16;
			} else if (strcmp(optarg, "256") == 0) {
				colormode = 256;
				imageopt.color = ReductorColor_Fixed256;
			} else if (strcmp(optarg, "gray") == 0 ||
			           strcmp(optarg, "grey") == 0) {
				colormode = 2;
				imageopt.color = ReductorColor_GrayLevel(256);
			} else if ((strncmp(optarg, "gray", 4) == 0 ||
			            strncmp(optarg, "grey", 4) == 0   ) &&
			           (n = stou32def(optarg + 4, -1, NULL)) != -1 &&
			           (2 <= n && n <= 256)) {
				colormode = 2;
				imageopt.color = ReductorColor_GrayLevel(n);
			} else {
				errx(1, "%s: invalid color mode", optarg);
			}
			break;
		 }

		 case OPT_dark:
			opt_bgtheme = BG_DARK;
			break;

		 case OPT_debug_image:
			SET_DIAG_LEVEL(diag_image);
			break;

		 case OPT_debug_json:
			SET_DIAG_LEVEL(diag_json);
			break;

		 case OPT_debug_net:
			SET_DIAG_LEVEL(diag_net);
			break;

		 case OPT_debug_term:
			SET_DIAG_LEVEL(diag_term);
			break;

		 case OPT_font:
		 {
			// "7x14" のような形式で指定する。
			char buf[32];
			strlcpy(buf, optarg, sizeof(buf));
			char *h = strchr(buf, 'x');
			if (h == NULL) {
				errx(1, "--font %s: argument must be <W>x<H>", optarg);
			}
			*h++ = '\0';
			int width  = stou32def(buf, -1, NULL);
			int height = stou32def(h, -1, NULL);
			if (width < 1 || height < 1) {
				errno = EINVAL;
				err(1, "--font %s", optarg);
			}
			opt_fontwidth  = (uint)width;
			opt_fontheight = (uint)height;
			break;
		 }

		 case OPT_help_all:
			help_all();
			exit(0);

		 case OPT_light:
			opt_bgtheme = BG_LIGHT;
			break;

		 case OPT_max_image_cols:
			max_image_count = stou32def(optarg, -1, NULL);
			if (max_image_count < 0) {
				errno = EINVAL;
				err(1, "--max-image-cols %s", optarg);
			}
			break;

		 case OPT_no_image:
			warnx("--no-image is obsolete.  --show-image=no is used instead.");
			opt_show_image = 0;
			break;

		 case OPT_nsfw:
			opt_nsfw = parse_optmap(map_nsfw, optarg);
			if ((int)opt_nsfw < 0) {
				errx(1, "--nsfw %s: must be 'show', 'blur', 'alt', or 'hide'",
					optarg);
			}
			break;

		 case OPT_play:
		 {
			if (strcmp(optarg, "-") == 0) {
				playfile = NULL;
			} else {
				playfile = optarg;
			}
			cmd = Cmd_play;
			break;
		 }

		 case OPT_progress:
			opt_progress = true;
			break;

		 case OPT_record:
			opt_record_file = optarg;
			break;

		 case OPT_show_cw:
			opt_show_cw = true;
			break;

		 case OPT_show_image:
			if (strcmp(optarg, "auto") == 0) {
				opt_show_image = -1;
			} else if (strcmp(optarg, "no") == 0) {
				opt_show_image = 0;
			} else if (strcmp(optarg, "yes") == 0) {
				opt_show_image = 1;
			} else {
				errx(1,
					"--show-image=<option> must be one of [ auto | no | yes ]");
			}
			break;

		 case 'v':
			version();
			exit(0);

		 default:
			usage();
			exit(0);
		}
	}
	ac -= optind;
	av += optind;

	// XXX とりあえず
	if (ac > 0) {
		cmd = Cmd_stream;
	}

	if (cmd == Cmd_none) {
		usage();
		exit(0);
	}

	// コマンド共通の初期化。
	if (init() == false) {
		err(1, "init failed");
	}

	if (cmd == Cmd_stream || cmd == Cmd_play) {
		// 表示系の初期化。
		init_screen();

		if (cmd == Cmd_stream) {
			// XXX とりあえずいきなり引数でサーバを指定する。
			if (ac < 1) {
				errx(1, "please specify server name");
			}

			// 古いキャッシュを削除する。
			progress("Deleting expired cache files...");
			invalidate_cache();
			progress("done\n");

			cmd_misskey_stream(av[0]);
		} else {
			cmd_misskey_play(playfile);
		}
	}

	return 0;
}

static void
version(void)
{
	fprintf(stderr, "sayaka (csrc)\n");
}

static void
usage(void)
{
	fprintf(stderr, "usage: %s [<options...>]\n", getprogname());
}

static void
help_all(void)
{
	fprintf(stderr, "usage: %s [<options>...]\n", getprogname());
	fprintf(stderr,
"  -c,--color=<color> : Specify color mode (default: 256)\n"
"     1        : Monochrome image, and text has no escape sequences\n"
"     2        : Monochrome (2-level grayscale) image\n"
"     8        : Fixed RGB 8 colors\n"
"     16       : Fixed ANSI compatible 16 colors\n"
"     256      : Fixed 256 colors (MSX SCREEN8 compatible palette)\n"
"     gray[<n>]: (2..256) shades of grayscale. 256 if <n> is ommitted.\n"
"                'gray2' is a synonym for '2'.\n"
"  --dark/--light\n"
"  --debug-image=<0..2>\n"
"  --debug-json=<0..2>\n"
"  --debug-net=<0..2>\n"
"  --debug-term=<0..2>\n"
"  --font=<W>x<H>\n"
"  --help-all  : This help.\n"
"  --max-image-cols=<n>\n"
"  --nsfw=<mode>\n"
"  --play=<filename|->\n"
"  --progress\n"
"  --record=<filename>\n"
"  --show-cw\n"
"  --show-image=<mode>\n"
"  -v,--version\n"
	);
}

// コマンド共通の初期化。
static bool
init(void)
{
	char buf[PATH_MAX];

	const char *home = getenv("HOME");
	if (home == NULL) {
		home = "";
	}

	// パスを用意。
	snprintf(buf, sizeof(buf), "%s/.sayaka/", home);
	basedir = strdup(buf);
	strlcat(buf, "cache", sizeof(buf));
	cachedir = strdup(buf);
	if (basedir == NULL || cachedir == NULL) {
		return false;
	}

	// ディレクトリを作成。
	mkdir_if(basedir);
	mkdir_if(cachedir);

	// シグナルハンドラを設定。
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

	return true;
}

// dirname がなければディレクトリを作成する。
// 失敗すると err(3) で終了する。
static void
mkdir_if(const char *dirname)
{
	struct stat st;
	int r;

	r = stat(dirname, &st);
	if (r < 0 && errno == ENOENT) {
		r = mkdir(dirname, 0755);
		if (r < 0) {
			err(1, "%s: mkdir %s", __func__, dirname);
		}
		printf("%s: create %s\n", __func__, dirname);
	}
}

// 起動経過を表示。(遅マシン用)
static void
progress(const char *msg)
{
	if (__predict_false(opt_progress)) {
		fputs(msg, stdout);
		fflush(stdout);
	}
}

// 表示周りの初期化。
static void
init_screen(void)
{
	// 端末の背景色を調べる (オプションで指定されてなければ)。
	// 判定できなければ背景色白をデフォルトにしておく。
	if (opt_bgtheme == BG_AUTO) {
		progress("Checking background color...");
		opt_bgtheme = terminal_get_bgtheme();
		switch (opt_bgtheme) {
		 case BG_LIGHT:
			progress("light");
			break;
		 case BG_DARK:
			progress("dark");
			break;
		 default:
			progress("done");
			warnx("Terminal doesn't support contol sequence; "
				"assume --light");
			opt_bgtheme = BG_LIGHT;
			break;
		}
	}

	// 端末が SIXEL をサポートしているか。
	if (opt_show_image == -1) {
		progress("Checking whether the terminal supports sixel...");
		opt_show_image = terminal_support_sixel();
		if (opt_show_image == 0) {
			progress("no\n");
		} else if (opt_show_image == 1) {
			progress("yes\n");
		} else {
			progress("?\n");
			warnx("terminal_support_sixel returns %d", opt_show_image);
			opt_show_image = 0;
		}
	}

	// 出力文字コードの初期化。

	// 色の初期化。
	init_color();

	// キャッシュファイル用の色モード名。
	switch (imageopt.color & ReductorColor_MASK) {
	 case ReductorColor_Gray:
	 {
		uint grayscale = (imageopt.color >> 8) + 1;
		if (grayscale == 2) {
			strlcpy(colorname, "2", sizeof(colorname));
		} else {
			snprintf(colorname, sizeof(colorname), "gray%u", grayscale);
		}
		break;
	 }
	 case ReductorColor_Fixed8:
		strlcpy(colorname, "8", sizeof(colorname));
		break;
	 case ReductorColor_ANSI16:
		strlcpy(colorname, "16", sizeof(colorname));
		break;
	 case ReductorColor_Fixed256:
		strlcpy(colorname, "256", sizeof(colorname));
		break;
	 default:
		// ?
		snprintf(colorname, sizeof(colorname), "RC%u", imageopt.color);
		break;
	}

	// 一度手動で呼び出して桁数を取得。
	sigwinch(true);
}

// 古いキャッシュを削除する。
static void
invalidate_cache(void)
{
	char cmd[1024];

	// アイコンは1か月分くらいか
	snprintf(cmd, sizeof(cmd),
		"find %s -name icon-\\* -type f -atime +30 -exec rm {} +", cachedir);
	if (system(cmd) < 0) {
		warn("system(find icon)");
	}

	// 写真は2日分くらいか
	snprintf(cmd, sizeof(cmd),
		"find %s -name http\\* -type f -atime +2 -exec rm {} +", cachedir);
	if (system(cmd) < 0) {
		warn("system(find photo)");
	}
}

static void
signal_handler(int signo)
{
	switch (signo) {
	 case SIGINT:
		// SIXEL 出力中なら中断する。
		if (in_sixel) {
			printf(CAN ESC "\\");
			fflush(stdout);
		} else {
			exit(0);
		}
		break;

	 case SIGWINCH:
		sigwinch(false);
		break;

	 default:
		warnx("caught signal %d", signo);
		break;
	}
}

// SIGWINCH の処理。
static void
sigwinch(bool initializing)
{
	struct winsize ws;
	int ws_cols = 0;
	int ws_width = 0;
	int ws_height = 0;
	const char *msg_cols = "";
	const char *msg_width = "";
	const char *msg_height = "";
	int r;

	r = ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws);
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

	// 画面幅は常に更新
	if (ws_cols > 0) {
		screen_cols = ws_cols;
		msg_cols = " (from ioctl)";
	} else {
		screen_cols = 0;
		msg_cols = " (not detected)";
	}

	// フォント幅と高さは指定されてない時だけ取得した値を使う
	bool use_default_font = false;
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

	const char *f = (initializing ? "init_screen" : __func__);
	Debug(diag_term, "%s: screen columns=%u%s", f, screen_cols, msg_cols);
	Debug(diag_term, "%s: font height=%u%s", f, fontheight, msg_height);
	Debug(diag_term, "%s: font width=%u%s", f, fontwidth, msg_width);
	Debug(diag_term, "%s: iconsize=%u, indent_columns=%u imagesize=%u",
		f, iconsize, indent_cols, imagesize);
}
