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

#include "common.h"
#include "sixelv.h"
#include "image.h"
#include <err.h>
#include <getopt.h>
#include <string.h>

typedef enum {
	OutputFormat_SIXEL,
	OutputFormat_BMP,
} OutputFormat;

static void version(void);
static void usage(void);
static bool do_file(const char *filename);

static struct diag *diag_image;
static struct diag *diag_sixel;
static bool ignore_error;			// true ならエラーでも次ファイルを処理
static ReductorMethod opt_method;
static ReductorDiffuse opt_diffuse;
static ReductorColor opt_color;
static uint opt_height;
static uint opt_width;
static ResizeAxis opt_resize_axis;
static const char *output_filename;	// 出力ファイル名。NULL なら stdout
static OutputFormat output_format;	// 出力形式

enum {
	OPT__start = 0x7f,
	OPT_color,
	OPT_debug_image,
	OPT_debug_sixel,
	OPT_diffusion,
	OPT_gray,
	OPT_height,
	OPT_help,
	OPT_output_format,
	OPT_resize_axis,
	OPT_version,
	OPT_width,
};

static const struct option longopts[] = {
	{ "color",			required_argument,	NULL,	'c' },
	{ "debug-image",	required_argument,	NULL,	OPT_debug_image },
	{ "debug-sixel",	required_argument,	NULL,	OPT_debug_sixel },
	{ "diffusion",		required_argument,	NULL,	OPT_diffusion },
	{ "gray",			required_argument,	NULL,	OPT_gray },
	{ "grey",			required_argument,	NULL,	OPT_gray },
	{ "height",			required_argument,	NULL,	'h' },
	{ "help",			no_argument,		NULL,	OPT_help, },
	{ "ignore-error",	no_argument,		NULL,	'i' },
	{ "output-format",	required_argument,	NULL,	'O' },
	{ "resize-axis",	required_argument,	NULL,	OPT_resize_axis },
	{ "version",		no_argument,		NULL,	'v' },
	{ "width",			required_argument,	NULL,	'w' },
};

int
main(int ac, char *av[])
{
	int c;

	diag_image = diag_alloc();
	diag_sixel = diag_alloc();

	ignore_error = false;
	opt_method = ReductorMethod_HighQuality;
	opt_diffuse = RDM_FS;
	opt_color = ReductorColor_Fixed256;
	opt_resize_axis = ResizeAxis_Both;
	opt_height = 0;
	opt_width = 0;
	output_filename = NULL;
	output_format = OutputFormat_SIXEL;

	while ((c = getopt_long(ac, av, "c:d:h:iO:o:vw:", longopts, NULL)) != -1) {
		switch (c) {
		 case 'c':
		 {
			int num = atoi(optarg);
			switch (num) {
			 case 2:	opt_color = ReductorColor_Mono;		break;
			 case 8:	opt_color = ReductorColor_Fixed8;	break;
			 case 16:	opt_color = ReductorColor_ANSI16;	break;
			 case 256:	opt_color = ReductorColor_Fixed256;	break;
			 default:
				errx(1, "invalid color mode");
			}
			break;
		 }

		 case 'd':
			if (strcmp(optarg, "none") == 0) {
				opt_method = ReductorMethod_Simple;
			} else if (strcmp(optarg, "high") == 0) {
				opt_method = ReductorMethod_HighQuality;
			} else {
				errx(1, "invalid reductor method");
			}
			break;

		 case OPT_debug_image:
			diag_set_level(diag_image, atoi(optarg));
			break;

		 case OPT_debug_sixel:
			diag_set_level(diag_sixel, atoi(optarg));
			break;

		 case OPT_gray:
		 {
			uint num = atoi(optarg);
			if (num < 2 || num > 256) {
				errx(1, "invalid grayscale");
			}
			opt_color = ReductorColor_Gray | ((num - 1) << 8);
			break;
		 }

		 case 'h':
			opt_height = atoi(optarg);
			break;

		 case 'i':
			ignore_error = true;
			break;

		 case 'O':
			if (strcmp(optarg, "sixel") == 0) {
				output_format = OutputFormat_SIXEL;
			} else if (strcmp(optarg, "bmp") == 0) {
				output_format = OutputFormat_BMP;
			} else {
				errx(1, "Invalid output format type");
			}
			break;

		 case 'o':
			if (strcmp(optarg, "-") == 0) {
				output_filename = NULL;
			} else {
				output_filename = optarg;
			}
			break;

		 case OPT_resize_axis:
			//opt_resize_axis = ;

		 case 'v':
			version();

		 case 'w':
			opt_width = atoi(optarg);
			break;

		 default:
			usage();
		}
	}
	ac -= optind;
	av += optind;

	if (ac == 0) {
		usage();
	}

	if (output_filename != NULL && ac > 1) {
		errx(1,
			"-o <output_filename> cannot be used with multiple input file.");
	}

	for (int i = 0; i < ac; i++) {
		if (do_file(av[i]) == false && ignore_error == false) {
			return 1;
		}
	}

	return 0;
}

