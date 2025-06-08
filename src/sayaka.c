/* vi:set ts=4: */
/*
 * Copyright (C) 2014-2025 Tetsuya Isaki
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
#include "ngword.h"
#include <err.h>
#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

#define SAYAKA_VERSION "3.8.2"
#define SAYAKA_RELDATE "2025/05/17"

enum {
	DEFAULT_FONT_WIDTH	= 7,
	DEFAULT_FONT_HEIGHT = 14,
};

enum {
	CMD_NONE = 0,
	CMD_STREAM,
	CMD_PLAY,
};

// ヘッダの依存関係を減らすため。
extern struct image_opt imageopt;

static void version(void);
static void usage(void);
static void help_all(void);
static bool init(void);
static void mkdir_if(const char *);
static void progress(const char *);
static void init_screen(void);
static void init_ngword(void);
static void invalidate_cache(void);
static const char *get_token(const char *);
static void signal_handler(int);
static void sigwinch(bool);

const char progname[] = "sayaka";
const char progver[]  = SAYAKA_VERSION;

static const char *basedir;
const char *cachedir;
uint colormode;						// テキストの色数(モード)
char colorname[16];					// キャッシュファイルに使う色名
struct diag *diag_format;
struct diag *diag_image;
struct diag *diag_json;
struct diag *diag_net;
struct diag *diag_term;
uint fontwidth;						// 使用するフォント幅   (ドット数)
uint fontheight;					// 使用するフォント高さ (ドット数)
uint iconsize;						// アイコンの大きさ (正方形、ドット数)
struct image_opt imageopt;			// 画像関係のオプション
uint imagesize;						// 画像の大きさ
uint indent_cols;					// インデント1階層の桁数
bool in_sixel;						// SIXEL 出力中か。
struct net_opt netopt_image;		// 画像ダウンロード用ネットワークオプション
struct net_opt netopt_main;			// メインストリーム用ネットワークオプション
struct ngwords *ngwords;			// NG ワード集
int opt_bgtheme;					// -1:自動判別 0:Dark 1:Light
const char *opt_codeset;			// 出力文字コード (NULL なら UTF-8)
static uint opt_fontwidth;			// --font 指定の幅   (指定なしなら 0)
static uint opt_fontheight;			// --font 指定の高さ (指定なしなら 0)
uint opt_nsfw;						// NSFW コンテンツの表示方法
bool opt_overwrite_cache;			// キャッシュファイルを更新する
static bool opt_progress;
const char *opt_record_file;		// 録画ファイル名 (NULL なら録画しない)
bool opt_show_cw;					// CW を表示するか。
int opt_show_image;					// -1:自動判別 0:出力しない 1:出力する
uint screen_cols;					// 画面の桁数

enum {
	OPT__start = 0x7f,
	OPT_ciphers,
	OPT_dark,
	OPT_debug_format,
	OPT_debug_image,
	OPT_debug_json,
	OPT_debug_net,
	OPT_debug_term,
	OPT_eaw_a,
	OPT_eaw_n,
	OPT_euc_jp,
	OPT_font,
	OPT_help,
	OPT_help_all,
	OPT_ipv4,
	OPT_ipv6,
	OPT_jis,
	OPT_light,
	OPT_mathalpha,
	OPT_max_image_cols,
	OPT_misskey,
	OPT_no_combine,
	OPT_no_image,	// backward compatibility
	OPT_nsfw,
	OPT_overwrite_cache,
	OPT_progress,
	OPT_show_cw,
	OPT_show_image,
	OPT_sixel_or,
	OPT_timeout_image,
};

static const struct option longopts[] = {
	{ "ciphers",		required_argument,	NULL,	OPT_ciphers },
	{ "color",			required_argument,	NULL,	'c' },
	{ "dark",			no_argument,		NULL,	OPT_dark },
	{ "debug-format",	required_argument,	NULL,	OPT_debug_format },
	{ "debug-image",	required_argument,	NULL,	OPT_debug_image },
	{ "debug-json",		required_argument,	NULL,	OPT_debug_json },
	{ "debug-net",		required_argument,	NULL,	OPT_debug_net },
	{ "debug-term",		required_argument,	NULL,	OPT_debug_term },
	{ "eaw-a",			required_argument,	NULL,	OPT_eaw_a },
	{ "eaw-n",			required_argument,	NULL,	OPT_eaw_n },
	{ "euc-jp",			no_argument,		NULL,	OPT_euc_jp },
	{ "font",			required_argument,	NULL,	OPT_font },
	{ "help",			no_argument,		NULL,	OPT_help },
	{ "help-all",		no_argument,		NULL,	OPT_help_all },
	{ "home",			no_argument,		NULL,	'h' },
	{ "ipv4",			no_argument,		NULL,	OPT_ipv4 },
	{ "ipv6",			no_argument,		NULL,	OPT_ipv6 },
	{ "jis",			no_argument,		NULL,	OPT_jis },
	{ "light",			no_argument,		NULL,	OPT_light },
	{ "local",			no_argument,		NULL,	'l' },
	{ "mathalpha",		no_argument,		NULL,	OPT_mathalpha },
	{ "max-image-cols",	required_argument,	NULL,	OPT_max_image_cols },
	{ "misskey",		no_argument,		NULL,	OPT_misskey },
	{ "no-combine",		no_argument,		NULL,	OPT_no_combine },
	{ "no-image",		no_argument,		NULL,	OPT_no_image },
	{ "nsfw",			required_argument,	NULL,	OPT_nsfw },
	{ "overwrite-cache",no_argument,		NULL,	OPT_overwrite_cache },
	{ "play",			required_argument,	NULL,	'p' },
	{ "progress",		no_argument,		NULL,	OPT_progress },
	{ "record",			required_argument,	NULL,	'r' },
	{ "server",			required_argument,	NULL,	's' },
	{ "show-cw",		no_argument,		NULL,	OPT_show_cw },
	{ "show-image",		required_argument,	NULL,	OPT_show_image },
	{ "sixel-or",		no_argument,		NULL,	OPT_sixel_or },
	{ "timeout-image",	required_argument,	NULL,	OPT_timeout_image },
	{ "token",			required_argument,	NULL,	't' },
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
	const char *token_file;
	const char *server;
	const char *playfile;
	bool is_home;

	diag_format = diag_alloc();
	diag_image = diag_alloc();
	diag_json = diag_alloc();
	diag_net  = diag_alloc();
	diag_term = diag_alloc();
	diag_set_timestamp(diag_net, true);

	cmd = CMD_NONE;
	image_opt_init(&imageopt);
	net_opt_init(&netopt_image);
	net_opt_init(&netopt_main);
	colormode = 256;
	opt_bgtheme = BG_AUTO;
	opt_eaw_a = 2;
	opt_eaw_n = 1;
	opt_fontwidth = 0;
	opt_fontheight = 0;
	opt_nsfw = NSFW_BLUR;
	opt_progress = false;
	opt_show_image = -1;
	token_file = NULL;
	server = NULL;
	playfile = NULL;
	is_home = false;

	netopt_image.timeout_msec = 3000;

	while ((c = getopt_long(ac, av, "c:hlp:r:s:t:v", longopts, NULL)) != -1) {
		switch (c) {
		 case 'c':
			// ここは元々色数を指定しているのではなく、色モード指定。
			// -c 2 は、画像はモノクロで、テキストはボールドのみ飾り付けを行う。
			// -c 1 は、画像はモノクロで、テキストはボールドも含めて一切の
			// 飾り付けを行わない (ボールドがつらい端末を救済するため)。
			if (strcmp(optarg, "1") == 0) {
				colormode = 1;
				imageopt.color = MAKE_COLOR_MODE_GRAY(2);
			} else {
				imageopt.color = image_parse_color(optarg);
				if (imageopt.color == COLOR_MODE_NONE) {
					errx(1, "%s: invalid color mode", optarg);
				}
				switch (GET_COLOR_MODE(imageopt.color)) {
				 case COLOR_MODE_GRAY:			colormode = 2;		break;
				 case COLOR_MODE_8_RGB:			colormode = 8;		break;
				 case COLOR_MODE_16_VGA:		colormode = 16;		break;
				 case COLOR_MODE_256_RGB332:	colormode = 256;	break;
				}
			}
			// 8色以下くらいだと --cdm 0.375 くらいが絶妙。
			if (colormode <= 8) {
				imageopt.cdm = 96;
			}
			break;

		 case OPT_ciphers:
			// 今のところ "RSA" (大文字) しか指定できない。
			if (strcmp(optarg, "RSA") == 0) {
				netopt_main.use_rsa_only = true;
				netopt_image.use_rsa_only = true;
			} else {
				errx(1, "Invalid ciphers: '%s'", optarg);
			}
			break;

		 case OPT_dark:
			opt_bgtheme = BG_DARK;
			break;

		 case OPT_debug_format:
			SET_DIAG_LEVEL(diag_format);
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

		 case OPT_eaw_a:
			opt_eaw_a = stou32def(optarg, -1, NULL);
			if (opt_eaw_a < 1 || opt_eaw_a > 2) {
				errx(1, "--eaw-a %s: must be either 1 or 2", optarg);
			}
			break;

		 case OPT_eaw_n:
			opt_eaw_n = stou32def(optarg, -1, NULL);
			if (opt_eaw_n < 1 || opt_eaw_n > 2) {
				errx(1, "--eaw-n %s: must be either 1 or 2", optarg);
			}
			break;

		 case OPT_euc_jp:
			opt_codeset = "euc-jp";
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

		 case 'h':
			cmd = CMD_STREAM;
			is_home = true;
			break;

		 // OPT_help は default:

		 case OPT_help_all:
			help_all();
			exit(0);

		 case OPT_ipv4:
			netopt_main.address_family = 4;
			netopt_image.address_family = 4;
			break;

		 case OPT_ipv6:
			netopt_main.address_family = 6;
			netopt_image.address_family = 6;
			break;

		 case OPT_jis:
			opt_codeset = "iso-2022-jp";
			break;

		 case 'l':
			cmd = CMD_STREAM;
			is_home = false;
			break;

		 case OPT_light:
			opt_bgtheme = BG_LIGHT;
			break;

		 case OPT_mathalpha:
			opt_mathalpha = true;
			break;

		 case OPT_max_image_cols:
			max_image_count = stou32def(optarg, -1, NULL);
			if (max_image_count < 0) {
				errno = EINVAL;
				err(1, "--max-image-cols %s", optarg);
			}
			break;

		 case OPT_misskey:
			// 今のところ何もしない。
			break;

		 case OPT_no_combine:
			opt_nocombine = true;
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

		 case OPT_overwrite_cache:
			opt_overwrite_cache = true;
			break;

		 case 'p':
			if (strcmp(optarg, "-") == 0) {
				playfile = NULL;
			} else {
				playfile = optarg;
			}
			cmd = CMD_PLAY;
			break;

		 case OPT_progress:
			opt_progress = true;
			break;

		 case 'r':
			opt_record_file = optarg;
			break;

		 case 's':
			server = optarg;
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

		 case OPT_sixel_or:
			imageopt.output_ormode = true;
			break;

		 case 't':
			token_file = optarg;
			break;

		 case OPT_timeout_image:
			netopt_image.timeout_msec = stou32def(optarg, -1, NULL);
			if ((int32)netopt_image.timeout_msec == -1) {
				errno = EINVAL;
				err(1, "--timeout-image %s", optarg);
			}
			break;

		 case 'v':
			version();
			exit(0);

		 case OPT_help:
		 default:
			usage();
			exit(0);
		}
	}
	ac -= optind;
	av += optind;

	if (cmd == CMD_NONE) {
		usage();
		exit(0);
	}

	// コマンド共通の初期化。
	if (init() == false) {
		err(1, "init failed");
	}

	if (cmd == CMD_STREAM || cmd == CMD_PLAY) {
		init_ngword();
		init_screen();

		if (cmd == CMD_STREAM) {
			if (server == NULL) {
				errx(1, "server must be specified");
			}

			const char *token = NULL;
			if (token_file) {
				token = get_token(token_file);
			} else if (is_home) {
				errx(1, "Home timeline requires your access token");
			}

			// 古いキャッシュを削除する。
			progress("Deleting expired cache files...");
			invalidate_cache();
			progress("done\n");

			cmd_misskey_stream(server, is_home, token);
		} else {
			cmd_misskey_play(playfile);
		}
	}

	return 0;
}

static void
version(void)
{
	printf("%s %s (%s) - Misskey stream client\n",
		progname, progver, SAYAKA_RELDATE);
}

static void
usage(void)
{
	fprintf(stderr, "usage: %s <command> [<options...>]\n", progname);
	fprintf(stderr,
" <command>\n"
"  -h,--home           : Home timeline mode (needs --server and --token)\n"
"  -l,--local          : Local timeline mode (needs --server)\n"
"  -p,--play=<file|->  : Playback mode\n"
" <options>\n"
"  -s,--server=<host>  : Set misskey server\n"
"  -t,--token=<file>   : Set misskey access token file\n"
"  -c,--color=<color>  : 256,16,8,2,1 and gray[2-256] (default:256)\n"
"  --show-cw           : Open CW(Contents Warning) part\n"
"  --nsfw=<show|blur|alt|hide> : How to show NSFW contents (default:blur)\n"
"  --show-image=<yes|no|auto>  : Whether to show image or not (default:auto)\n"
"  -r,--record=<file>  : Record JSON to <file>\n"
"  --help-all          : More details\n"
	);
}

static void
help_all(void)
{
	fprintf(stderr, "usage: %s <command> [<options>...]\n", progname);
	fprintf(stderr,
" <command>\n"
"  -h,--home              : Home timeline mode (needs --server and --token)\n"
"  -l,--local             : Local timeline mode (needs --server)\n"
"  -p,--play=<file|->     : Playback mode ('-' means stdin)\n"
" <options>\n"
"  -c,--color=<colormode> : Set color mode (default:256)\n"
"     256      : Fixed 256 colors (MSX SCREEN8 compatible palette)\n"
"     16       : Fixed ANSI compatible 16 colors\n"
"     8        : Fixed RGB 8 colors\n"
"     2        : Monochrome image, but use text bold sequence\n"
"     1        : Monochrome image, and disable any text color sequences\n"
"     gray[<n>]: (2..256) shades of grayscale. If <n> is omitted, 256 is used\n"
"                'gray2' is a synonym for '2'\n"
"  --ciphers=<ciphers>    : \"RSA\" can only be specified\n"
"  --dark / --light       : Assume background color (default:auto detect)\n"
"  --eaw-a=<1|2>          : Width of Unicode EAW Anbiguous char (default:2)\n"
"  --eaw-n=<1|2>          : Width of Unicode EAW Neutral char   (defualt:1)\n"
"  --euc-jp / --jis       : Set output charset\n"
"  --font=<W>x<H>         : Set font size (Normally autodetected)\n"
"  --help-all             : This help\n"
"  --ipv4 / --ipv6        : Connect only IPv4/v6 for both stream and images\n"
"  --mathalpha            : Use alternate character for some MathAlpha chars\n"
"  --misskey              : Set misskey mode (No other choices at this point)\n"
"  --max-image-cols=<n>   : Set max number of images per line\n"
"                           0 means much as possible (default:0)\n"
"  --no-conbine           : Don't combine Unicode combined characters\n"
"  --nsfw=<mode>          : How to show NSFW images (default:blur)\n"
"     show     : Show image as is\n"
"     blur     : Show blurred image\n"
"     alt      : Hide image but display only filetype\n"
"     hide     : Hide this note itself if the note has NSFW contents\n"
"  --overwrite-cache      : Don't use cache file and overwrite it by new one\n"
"  --progress             : Show startup progress (for slow machines)\n"
"  -r,--record=<file>     : Record JSON to <file>\n"
"  -s,--server=<host>     : Set misskey server\n"
"  --sixel-or             : Output SIXEL by OR-mode\n"
"  --show-cw              : Open CW(Contents Warning) part\n"
"  --show-image=<mode>    : Whether to show image or not (default:auto)\n"
"     yes      : Force output SIXEL image even if terminal doesn't support\n"
"     no       : Don't output SIXEL image (--no-image can be used)\n"
"     auto     : Auto detect\n"
"  --timeout-image=<msec> : Set connection timeout for image (default:3000)\n"
"  -t,--token=<file>      : Set misskey access token file\n"
"  -v,--version\n"
"  --debug-format=<0..2>\n"
"  --debug-image=<0..2>\n"
"  --debug-json=<0..2>\n"
"  --debug-net=<0..2>\n"
"  --debug-term=<0..2>\n"
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
	snprintf(buf, sizeof(buf), "%s/.sayaka", home);
	basedir = strdup(buf);
	strlcat(buf, "/cache", sizeof(buf));
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
		warnx("create %s", dirname);
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

// NG ワードの初期化。
static void
init_ngword(void)
{
	char filename[PATH_MAX];

	snprintf(filename, sizeof(filename), "%s/ngword.json", basedir);
	ngwords = ngword_read_file(filename, diag_format/*?*/);
}

