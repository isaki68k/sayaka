/* vi:set ts=4: */
/*
 * Copyright (C) 2015 Y.Sugahara (moveccr)
 * Copyright (C) 2021-2025 Tetsuya Isaki
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
// sixelv - SIXEL 画像コンバータ
//

#include "sixelv.h"
#include "image.h"
#include <err.h>
#include <fcntl.h>
#include <getopt.h>
#include <signal.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/ioctl.h>

#define SIXELV_VERSION "3.8.3"
#define SIXELV_RELDATE "2025/06/29"

typedef enum {
	OUTPUT_FORMAT_SIXEL,
	OUTPUT_FORMAT_BMP,
	OUTPUT_FORMAT_ASCII,
} OutputFormat;

static void version(void);
static void list(void);
static void usage(void);
static void help_all(void);
static bool do_file(const char *filename);
static struct image *read_blurhash(struct pstream *, uint *, uint *);
static void signal_handler(int);

static struct diag *diag_image;
static struct diag *diag_net;
static struct diag *diag_sixel;
static uint fontwidth;				// フォント幅 (ドット数)
static uint fontheight;				// フォント高さ (ドット数)
static bool show_filename;			// 画像の前にファイル名を表示
static bool ignore_error;			// true ならエラーでも次ファイルを処理
static FILE *ofp;					// 出力中のストリーム
static bool opt_blurhash_nearest;	// Blurhash を最近傍補間する
static ResizeAxis opt_resize_axis;
static uint opt_width;
static uint opt_height;
static int  opt_page;
static bool opt_profile;			// プロファイル
static const char *output_filename;	// 出力ファイル名。NULL なら stdout
static OutputFormat output_format;	// 出力形式
static struct image_opt imageopt;
static struct net_opt netopt;

#define PROF(tv)	do {					\
	if (opt_profile)						\
		clock_gettime(CLOCK_MONOTONIC, tv);	\
} while (0)

const char progname[] = "sixelv";
const char progver[]  = SIXELV_VERSION;

enum {
	OPT__start = 0x7f,
	OPT_blurhash_nearest,
	OPT_cdm,
	OPT_ciphers,
	OPT_debug_image,
	OPT_debug_net,
	OPT_debug_sixel,
	OPT_gain,
	OPT_height,
	OPT_help,
	OPT_help_all,
	OPT_ipv4,
	OPT_ipv6,
	OPT_list,
	OPT_output_format,
	OPT_profile,
	OPT_resize_axis,
	OPT_sixel_or,
	OPT_sixel_transbg,
	OPT_suppress_palette,
	OPT_version,
	OPT_width,
};

static const struct option longopts[] = {
	{ "blurhash-nearest",no_argument,		NULL,	OPT_blurhash_nearest },
	{ "bn",				no_argument,		NULL,	OPT_blurhash_nearest },
	{ "ciphers",		required_argument,	NULL,	OPT_ciphers },
	{ "color",			required_argument,	NULL,	'c' },
	{ "cdm",			required_argument,	NULL,	OPT_cdm },
	{ "debug-image",	required_argument,	NULL,	OPT_debug_image },
	{ "debug-net",		required_argument,	NULL,	OPT_debug_net },
	{ "debug-sixel",	required_argument,	NULL,	OPT_debug_sixel },
	{ "diffusion",		required_argument,	NULL,	'd' },
	{ "gain",			required_argument,	NULL,	OPT_gain },
	{ "height",			required_argument,	NULL,	'h' },
	{ "help",			no_argument,		NULL,	OPT_help, },
	{ "help-all",		no_argument,		NULL,	OPT_help_all },
	{ "ignore-error",	no_argument,		NULL,	'i' },
	{ "ipv4",			no_argument,		NULL,	OPT_ipv4 },
	{ "ipv6",			no_argument,		NULL,	OPT_ipv6 },
	{ "list",			no_argument,		NULL,	OPT_list },
	{ "output-format",	required_argument,	NULL,	'O' },
	{ "page",			required_argument,	NULL,	'p' },
	{ "profile",		no_argument,		NULL,	OPT_profile },
	{ "reduction",		required_argument,	NULL,	'r' },
	{ "resize-axis",	required_argument,	NULL,	OPT_resize_axis },
	{ "sixel-or",		no_argument,		NULL,	OPT_sixel_or },
	{ "sixel-transbg",	no_argument,		NULL,	OPT_sixel_transbg },
	{ "suppress-palette", no_argument,		NULL,	OPT_suppress_palette },
	{ "version",		no_argument,		NULL,	OPT_version },
	{ "width",			required_argument,	NULL,	'w' },
	{ NULL },
};

static const struct optmap map_output_format[] = {
	{ "ascii",		OUTPUT_FORMAT_ASCII },
	{ "bmp",		OUTPUT_FORMAT_BMP },
	{ "sixel",		OUTPUT_FORMAT_SIXEL },
	{ NULL },
};

static const struct optmap map_diffuse[] = {
	{ "none",		DIFFUSE_NONE },
	{ "sfl",		DIFFUSE_SFL },
	{ "fs",			DIFFUSE_FS },
	{ "atkinson",	DIFFUSE_ATKINSON },
	{ "jajuni",		DIFFUSE_JAJUNI },
	{ "stucki",		DIFFUSE_STUCKI },
	{ "burkes",		DIFFUSE_BURKES },
	{ "2",			DIFFUSE_2 },
	{ "3",			DIFFUSE_3 },
	{ "rgb",		DIFFUSE_RGB },
	{ NULL },
};

static const struct optmap map_reductor_method[] = {
	{ "none",		REDUCT_SIMPLE },
	{ "simple",		REDUCT_SIMPLE },
	{ "high",		REDUCT_HIGH_QUALITY },
	{ NULL },
};

static const struct optmap map_resize_axis[] = {
	{ "both",				RESIZE_AXIS_BOTH },
	{ "width",				RESIZE_AXIS_WIDTH },
	{ "height",				RESIZE_AXIS_HEIGHT },
	{ "long",				RESIZE_AXIS_LONG },
	{ "short",				RESIZE_AXIS_SHORT },
	{ "scaledown-both",		RESIZE_AXIS_SCALEDOWN_BOTH },
	{ "sdboth",				RESIZE_AXIS_SCALEDOWN_BOTH },
	{ "scaledown-width",	RESIZE_AXIS_SCALEDOWN_WIDTH },
	{ "sdwidth",			RESIZE_AXIS_SCALEDOWN_WIDTH },
	{ "scaledown-height",	RESIZE_AXIS_SCALEDOWN_HEIGHT },
	{ "sdheight",			RESIZE_AXIS_SCALEDOWN_HEIGHT },
	{ "scaledown-long",		RESIZE_AXIS_SCALEDOWN_LONG },
	{ "sdlong",				RESIZE_AXIS_SCALEDOWN_LONG },
	{ "scaledown-short",	RESIZE_AXIS_SCALEDOWN_SHORT },
	{ "sdshort",			RESIZE_AXIS_SCALEDOWN_SHORT },
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
	int rv;

	diag_image = diag_alloc();
	diag_net   = diag_alloc();
	diag_sixel = diag_alloc();
	diag_set_timestamp(diag_net, true);

	image_opt_init(&imageopt);
	net_opt_init(&netopt);
	ignore_error = false;
	opt_resize_axis = RESIZE_AXIS_BOTH;
	output_filename = NULL;
	output_format = OUTPUT_FORMAT_SIXEL;

	while ((c = getopt_long(ac, av, "c:d:h:iO:o:p:r:vw:",
					longopts, NULL)) != -1)
	{
		switch (c) {
		 case OPT_blurhash_nearest:
			opt_blurhash_nearest = true;
			break;

		 case 'c':
			imageopt.color = image_parse_color(optarg);
			if (imageopt.color == COLOR_MODE_NONE) {
				errx(1, "%s: invalid color mode", optarg);
			}
			break;

		 case OPT_cdm:
		 {
			float f = atof(optarg);
			if (f < 0 || f > 1) {
				errx(1, "Invalid value: %s", optarg);
			}
			imageopt.cdm = (uint)(f * 256);
			break;
		 }

		 case OPT_ciphers:
			// 今のところ "RSA" (大文字) しか指定できない。
			if (strcmp(optarg, "RSA") == 0) {
				netopt.use_rsa_only = true;
			} else {
				errx(1, "Invalid ciphers: '%s'", optarg);
			}
			break;

		 case 'd':
			imageopt.diffuse = parse_optmap(map_diffuse, optarg);
			if ((int)imageopt.diffuse < 0) {
				errx(1, "Invalid diffusion '%s'", optarg);
			}
			break;

		 case OPT_debug_image:
			SET_DIAG_LEVEL(diag_image);
			break;

		 case OPT_debug_net:
			SET_DIAG_LEVEL(diag_net);
			break;

		 case OPT_debug_sixel:
			SET_DIAG_LEVEL(diag_sixel);
			break;

		 case OPT_gain:
		 {
			float f = atof(optarg);
			if (f < 0 || f > 2) {
				errx(1, "invalid gain");
			}
			imageopt.gain = (int)(f * 256);
			if (imageopt.gain == 256) {
				imageopt.gain = -1;
			}
			break;
		 }

		 case 'h':
		 {
			opt_height = stou32def(optarg, 0, NULL);
			if (opt_height == 0) {
				errx(1, "invalid height: %s", optarg);
			}
			break;
		 }

		 case OPT_help:
			usage();
			exit(0);

		 case OPT_help_all:
			help_all();
			exit(0);

		 case 'i':
			ignore_error = true;
			break;

		 case OPT_ipv4:
			netopt.address_family = 4;
			break;

		 case OPT_ipv6:
			netopt.address_family = 6;
			break;

		 case OPT_list:
			list();
			exit(0);

		 case 'O':
			output_format = parse_optmap(map_output_format, optarg);
			if ((int)output_format < 0) {
				errx(1, "Invalid output format '%s'", optarg);
			}
			break;

		 case 'o':
			if (strcmp(optarg, "-") == 0) {
				output_filename = NULL;
			} else {
				output_filename = optarg;
			}
			break;

		 case 'p':
			opt_page = stou32def(optarg, -1, NULL);
			if (opt_page == -1) {
				errx(1, "invalid page: %s", optarg);
			}
			break;

		 case OPT_profile:
			opt_profile = true;
			break;

		 case 'r':
			imageopt.method = parse_optmap(map_reductor_method, optarg);
			if ((int)imageopt.method < 0) {
				errx(1, "invalid reductor method '%s'", optarg);
			}
			break;

		 case OPT_resize_axis:
			opt_resize_axis = parse_optmap(map_resize_axis, optarg);
			if ((int)opt_resize_axis < 0) {
				errx(1, "Invalid resize axis '%s'", optarg);
			}
			break;

		 case OPT_sixel_or:
			imageopt.output_ormode = true;
			break;

		 case OPT_sixel_transbg:
			imageopt.output_transbg = true;
			break;

		 case OPT_suppress_palette:
			imageopt.suppress_palette = true;
			break;

		 case 'v':
			show_filename = true;
			break;

		 case OPT_version:
			version();
			exit(0);

		 case 'w':
		 {
			opt_width = stou32def(optarg, 0, NULL);
			if (opt_width == 0) {
				errx(1, "invalid width: %s", optarg);
			}
			break;
		 }

		 default:
			usage();
			exit(0);
		}
	}
	ac -= optind;
	av += optind;

	if (ac == 0) {
		usage();
		exit(0);
	}

	if (output_filename != NULL && ac > 1) {
		errx(1,
			"-o <output_filename> cannot be used with multiple input file.");
	}

	// 出力形式が ASCII でフォントサイズ未指定ならここでフォントサイズを取得。
	// 出力先が端末でなければ適当な値を代入する。
	if (output_format == OUTPUT_FORMAT_ASCII &&
			(fontwidth == 0 || fontheight == 0))
	{
		struct winsize ws;
		int r = ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws);
		if (r == 0) {
			if (ws.ws_col != 0) {
				fontwidth = ws.ws_xpixel / ws.ws_col;
			}
			if (ws.ws_row != 0) {
				fontheight = ws.ws_ypixel / ws.ws_row;
			}
		}

		// 取得できなければ適当な値を入れておく。
		if (fontwidth == 0) {
			fontwidth = 7;
		}
		if (fontheight == 0) {
			fontheight = 14;
		}
	}

	signal(SIGPIPE, SIG_IGN);
	if (output_format == OUTPUT_FORMAT_SIXEL) {
		signal(SIGINT, signal_handler);
	}

	rv = 0;
	for (int i = 0; i < ac; i++) {
		const char *infilename;
		if (strcmp(av[i], "-") == 0) {
			infilename = NULL;
		} else {
			infilename = av[i];
			if (show_filename) {
				printf("%s\n", infilename);
			}
		}
		if (do_file(infilename) == false && ignore_error == false) {
			rv = 1;
			break;
		}
	}

	return rv;
}

static void
version(void)
{
	printf("%s %s (%s) - SIXEL viewer\n", progname, progver, SIXELV_RELDATE);
}

static void
list(void)
{
	char **names = image_get_loaderinfo();
	for (uint i = 0; names[i] != NULL; i += 2) {
		printf("%-8s %s\n", names[i], names[i + 1]);
	}
	free(names);
}

static void
usage(void)
{
	fprintf(stderr,
		"usage: %s [<options...>] [-|<file|url...>]\n", progname);
	fprintf(stderr,
"  -c <color>      : Color mode. 2, 8, 16, 256, gray[2..256] (default:256)\n"
"                    (See --help-all for more details)\n"
"  -w <width>      : Resize width to <width> pixel\n"
"  -h <height>     : Resize height to <height> pixel\n"
"  -r <method>     : Reduction method, none(simple) or high (default:high)\n"
"  -O <fmt>        : Output format, ascii, bmp or sixel (default: sixel)\n"
"  -o <filename>   : Output filename, '-' means stdout (default: -)\n"
"  -p <page>       : Specify the page(frame). (animated GIF/WebP only)\n"
"  -v              : Show input filename\n");

	static const char * const opts[] = {
		"-d <diffusion>",
		"--resize-axis=<axis>",
		"--gain=<gain>",
		"--blurhash-nearest",
		"--sixel-or",
		"--sixel-transbg",
		"--suppress-palette",
		"--ignore-error",
		"--ciphers=<ciphers>",
		"--help-all",
		"--debug-image=<0..2>",
		"--debug-net=<0..2>",
		"--debug-sixel=<0..2>",
		"", // 偶数パディング
	};
	for (uint i = 0; i < countof(opts) / 2; i++) {
		fprintf(stderr, "  %-39s%s\n", opts[i * 2], opts[i * 2 + 1]);
	}
}

static void
help_all(void)
{
	fprintf(stderr,
		"usage: %s [<options...>] [-|<file|url...>]\n", progname);
	fprintf(stderr,
"  -c,--color=<colormode> : Set color mode (default:256)\n"
"     256          : A synonym for 'adaptive256'\n"
"     adaptive[<n>]: Adaptive palette (n=8..256). If <n> is omitted, 256 is used\n"
"     fixed256     : Fixed 256 colors (MSX SCREEN8 compatible palette)\n"
"     xterm256     : Fixed 256 colors (xterm compatible palette)\n"
"     16           : Fixed ANSI compatible 16 colors\n"
"     8            : Fixed RGB 8 colors\n"
"     2            : Monochrome (2-level grayscale)\n"
"     gray[<n>]    : (2..256) shades of grayscale. 'gray2' is a synonym for '2'.\n"
"                    If <n> is omitted, 256 is used\n"
"  -w,--width=<width>     : Resize width to <width> pixel\n"
"  -h,--height=<height>   : Resize height to <height> pixel\n"
"  --resize-axis=<axis>   : Set an origin axis for resizing (default:both)\n"
"     both, width, height, long, short, and\n"
"     scaledown-{both,width,height,long,short} or (sd*)\n"
"  -r,--reduction=<method>: Set reduction method (default:high)\n"
"     none, simple: No diffusion\n"
"     high        : Use 2D Diffusion (with diffusion default:sfl)\n"
"  -d,--diffusion=<diffusion> : Set diffusion algorithm\n"
"     sfl      : Sierra Filter Lite\n"
"     fs       : Floyd Steinberg\n"
"     atkinson : Atkinson\n"
"     jajuni   : Jarvis, Judice, Ninke\n"
"     stucki   : Stucki\n"
"     burkes   : Burkes\n"
"     2        : 2-pixels (right, down)\n"
"     3        : 3-pixels (right, down, rightdown)\n"
"     none     : No diffution\n"
"  --bn,--blurhash-nearest\n"
"  --cdm=<value>          : Differential Color Diffusion Attenuator,\n"
"                           between 0.0 and 1.0 (default:1.0)\n"
"  --gain=<gain>          : Set output gain between 0.0 and 2.0 (default:1.0)\n"
"  --help-all             : This help\n"
"  --list                 : Show supported filetype and decoder list\n"
"  -O,--output-format=<fmt> : ascii, bmp or sixel (default:sixel)\n"
"  -o <filename>          : Output filename, '-' means stdout (default:-)\n"
"  -p,--page=<page>       : Specify the page(frame). (animated GIF/WebP only)\n"
"  --sixel-or             : Output SIXEL by OR-mode\n"
"  --sixel-transbg        : Make SIXEL background transparent\n"
"  --suppress-palette     : Suppress output of SIXEL palette definition\n"
"  --ciphers <ciphers>    : \"RSA\" can only be specified\n"
"  --ipv4 / --ipv6        : Connect only IPv4/v6\n"
"  -i,--ignore-error\n"
"  -v                     : Show input filename\n"
"  --version\n"
"  --debug-image=<0..2>\n"
"  --debug-net  =<0..2>\n"
"  --debug-sixel=<0..2>\n"
	);
	exit(0);
}

// ファイル1つを表示する。
// infile はファイルパスか NULL なら標準入力。
static bool
do_file(const char *infile)
{
	bool rv = false;
	struct httpclient *http = NULL;
	struct pstream *pstream = NULL;
	struct image *srcimg = NULL;
	struct image *resimg = NULL;
	int ifd = -1;
	FILE *ifp = NULL;
	const char *infilename;	// 表示用
	image_read_hint hint;
	uint dst_width;
	uint dst_height;
	struct timespec load_start;
	struct timespec load_end;
	struct timespec cvt_start;
	struct timespec reduct_start;
	struct timespec reduct_end;
	struct timespec sixel_start;
	struct timespec sixel_end;

	infilename = infile;
	if (infile == NULL) {
		// 標準入力
		ifd = STDIN_FILENO;
		infilename = "stdin";
	} else if (strncmp(infilename, "http://",  7) == 0 ||
	           strncmp(infilename, "https://", 8) == 0)
	{
		// URL
		http = httpclient_create(diag_net);
		if (http == NULL) {
			warn("httpclient_create() failed");
			return false;
		}
		int code = httpclient_connect(http, infile, &netopt);
		if (code == -2) {
			warnx("%s: SSL not compiled", infilename);
			goto abort;
		} else if (code < 0) {
			warn("%s: connection failed", infilename);
			goto abort;
		} else if (code >= 400) {
			warnx("%s: connection failed: HTTP %u %s", infilename,
				code, httpclient_get_resmsg(http));
			goto abort;
		}
		ifp = httpclient_fopen(http);
		if (ifp == NULL) {
			warn("%s: httpclient_fopen() failed", infilename);
			goto abort;
		}
	} else {
		// ファイル名
		ifd = open(infile, O_RDONLY);
		if (ifd < 0) {
			warn("%s", infilename);
			return false;
		}
	}

	// ifp or ifd からピークストリームを作成。
	if (ifp) {
		pstream = pstream_init_fp(ifp);
		if (pstream == NULL) {
			warn("%s: pstream_init_fp() failed", infilename);
			goto abort;
		}
	} else {
		pstream = pstream_init_fd(ifd);
		if (pstream == NULL) {
			warn("%s: pstream_init_fd() failed", infilename);
			goto abort;
		}
	}

	PROF(&load_start);

	// 画像形式判定。
	int loader_idx = image_match(pstream, diag_image);

	if (loader_idx >= 0) {
		// 読み込み。
		// この hint は libjpeg の scaling hint のことで、これをもとに
		// 1/8 とかで読み込んだものが srcimg->{width,height} になる。
		// ASCII の時には hint サイズも加工したほうが効率はいいがとりあえず。
		memset(&hint, 0, sizeof(hint));
		hint.axis   = opt_resize_axis;
		hint.width  = opt_width;
		hint.height = opt_height;
		hint.page   = opt_page;
		srcimg = image_read(pstream, loader_idx, &hint, diag_image);
		if (srcimg) {
			// 得られた画像サイズと引数指定から、いい感じにサイズを決定。
			image_get_preferred_size(srcimg->width, srcimg->height,
				opt_resize_axis, opt_width, opt_height,
				&dst_width, &dst_height);
		}
	} else {
		// どの画像形式でもなさそうなら Blurhash を試す。
		srcimg = read_blurhash(pstream, &dst_width, &dst_height);
	}

	PROF(&load_end);

	if (srcimg == NULL) {
		warnx("%s: Unknown image format", infilename);
		goto abort;
	}

	if (output_format == OUTPUT_FORMAT_ASCII) {
		// ここでピクセルサイズを桁数行数に変更。
		dst_width  = howmany(dst_width, fontwidth);
		dst_height = howmany(dst_height, fontheight);
	}

	Debug(diag_image,
		"InputSize=(%u, %u) OutputSize=(%u, %u) OutputColor=%s",
		srcimg->width, srcimg->height, dst_width, dst_height,
		colormode_tostr(imageopt.color));

	if (dst_width == 0 || dst_height == 0) {
		warnx("%s: Output size (%u, %u) is too small",
			infilename, dst_width, dst_height);
		goto abort;
	}

	PROF(&cvt_start);
	image_convert_to16(srcimg);
	PROF(&reduct_start);

	// 減色 & リサイズ。
	resimg = image_reduct(srcimg, dst_width, dst_height, &imageopt, diag_image);
	if (resimg == NULL) {
		warnx("reductor failed");
		goto abort;
	}

	PROF(&reduct_end);

	if (GET_COLOR_MODE(imageopt.color) == COLOR_MODE_ADAPTIVE) {
		Debug(diag_image, "AdaptivePalette InputColors=%u%s OutputColors=%u",
			srcimg->palette_count,
			(srcimg->palette_count > 256 ? "/32768" : ""),
			resimg->palette_count);
	}

	// 出力先をオープン。
	if (output_filename == NULL) {
		ofp = stdout;
	} else {
		ofp = fopen(output_filename, "w");
		if (ofp == NULL) {
			warn("fopen(%s) failed", output_filename);
			goto abort;
		}
	}

	PROF(&sixel_start);

	// 書き出し。
	switch (output_format) {
	 case OUTPUT_FORMAT_SIXEL:
		image_sixel_write(ofp, resimg, &imageopt, diag_sixel);
		break;
	 case OUTPUT_FORMAT_BMP:
		if (image_bmp_write(ofp, resimg, diag_image) == false) {
			goto abort;
		}
		break;
	 case OUTPUT_FORMAT_ASCII:
		if (image_ascii_write(ofp, resimg, &imageopt, diag_image) == false) {
			goto abort;
		}
		break;
	}
	fflush(ofp);

	PROF(&sixel_end);

	if (opt_profile) {
		uint64 load_usec;
		uint64 cvt_usec;
		uint64 reduct_usec;
		uint64 sixel_usec;
		load_usec = timespec_to_usec(&load_end) - timespec_to_usec(&load_start);
		cvt_usec = timespec_to_usec(&reduct_end) - timespec_to_usec(&cvt_start);
		reduct_usec = timespec_to_usec(&reduct_end)
											- timespec_to_usec(&reduct_start);
		sixel_usec = timespec_to_usec(&sixel_end)
											- timespec_to_usec(&sixel_start);
		float ltime = (float)load_usec   / 1000;
		float ctime = (float)cvt_usec    / 1000;
		float rtime = (float)reduct_usec / 1000;
		float stime = (float)sixel_usec  / 1000;
		diag_print(diag_image,
			"Load(+IO) %4.1f, Cvt %4.1f, Reduct %4.1f, %s(+IO) %4.1f msec",
			ltime, ctime, rtime,
			(output_format == OUTPUT_FORMAT_SIXEL ? "SIXEL" : "Write"),
			stime);
	}

	rv = true;
 abort:
	if (output_filename != NULL) {
		if (ofp) {
			fclose(ofp);
		}
	}
	ofp = NULL;

	image_free(resimg);
	image_free(srcimg);

	if (pstream) {
		pstream_cleanup(pstream);
	}
	if (ifp) {
		fclose(ifp);
	}
	if (ifd >= 3) {
		close(ifd);
	}
	if (http) {
		httpclient_destroy(http);
	}
	return rv;
}

// pstream から Blurhash 画像を読み込んで返す。
// その際画像とコマンドラインオプションから求めた表示画像サイズを
// dst_width, dst_height に返す。
static struct image *
read_blurhash(struct pstream *pstream, uint *dst_width, uint *dst_height)
{
	FILE *fp;
	struct image *srcimg;
	int bw;
	int bh;
	uint width;
	uint height;

	// 先に生成する画像サイズを決定してローダに渡す必要がある。
	// -w,-h	--bn
	// なし		なし		: 20倍で生成、等倍にリサイズ。
	// あり		なし		: WxH で生成、等倍にリサイズ。
	// なし		あり		: 1倍で生成、 20倍にリサイズ。
	// あり		あり		: 1倍で生成、 WxH にリサイズ。

	if (opt_blurhash_nearest) {
		bw = -1;
		bh = -1;
	} else {
		if (opt_width == 0 && opt_height == 0) {
			// -w, -h ともに指定されなければ勝手に縦横 20倍とする。
			bw = -20;
			bh = -20;
		} else if (opt_width > 0 && opt_height > 0) {
			// 両方指定されたらそのサイズ。
			bw = opt_width;
			bh = opt_height;
		} else {
			// -w か -h 片方しか指定されなかった場合、どのみち
			// オリジナルのアスペクト比も不明なので 1:1 とするしかない。
			if (opt_width > 0) {
				bw = opt_width;
			} else {
				bw = opt_height;
			}
			bh = bw;
		}
	}

	srcimg = NULL;

	fp = pstream_open_for_read(pstream);
	if (fp == NULL) {
		return NULL;
	}

	srcimg = image_blurhash_read(fp, bw, bh, diag_image);
	fclose(fp);

	if (srcimg) {
		// リサイズ後のサイズを決定。
		if (opt_blurhash_nearest) {
			if (opt_width == 0 && opt_height == 0) {
				width  = srcimg->width * 20;
				height = srcimg->height * 20;
			} else if (opt_width > 0 && opt_height > 0) {
				width  = opt_width;
				height = opt_height;
			} else {
				if (opt_width > 0) {
					width = opt_width;
				} else {
					width = opt_height;
				}
				height = width;
			}
		} else {
			// 等倍にリサイズ。
			width  = srcimg->width;
			height = srcimg->height;
		}
		*dst_width  = width;
		*dst_height = height;
	}
	return srcimg;
}

static void
signal_handler(int signo)
{
	switch (signo) {
	 case SIGINT:
		if (ofp) {
			image_sixel_abort(ofp);
		}
		exit(0);

	 default:
		break;
	}
}