static void
version(void)
{
	string *info = image_get_decoderinfo();

	printf("%s - SIXEL viewer\n", getprogname());
	printf(" Supported loader: %s\n", string_get(info));

	string_free(info);
	exit(0);
}

static void
usage(void)
{
	fprintf(stderr,
		"usage: %s [<options...>] [-|<files...>]\n", getprogname());
}

// ファイル1つを表示する。
// infilename はファイルパスか "-"(標準入力)。
static bool
do_file(const char *infilename)
{
	bool rv = false;
	FILE *ifp = NULL;
	FILE *ofp = NULL;
	uint dst_width;
	uint dst_height;

	if (strcmp(infilename, "-") == 0) {
		infilename = "stdin";

		ifp = fdstream_open(STDIN_FILENO);
		if (ifp == NULL) {
			warn("fopen_fd(%s) failed", infilename);
			return false;
		}
	} else {
		ifp = fopen(infilename, "r");
		if (ifp == NULL) {
			warn("fopen(%s) failed", infilename);
			return false;
		}
	}

	// 読み込み。
	struct image *srcimg = image_create_fp(ifp, diag_image);
	if (srcimg == NULL) {
		warn("image_create_fp(%s) failed", infilename);
		goto abort1;
	}

	// いい感じにサイズを決定。
	image_get_preferred_size(srcimg->width, srcimg->height,
		opt_resize_axis, opt_width, opt_height,
		&dst_width, &dst_height);
	Debug(diag_image, "%s: src size=(%u, %u) dst size=(%u, %u) dst color=%s",
		__func__,
		srcimg->width, srcimg->height, dst_width, dst_height,
		reductorcolor_tostr(opt_color));

	// 減色 & リサイズ
	struct image_reduct_param param;
	memset(&param, 0, sizeof(param));
	param.method = opt_method;
	param.diffuse = RDM_FS;
	param.color = opt_color;
	struct image *resimg = image_reduct(srcimg, dst_width, dst_height,
		&param, diag_image);
	if (resimg == NULL) {
		warnx("reductor failed");
		goto abort2;
	}

	// 出力先をオープン。
	if (output_filename == NULL) {
		ofp = stdout;
	} else {
		ofp = fopen(output_filename, "w");
		if (ofp == NULL) {
			warn("fopen(%s) failed", output_filename);
			goto abort3;
		}
	}

	// 書き出し。
	if (output_format == OutputFormat_SIXEL) {
		struct sixel_opt opt;
		memset(&opt, 0, sizeof(opt));
		image_sixel_write(ofp, resimg, &opt, diag_sixel);
	} else {
		struct image *bmpimg = image_coloring(resimg);
		if (bmpimg == NULL) {
			warn("image_coloring(%s) failed", output_filename);
			goto abort4;
		}
		bool r = image_bmp_write(ofp, bmpimg, diag_image);
		image_free(bmpimg);
		if (r == false) {
			goto abort4;
		}
	}
	fflush(ofp);

	rv = true;
 abort4:
	if (output_filename != NULL) {
		fclose(ofp);
	}
 abort3:
	image_free(resimg);
 abort2:
	image_free(srcimg);
 abort1:
	// stdin ならクローズしないの処理は fdstream_close() で行っている。
	fclose(ifp);
	return rv;
}