// 表示周りの初期化。
static void
init_screen(void)
{
	// 出力先が端末かどうか。
	bool is_tty = (isatty(STDOUT_FILENO) != 0);

	// 端末の背景色を調べる。
	// 判定できなければ背景色白をデフォルトにしておく。
	// モノクロモードなら不要。
	if (opt_bgtheme == BG_AUTO && is_tty && colormode > 2) {
		progress("Checking background color...");
		uint32 c = terminal_get_bgcolor();
		if ((int32)c < 0) {
			progress("done\n");
			warnx("Terminal doesn't support contol sequence; "
				"assume --light");
			opt_bgtheme = BG_LIGHT;
		} else {
			float r = (float)((c >> 16) & 0xff) / 255;
			float g = (float)((c >>  8) & 0xff) / 255;
			float b = (float)( c        & 0xff) / 255;
			// グレースケールで、黒に近ければ 0、白に近ければ 1 にする。
			// ここは背景色の明暗だけ分かればいいので細かいことは気にしない。
			float I = (0.2126 * r) + (0.7152 * g) + (0.0722 * b);
			opt_bgtheme = (int)(I + 0.5);
			if (opt_bgtheme == BG_LIGHT) {
				progress("light\n");
			} else {
				progress("dark\n");
			}
		}
	}

	// 端末が SIXEL をサポートしているか。
	if (opt_show_image == -1 && is_tty) {
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
	if (init_codeset(opt_codeset) == false) {
		if (errno == 0) {
			errx(1, "output charset is specified but iconv is not builtin.");
		} else {
			err(1, "iconv_open failed");
		}
	}

	// 色の初期化。
	init_color();

	// キャッシュファイル用の色モード名。
	switch (GET_COLOR_MODE(imageopt.color)) {
	 case COLOR_MODE_GRAY:
	 {
		uint grayscale = GET_COLOR_COUNT(imageopt.color);
		if (grayscale == 2) {
			strlcpy(colorname, "2", sizeof(colorname));
		} else {
			snprintf(colorname, sizeof(colorname), "gray%u", grayscale);
		}
		break;
	 }
	 case COLOR_MODE_8_RGB:
		strlcpy(colorname, "8", sizeof(colorname));
		break;
	 case COLOR_MODE_16_VGA:
		strlcpy(colorname, "16", sizeof(colorname));
		break;
	 case COLOR_MODE_ADAPTIVE:
		strlcpy(colorname, "256", sizeof(colorname));
		break;
	 default:
		// ?
		snprintf(colorname, sizeof(colorname), "RC%d", imageopt.color);
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

	// アイコンは1か月分くらいか。
	snprintf(cmd, sizeof(cmd),
		"find %s -name icon-\\* -type f -atime +30 -exec rm {} +", cachedir);
	if (system(cmd) < 0) {
		warn("system(find icon)");
	}

	// 写真は2日分くらいか。
	snprintf(cmd, sizeof(cmd),
		"find %s -name file\\* -type f -atime +2 -exec rm {} +", cachedir);
	if (system(cmd) < 0) {
		warn("system(find photo)");
	}
}

// filename からトークンを取得して strdup したものを返す。
// 失敗するとその場でエラー終了する。
static const char *
get_token(const char *filename)
{
	char buf[64];
	FILE *fp;
	const char *token;

	assert(filename);

	memset(buf, 0, sizeof(buf));
	fp = fopen(filename, "r");
	if (fp == NULL) {
		err(1, "%s", filename);
	}
	// 1行目だけ。
	if (fgets(buf, sizeof(buf), fp)) {
		chomp(buf);
	}
	fclose(fp);

	if (buf[0] == '\0') {
		errx(1, "%s: No token found", filename);
	}

	token = strdup(buf);
	if (token == NULL) {
		err(1, "%s: strdup", __func__);
	}
	return token;
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
		}
		printf("\n");
		exit(0);

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
	bool is_tty;
	int ws_cols = 0;
	int ws_width = 0;
	int ws_height = 0;
	const char *msg_cols = "";
	const char *msg_width = "";
	const char *msg_height = "";
	int r;

	// 出力先が端末かどうか。
	is_tty = (isatty(STDOUT_FILENO) != 0);

	if (is_tty) {
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
	}

	// 画面幅は常に更新。
	if (ws_cols > 0) {
		screen_cols = ws_cols;
		msg_cols = " (from ioctl)";
	} else {
		screen_cols = 0;
		msg_cols = " (not detected)";
	}

	// フォント幅と高さは指定されてない時だけ取得した値を使う。
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
	if (use_default_font && opt_show_image == 1 && is_tty) {
		printf("sayaka: Fontsize not detected. "
			"Application default %ux%u is used.\n", fontwidth, fontheight);
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

	// そこからインデント幅を決定。
	indent_cols = ((int)(iconsize / fontwidth)) + 1;

	const char *f = (initializing ? "init_screen" : __func__);
	Debug(diag_term, "%s: screen columns=%u%s", f, screen_cols, msg_cols);
	Debug(diag_term, "%s: font height=%u%s", f, fontheight, msg_height);
	Debug(diag_term, "%s: font width=%u%s", f, fontwidth, msg_width);
	Debug(diag_term, "%s: iconsize=%u, indent_columns=%u imagesize=%u",
		f, iconsize, indent_cols, imagesize);
}
