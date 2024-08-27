/* vi:set ts=4: */
/*
 * Copyright (C) 2023-2024 Tetsuya Isaki
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
// Blurhash 読み込み
//

#if !defined(GEN_L2SRGB) && !defined(GEN_BASE83)

#include "common.h"
#include "image_priv.h"
#include <string.h>
#include <math.h>

#define BUFSIZE	(256)	// どうする?

#define L2SRGBSIZE	(64)

struct colorf {
	float r;
	float g;
	float b;
};

static uint decode83(const char *, uint, uint);
static void decode_dc(struct colorf *, uint);
static float decode_acq(uint);
static float decode_maxac(int);
static float srgb2linear(int);
static uint linear2srgb(float);
static float *bases_for(uint, uint);

static const uint8 table_L2SRGB[L2SRGBSIZE];
static const uint8 table_base83[0x60];

bool
image_blurhash_match(FILE *fp, const diag *diag)
{
	// 長さが必要なので全部読むしかない。
	char src[BUFSIZE];

	if (fgets(src, sizeof(src), fp) == NULL) {
		return NULL;
	}
	chomp(src);

	uint comp = decode83(src, 0, 1);
	uint compx = (comp % 9) + 1;
	uint compy = (comp / 9) + 1;
	bool ok = (strlen(src) == compx * compy * 2 + 4);
	if (ok) {
		Debug(diag, "%s: looks OK", __func__);
	}
	return ok;
}

image *
image_blurhash_read(FILE *fp, const image_opt *opt, const diag *diag)
{
	char src[BUFSIZE];
	float maxvalue;
	struct colorf *v;
	image *img = NULL;
	struct colorf *values = NULL;
	float *bases_x = NULL;
	float *bases_y = NULL;
	bool success = false;

	if (fgets(src, sizeof(src), fp) == NULL) {
		return NULL;
	}
	chomp(src);

	uint comp = decode83(src, 0, 1);
	uint compx = (comp % 9) + 1;
	uint compy = (comp / 9) + 1;

	// デフォルトでは適当に 20倍の大きさとする。
	// comp[x,y] が 1..9 なので 20 〜 180 px。
	// opt で指定することも可能。
	uint width;
	uint height;
	image_get_preferred_size(20 * compx, 20 * compy,
		ResizeAxis_Both, opt->width, opt->height,
		&width, &height);
	img = image_create(width, height, 3);
	if (img == NULL) {
		goto abort;
	}

	// Decode quantized max value.
	maxvalue = decode_maxac(decode83(src, 1, 1));

	uint valuelen = 1 + ((uint)strlen(src) - 6) / 2;
	values = malloc(sizeof(struct colorf) * valuelen);
	if (values == NULL) {
		goto abort;
	}

	// 1つ目。
	decode_dc(&values[0], decode83(src, 2, 4));

	// 残り。
	v = &values[1];
	for (uint pos = 6, end = strlen(src); pos < end; pos += 2) {
		uint q = decode83(src, pos, 2);
		uint qr =  q / (19 * 19);
		uint qg = (q / 19) % 19;
		uint qb =  q % 19;
		v->r = decode_acq(qr) * maxvalue;
		v->g = decode_acq(qg) * maxvalue;
		v->b = decode_acq(qb) * maxvalue;
		v++;
	}

	bases_x = bases_for(width,  compx);
	bases_y = bases_for(height, compy);
	if (bases_x == NULL || bases_y == NULL) {
		goto abort;
	}

	// RGB に展開。
	uint8 *d = img->buf;
	for (uint y = 0; y < height; y++) {
		for (uint x = 0; x < width; x++) {
			struct colorf c;

			memset(&c, 0, sizeof(c));
			for (int ny = 0; ny < compy; ny++) {
				for (int nx = 0; nx < compx; nx++) {
					float base = bases_x[x * compx + nx] *
					             bases_y[y * compy + ny];
					v = &values[ny * compx + nx];
					c.r += v->r * base;
					c.g += v->g * base;
					c.b += v->b * base;
				}
			}
			*d++ = linear2srgb(c.r);
			*d++ = linear2srgb(c.g);
			*d++ = linear2srgb(c.b);
		}
	}

	success = true;
 abort:
	free(values);
	free(bases_x);
	free(bases_y);
	if (success == false) {
		image_free(img);
		img = NULL;
	}
	return img;
}

// src の pos から len バイトをデコードする。
// len は 1, 2, 4 なので戻り値は uint に収まる。
static uint
decode83(const char *src, uint pos, uint len)
{
	uint val = 0;

	for (uint i = 0; i < len; i++) {
		uint32 c = src[pos + i] - 0x20;
		if (__predict_false((uint8)c >= 0x60)) {
			return -1;
		}
		uint32 d = table_base83[c];
		if (__predict_false((int8)d < 0)) {
			return -1;
		}
		val = val * 83 + d;
	}
	return val;
}

static void
decode_dc(struct colorf *col, uint val)
{
	uint r =  val >> 16;
	uint g = (val >> 8) & 0xff;
	uint b =  val & 0xff;

	col->r = srgb2linear(r);
	col->g = srgb2linear(g);
	col->b = srgb2linear(b);
}

static float
decode_acq(uint val)
{
	int ival = (int)val - 9;
	int signsq = ival * abs(ival);
	return (float)signsq / 81;
}

static float
decode_maxac(int ival)
{
	return (float)(ival + 1) / 166;
}

static float
srgb2linear(int ival)
{
	if (ival <= 0) {
		return 0;
	}
	if (ival >= 255) {
		return 255;
	}

	float v = (float)ival / 255;
	if (v < 0.04045) {
		return v / 12.92;
	} else {
		return pow((v + 0.055) / 1.055, 2.4);
	}
}

static uint
linear2srgb(float val)
{
	if (val <= 0) {
		return 0;
	}
	if (val >= 1) {
		return 255;
	}

	int idx = (int)(val * L2SRGBSIZE);
	return table_L2SRGB[idx];
}

static float *
bases_for(uint pixels, uint comp)
{
	float *bases = malloc(sizeof(float) * pixels * comp);
	if (bases == NULL) {
		return NULL;
	}

	// cos() を c == 1 のときだけ計算する。
	// c >= 2 のときは間引きに相当するので再計算する必要はない。
	// bases はここでフルリサイズ(初期化)される前提。

	if (comp < 1) {
		goto done;
	}
	for (uint x = 0; x < pixels; x++) {
		bases[x * comp + 0] = 1;
	}
	if (comp < 2) {
		goto done;
	}

	float scale = M_PI / pixels;
	for (uint x = 0; x < pixels; x++) {
		bases[x * comp + 1] = cos(scale * x);
	}
	for (uint x = 0; x < pixels; x++) {
		for (uint c = 2; c < comp; c++) {
			uint t;
			t = (c * x) % (2 * pixels);
			if (t < pixels) {
				bases[x * comp + c] = bases[t * comp + 1];
			} else {
				t -= pixels;
				bases[x * comp + c] = -bases[t * comp + 1];
			}
		}
	}

 done:
	return bases;
}

static const uint8 table_L2SRGB[L2SRGBSIZE] = {
	  0,  34,  49,  61,  71,  79,  86,  93,
	 99, 105, 110, 115, 120, 124, 129, 133,
	137, 141, 145, 148, 152, 155, 158, 162,
	165, 168, 171, 174, 177, 179, 182, 185,
	188, 190, 193, 195, 198, 200, 202, 205,
	207, 209, 212, 214, 216, 218, 220, 223,
	225, 227, 229, 231, 233, 235, 237, 239,
	240, 242, 244, 246, 248, 250, 251, 253,
};

// Base83(?) のデコード表。'\x20'-'\x7f'
static const uint8 table_base83[0x60] = {
	0xff, 0xff, 0xff, 0x3e, 0x3f, 0x40, 0xff, 0xff,
	0xff, 0xff, 0x41, 0x42, 0x43, 0x44, 0x45, 0xff,
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
	0x08, 0x09, 0x46, 0x47, 0xff, 0x48, 0xff, 0x49,
	0x4a, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10,
	0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
	0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20,
	0x21, 0x22, 0x23, 0x4b, 0xff, 0x4c, 0x4d, 0x4e,
	0xff, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a,
	0x2b, 0x2c, 0x2d, 0x2e, 0x2f, 0x30, 0x31, 0x32,
	0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a,
	0x3b, 0x3c, 0x3d, 0x4f, 0x50, 0x51, 0x52, 0xff,
};

#endif // !GEN_L2SRGB && !GEN_BASE83

#if defined(GEN_L2SRGB)
// % cc -DGEN_L2SRGB image_blurhash.c -lm
// % ./a.out 512
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
int
main(int ac, char *av[])
{
	if (ac < 2) {
		printf("usage: %s <tablesize>\n", av[0]);
		return 1;
	}

	int n = atoi(av[1]);
	for (int i = 0; i < n; i++) {
		float val = (float)i / n;

		if (val < 0.0031308) {
			val = val * 12.92;
		} else {
			val = pow(val, (float)1 / 2.4) * 1.055 - 0.055;
		}
		int d = (int)(val * 255 + 0.5);

		printf("%c%3u,", ((i % 8) ? ' ' : '\t'), d);
		if ((i % 8) == 7) {
			printf("\n");
		}
	}
	return 0;
}
#endif

#if defined(GEN_BASE83)
// % cc -DGEN_BASE83 image_blurhash.c
// % ./a.out
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int
main(int ac, char *av[])
{
	static const char chars[] =
		"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ"
		"abcdefghijklmnopqrstuvwxyz#$%*+,-.:;=?@[]^_{|}~";

	for (int c = 0x20; c < 0x80; c++) {
		int n;
		char *p = strchr(chars, c);
		if (p == NULL) {
			n = -1;
		} else {
			n = p - chars;
		}

		printf("%c0x%02x,", ((c % 8) ? ' ' : '\t'), (uint8_t)n);
		if ((c % 8) == 7) {
			printf("\n");
		}
	}
	return 0;
}
#endif
