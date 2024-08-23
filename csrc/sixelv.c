/* vi:set ts=4: */
/*
 * Copyright (C) 2015 Y.Sugahara (moveccr)
 * Copyright (C) 2021-2024 Tetsuya Isaki
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
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>

typedef enum {
	OutputFormat_SIXEL,
	OutputFormat_BMP,
} OutputFormat;

// コマンドラインオプション文字列のデコード用
struct optmap {
	const char *name;
	int value;
};

static void version(void);
static void usage(void);
static void help_all(void);
static int parse_opt(const struct optmap *, const char *);
static bool do_file(const char *filename);
static void signal_handler(int);

static diag *diag_image;
static diag *diag_net;
static diag *diag_sixel;
static bool ignore_error;			// true ならエラーでも次ファイルを処理
static FILE *ofp;					// 出力中のストリーム
static ResizeAxis opt_resize_axis;
static const char *output_filename;	// 出力ファイル名。NULL なら stdout
static OutputFormat output_format;	// 出力形式
static image_opt imageopt;
static struct netstream_opt netopt;

enum {
	OPT__start = 0x7f,
	OPT_ciphers,
	OPT_color,
	OPT_debug_image,
	OPT_debug_net,
	OPT_debug_sixel,
	OPT_diffusion,
	OPT_gain,
	OPT_gray,
	OPT_height,
	OPT_help,
	OPT_help_all,
	OPT_ormode,
	OPT_output_format,
	OPT_resize_axis,
	OPT_suppress_palette,
	OPT_version,
	OPT_width,
};

static const struct option longopts[] = {
	{ "ciphers",		required_argument,	NULL,	OPT_ciphers },
	{ "color",			required_argument,	NULL,	'c' },
	{ "debug-image",	required_argument,	NULL,	OPT_debug_image },
	{ "debug-net",		required_argument,	NULL,	OPT_debug_net },
	{ "debug-sixel",	required_argument,	NULL,	OPT_debug_sixel },
	{ "diffusion",		required_argument,	NULL,	OPT_diffusion },
	{ "gain",			required_argument,	NULL,	OPT_gain },
	{ "gray",			required_argument,	NULL,	OPT_gray },
	{ "grey",			required_argument,	NULL,	OPT_gray },
	{ "height",			required_argument,	NULL,	'h' },
	{ "help",			no_argument,		NULL,	OPT_help, },
	{ "help-all",		no_argument,		NULL,	OPT_help_all },
	{ "ignore-error",	no_argument,		NULL,	'i' },
	{ "ormode",			no_argument,		NULL,	OPT_ormode },
	{ "output-format",	required_argument,	NULL,	'O' },
	{ "resize-axis",	required_argument,	NULL,	OPT_resize_axis },
	{ "suppress-palette", no_argument,		NULL,	OPT_suppress_palette },
	{ "version",		no_argument,		NULL,	'v' },
	{ "width",			required_argument,	NULL,	'w' },
	{ NULL },
};

static const struct optmap map_output_format[] = {
	{ "bmp",		OutputFormat_BMP },
	{ "sixel",		OutputFormat_SIXEL },
	{ NULL },
};

static const struct optmap map_diffuse[] = {
	{ "fs",			RDM_FS },
	{ "atkinson",	RDM_ATKINSON },
	{ "jajuni",		RDM_JAJUNI },
	{ "stucki",		RDM_STUCKI },
	{ "burkes",		RDM_BURKES },
	{ "2",			RDM_2 },
	{ "3",			RDM_3 },
	{ "RGB",		RDM_RGB },
	{ NULL },
};

static const struct optmap map_reductor_method[] = {
	{ "none",		ReductorMethod_Simple },
	{ "simple",		ReductorMethod_Simple },
	{ "high",		ReductorMethod_HighQuality },
	{ NULL },
};

static const struct optmap map_resize_axis[] = {
	{ "both",				ResizeAxis_Both },
	{ "width",				ResizeAxis_Width },
	{ "height",				ResizeAxis_Height },
	{ "long",				ResizeAxis_Long },
	{ "short",				ResizeAxis_Short },
	{ "scaledown-both",		ResizeAxis_ScaleDownBoth },
	{ "sdboth",				ResizeAxis_ScaleDownBoth },
	{ "scaledown-width",	ResizeAxis_ScaleDownWidth },
	{ "sdwidth",			ResizeAxis_ScaleDownWidth },
	{ "scaledown-height",	ResizeAxis_ScaleDownHeight },
	{ "sdheight",			ResizeAxis_ScaleDownHeight },
	{ "scaledown-long",		ResizeAxis_ScaleDownLong },
	{ "sdlong",				ResizeAxis_ScaleDownLong },
	{ "scaledown-short",	ResizeAxis_ScaleDownShort },
	{ "sdshort",			ResizeAxis_ScaleDownShort },
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
	netstream_opt_init(&netopt);
	ignore_error = false;
	opt_resize_axis = ResizeAxis_Both;
	output_filename = NULL;
	output_format = OutputFormat_SIXEL;

	while ((c = getopt_long(ac, av, "c:d:h:iO:o:vw:", longopts, NULL)) != -1) {
		switch (c) {
		 case 'c':
		 {
			int num = stou32def(optarg, -1, NULL);
			ReductorColor color;
			switch (num) {
			 case 2:	color = ReductorColor_Gray | (1U << 8);	break;
			 case 8:	color = ReductorColor_Fixed8;	break;
			 case 16:	color = ReductorColor_ANSI16;	break;
			 case 256:	color = ReductorColor_Fixed256;	break;
			 default:
				errx(1, "invalid color mode");
			}
			imageopt.color = color;
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
			imageopt.method = parse_opt(map_reductor_method, optarg);
			if ((int)imageopt.method < 0) {
				errx(1, "invalid reductor method '%s'", optarg);
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

		 case OPT_diffusion:
			imageopt.diffuse = parse_opt(map_diffuse, optarg);
			if ((int)imageopt.diffuse < 0) {
				errx(1, "Invalid diffusion '%s'", optarg);
			}
			break;

		 case OPT_gain:
		 {
			float f = atof(optarg);
			if (f < 0 || f > 2) {
				errx(1, "invalid gain");
			}
			imageopt.gain = (uint)(f * 256);
			break;
		 }

		 case OPT_gray:
		 {
			uint num = stou32def(optarg, 0, NULL);
			if (num < 2 || num > 256) {
				errx(1, "invalid grayscale: %s", optarg);
			}
			imageopt.color = ReductorColor_Gray | ((num - 1) << 8);
			break;
		 }

		 case 'h':
		 {
			imageopt.height = stou32def(optarg, -1, NULL);
			if ((int32)imageopt.height < 0) {
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

		 case 'O':
			output_format = parse_opt(map_output_format, optarg);
			if ((int)output_format < 0) {
				errx(1, "Invalid output format '%s'", optarg);
			}
			break;

		 case OPT_ormode:
			imageopt.output_ormode = true;
			break;

		 case 'o':
			if (strcmp(optarg, "-") == 0) {
				output_filename = NULL;
			} else {
				output_filename = optarg;
			}
			break;

		 case OPT_resize_axis:
			opt_resize_axis = parse_opt(map_resize_axis, optarg);
			if ((int)opt_resize_axis < 0) {
				errx(1, "Invalid resize axis '%s'", optarg);
			}
			break;

		 case OPT_suppress_palette:
			imageopt.suppress_palette = true;
			break;

		 case 'v':
			version();
			exit(0);

		 case 'w':
		 {
			imageopt.width = stou32def(optarg, -1, NULL);
			if ((int32)imageopt.width < 0) {
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

	if (output_format == OutputFormat_SIXEL) {
		signal(SIGINT, signal_handler);
	}

	rv = 0;
	for (int i = 0; i < ac; i++) {
		const char *infilename;
		if (strcmp(av[i], "-") == 0) {
			infilename = NULL;
		} else {
			infilename = av[i];
		}
		if (do_file(infilename) == false && ignore_error == false) {
			rv = 1;
			break;
		}
	}

	netstream_global_cleanup();
	return rv;
}

static void
version(void)
{
	string *info = image_get_loaderinfo();

	printf("%s - SIXEL viewer\n", getprogname());
	printf(" Supported loader: %s\n", string_get(info));

	string_free(info);
}

static void
usage(void)
{
	fprintf(stderr,
		"usage: %s [<options...>] [-|<file|url...>]\n", getprogname());
	fprintf(stderr,
"  -c <color>      : Color mode. 2, 8, 16, 256 (default:256)\n"
"  --gray=<level>  : Grayscale tone from 2 to 256 (default:256)\n"
"  -w <width>      : Resize width to <width> pixel\n"
"  -h <height>     : Resize height to <height> pixel\n"
"  -d <method>     : Reduction method, none(simple) or high (default:high)\n"
"  -O <fmt>        : Output format, bmp or sixel (default: sixel)\n"
"  -o <filename>   : Output filename, '-' means stdout (default: -)\n"
"  --diffusion=<diffusion>               --resize-axis=<axis>\n"
"  --gain=<gain>\n"
"  --ormode                              --suppress-palette\n"
"  -i, --ignore-error                    --ciphers=<ciphers>\n"
"  --help-all                            --debug-image=<0..2>\n"
"  --debug-net=<0..2>                    --debug-sixel=<0..2>\n"
	);
}

static void
help_all(void)
{
	fprintf(stderr,
		"usage: %s [<options...>] [-|<file|url...>]\n", getprogname());
	fprintf(stderr,
"  -c <color> : Specify color mode (default: 256)\n"
"     2   : monochrome (2-level grayscale)\n"
"     8   : Fixed RGB 8 colors\n"
"     16  : Fixed ANSI compatible 16 colors\n"
"     256 : Fixed 256 colors (MSX SCREEN8 compatible palette)\n"
"  --gray=<level> : Specify grayscale tone from 2 to 256 (default:256)\n"
"  -w=<width>,  --width=<width>   : Resize width to <width> pixel.\n"
"  -h=<height>, --height=<height> : Resize height to <height> pixel.\n"
"  --resize-axis=<axis> : Specify an origin axis for resizing. (default:both)\n"
"     both, width, height, long, short, and\n"
"     scaledown-{both,width,height,long,short} or (sd*)\n"
"  -d <method> : Specify reduction method (default: high)\n"
"     none, simple : No diffusion\n"
"     high         : Use 2D Diffusion (default: fs)\n"
"  --diffusion=<diffusion> : Specify diffusion algorithm\n"
"     fs       : Floyd Steinberg\n"
"     atkinson : Atkinson\n"
"     jajuni   : Jarvis, Judice, Ninke\n"
"     stucki   : Stucki\n"
"     burkes   : Burkes\n"
"     2        : 2-pixels (right, down)\n"
"     3        : 3-pixels (right, down, rightdown)\n"
"  --gain=<gain> : Output gain between 0.0 and 2.0 (default:1.0)\n"
"  -O <fmt>, --output-format=<fmt> : bmp or sixel (default: sixel)\n"
"  -o <filename> : Output filename, '-' means stdout (default: -)\n"
"  --ormode    : Output SIXEL by OR-mode\n"
"  --suppress-palette : Suppress output of SIXEL palette definition\n"
"  --ciphers <ciphers> : Only \"RSA\" can be specified for now\n"
"  -i, --ignore-error\n"
"  --debug-image=<0..2>\n"
"  --debug-net  =<0..2>\n"
"  --debug-sixel=<0..2>\n"
"  --help-all  : This help.\n"
	);
	exit(0);
}

// map から arg に対応する値を返す。
// 見付からなければ -1 を返す。
static int
parse_opt(const struct optmap *map, const char *arg)
{
	for (int i = 0; map[i].name; i++) {
		if (strcmp(map[i].name, arg) == 0) {
			return map[i].value;
		}
	}
	return -1;
}

// ファイル1つを表示する。
// infile はファイルパスか NULL なら標準入力。
static bool
do_file(const char *infile)
{
	bool rv = false;
	struct netstream *net = NULL;
	pstream *pstream = NULL;
	image *srcimg = NULL;
	image *resimg = NULL;
	int ifd = -1;
	FILE *ifp = NULL;
	const char *infilename;	// 表示用
	uint dst_width;
	uint dst_height;

	infilename = infile;
	if (infile == NULL) {
		// 標準入力
		ifd = STDIN_FILENO;
		infilename = "stdin";
	} else if (strncmp(infilename, "http://",  7) == 0 ||
	           strncmp(infilename, "https://", 8) == 0)
	{
		// URL
#if defined(HAVE_LIBCURL)
		net = netstream_init(diag_net);
		if (net == NULL) {
			warn("netstream_init() failed");
			return false;
		}
		int code = netstream_connect(net, infile, &netopt);
		if (code < 0) {
			warn("%s: netstream_connect() failed", infilename);
			goto abort;
		} else if (code == 1) {
			warnx("%s: connection failed", infilename);
			goto abort;
		} else if (code >= 400) {
			warnx("%s: connection failed: HTTP %u", infilename, code);
			goto abort;
		}
		ifp = netstream_fopen(net);
		if (ifp == NULL) {
			warn("%s: netstream_fopen() failed", infilename);
			goto abort;
		}
#else
		warnx("%s: Network support has not been compiled", infilename);
		return false;
#endif
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

	// 読み込み。
	srcimg = image_read_pstream(pstream, &imageopt, diag_image);
	if (srcimg == NULL) {
		if (errno == 0) {
			warnx("%s: Unknown image format", infilename);
		} else {
			warn("%s: image_read_pstream() failed", infilename);
		}
		goto abort;
	}

	// いい感じにサイズを決定。
	image_get_preferred_size(srcimg->width, srcimg->height,
		opt_resize_axis, imageopt.width, imageopt.height,
		&dst_width, &dst_height);
	Debug(diag_image, "%s: src size=(%u, %u) dst size=(%u, %u) dst color=%s",
		__func__,
		srcimg->width, srcimg->height, dst_width, dst_height,
		reductorcolor_tostr(imageopt.color));

	// 減色 & リサイズ
	resimg = image_reduct(srcimg, dst_width, dst_height, &imageopt, diag_image);
	if (resimg == NULL) {
		warnx("reductor failed");
		goto abort;
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

	// 書き出し。
	if (output_format == OutputFormat_SIXEL) {
		image_sixel_write(ofp, resimg, &imageopt, diag_sixel);
	} else {
		image *bmpimg = image_coloring(resimg);
		if (bmpimg == NULL) {
			warn("image_coloring(%s) failed", output_filename);
			goto abort;
		}
		bool r = image_bmp_write(ofp, bmpimg, diag_image);
		image_free(bmpimg);
		if (r == false) {
			goto abort;
		}
	}
	fflush(ofp);

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
	if (net) {
		netstream_cleanup(net);
	}
	return rv;
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
