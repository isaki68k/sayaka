/* vi:set ts=4: */
/*
 * Copyright (C) 2026 Tetsuya Isaki
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
// PNM 読み込み
//

// PNM (総称) ファイルは先頭の2バイトで形式が7つに分類される。
// "P1" … PBM(1bpp)、データ部が ASCII
// "P2" … PGM(Gray)、データ部が ASCII
// "P3" … PPM(RGB)、 データ部が ASCII
// "P4" … PBM(1bpp)、データ部がバイナリ
// "P5" … PGM(Gray)、データ部がバイナリ
// "P6" … PPM(RGB)、 データ部がバイナリ
// "P7" … PAM(RGBA等)、データ部はバイナリのみ

#include "common.h"
#include "image_priv.h"
#include "ascii_ctype.h"
#include <err.h>
#include <errno.h>
#include <string.h>

struct pnmctx
{
	FILE *fp;

	char *p;	// カーソル位置

	// 最大値 (bitdepth = 8 なら 255)。
	// PBM では使わない。
	uint maxval;

	// maxval 階調を 256 階調に変換するテーブル。
	// 先頭から maxval 個だけ使う。PBM では使わない。
	union {
		uint8 b[256];
		uint16 w[256];
	} palette;

	// 仕様では1行70文字を超えてはならないとされている。
	char buf[256];
};

#define CTX_INIT(ctx, fp, diag)	do {	\
	memset(ctx, 0, sizeof(*(ctx)));	\
	(ctx)->fp = (fp);	\
	(ctx)->p = (ctx)->buf;	\
} while (0)

static int  image_pnm_match(FILE *, const struct diag *);
static struct image *image_pnm_read_init(struct pnmctx *, int,
	const struct diag *);
static int  getnum(struct pnmctx *);
static const char *getstr(struct pnmctx *);

//
// P1: PBM (ASCII)
//

bool
image_pnm1_match(FILE *fp, const struct diag *diag)
{
	return (image_pnm_match(fp, diag) == '1');
}

struct image *
image_pnm1_read(FILE *fp, const image_read_hint *hint, const struct diag *diag)
{
	struct pnmctx ctx0;
	struct pnmctx *ctx = &ctx0;
	struct image *img;

	CTX_INIT(ctx, fp, diag);

	img = image_pnm_read_init(ctx, 1, diag);
	if (img == NULL) {
		return NULL;
	}
	Debug(diag, "%s: width=%u height=%u", __func__,
		img->width, img->height);

	// PBM は '0' と '1' しかないので、ピクセル間の空白を無視出来る。
	const char *s;
	uint16 *d = (uint16 *)img->buf;
	const uint16 *dend = d + image_get_stride(img) * img->height;
	while ((s = getstr(ctx)) != NULL) {
		for (; *s && d < dend; s++) {
			// 色の情報はないがグレースケールとの親和性のため 0 を黒とする。
			uint16 cc = (*s == '0') ? 0x0000 : 0x7fff;
			*d++ = cc;
		}
	}

	return img;
}

//
// P2: PGM (ASCII)
//

bool
image_pnm2_match(FILE *fp, const struct diag *diag)
{
	return (image_pnm_match(fp, diag) == '2');
}

struct image *
image_pnm2_read(FILE *fp, const image_read_hint *hint, const struct diag *diag)
{
	struct pnmctx ctx0;
	struct pnmctx *ctx = &ctx0;
	struct image *img;

	CTX_INIT(ctx, fp, diag);

	img = image_pnm_read_init(ctx, 2, diag);
	if (img == NULL) {
		return NULL;
	}
	Debug(diag, "%s: width=%u height=%u maxval=%u", __func__,
		img->width, img->height, ctx->maxval);

	int idx;
	uint16 *d = (uint16 *)img->buf;
	const uint16 *dend = d + image_get_stride(img) * img->height;
	while ((idx = getnum(ctx)) != -1 && d < dend) {
		uint16 cc = ctx->palette.w[idx];
		*d++ = cc;
	}

	return img;
}

//
// P3: PPM (ASCII)
//

bool
image_pnm3_match(FILE *fp, const struct diag *diag)
{
	return (image_pnm_match(fp, diag) == '3');
}

struct image *
image_pnm3_read(FILE *fp, const image_read_hint *hint, const struct diag *diag)
{
	struct pnmctx ctx0;
	struct pnmctx *ctx = &ctx0;
	struct image *img;

	CTX_INIT(ctx, fp, diag);

	img = image_pnm_read_init(ctx, 3, diag);
	if (img == NULL) {
		return NULL;
	}
	Debug(diag, "%s: width=%u height=%u maxval=%u", __func__,
		img->width, img->height, ctx->maxval);

	int idx;
	uint8 *d = (uint8 *)img->buf;
	const uint8 *dend = d + image_get_stride(img) * img->height;
	while ((idx = getnum(ctx)) != -1 && d < dend) {
		uint8 c = ctx->palette.b[idx];
		*d++ = c;
	}

	return img;
}

//
// 下請け
//

// image_*_match() の共通部分。
// PNM ならマジックの2バイト目を返す。そうでなければ 0 を返す。
static int
image_pnm_match(FILE *fp, const struct diag *diag)
{
	uint8 magic[2];

	size_t n = fread(magic, sizeof(magic), 1, fp);
	if (n == 0) {
		Debug(diag, "%s: fread failed: %s", __func__, strerrno());
		return 0;
	}

	if (magic[0] != 'P') {
		return 0;
	}
	return magic[1];
}

// image_*_read() の冒頭の共通部分。
// type==1 は PBM、
// type==2 は PGM、
// type==3 は PPM。
static struct image *
image_pnm_read_init(struct pnmctx *ctx, int type, const struct diag *diag)
{
	int width;
	int height;
	int maxval = 0;

	// マジックの2文字は読み捨てる。
	fgetc(ctx->fp);
	fgetc(ctx->fp);

	// 横と縦のピクセル数。
	width = getnum(ctx);
	if (width < 0) {
		return NULL;
	}
	height = getnum(ctx);
	if (height < 0) {
		return NULL;
	}

	// PBM 以外なら続いて最大値、8bpp なら 255。
	if (type != 1) {
		maxval = getnum(ctx);
		if (maxval < 0) {
			return NULL;
		}
		if (maxval > 255) {
			Debug(diag, "%s: maxval=%u not supported", __func__, maxval);
			return NULL;
		}

		ctx->maxval = maxval;
	}

	// PGM なら ARGB16 のテーブルを作成、
	// PPM なら uint8 のテーブルを作成。
	switch (type) {
	 case 2:
		for (uint i = 0; i <= maxval; i++) {
			uint8 v = (i * 255 / maxval) >> 3;
			uint16 cc = (v << 10) | (v << 5) | v;
			ctx->palette.w[i] = cc;
		}
		break;
	 case 3:
		for (uint i = 0; i <= maxval; i++) {
			uint8 v = i * 255 / maxval;
			ctx->palette.b[i] = v;
		}
		break;
	 default:
		__unreachable();
	}

	int imgfmt;
	switch (type) {
	 case 1:
	 case 2:
		imgfmt = IMAGE_FMT_ARGB16;
		break;
	 case 3:
		imgfmt = IMAGE_FMT_RGB24;
		break;
	 default:
		__unreachable();
	}

	return image_create(width, height, imgfmt);
}

// 次の単語を非負の10進数として数値にして返す。数値に出来なければ 0 を返す。
// EOF なら -1 を返す。
static int
getnum(struct pnmctx *ctx)
{
	char *end;
	long lval;

	const char *p = getstr(ctx);
	if (p == NULL) {
		return -1;
	}
	errno = 0;
	lval = strtol(p, &end, 10);
	if (end == p || errno == ERANGE) {
		return 0;
	}
	if (*end != '\0' && is_ascii_space(*end) == false) {
		return 0;
	}
	if (lval < 0) {
		return 0;
	}
	return (uint)lval;
}

// 次の単語を文字列のまま返す。EOF なら NULL を返す。
// 戻り値は ctx 内のバッファを指しているので解放不要。
static const char *
getstr(struct pnmctx *ctx)
{
	for (;;) {
		// ポインタが文字列の最後に達していれば、次の行を読み込む。
		while (*ctx->p == '\0') {
			if (fgets(ctx->buf, sizeof(ctx->buf), ctx->fp) == NULL) {
				return NULL;
			}

			// コメントがあれば削除。
			char *s = ctx->buf;
			char *e = strchr(s, '#');
			if (e) {
				*e = '\0';
			} else {
				e = strchr(s, '\0');
			}

			// その上で行末の空白文字を削除。PNM の空白は isspace(3) 準拠。
			while (--e >= s) {
				if (!is_ascii_space(*e)) {
					break;
				}
			}
			e[1] = '\0';

			ctx->p = s;
		}

		// まず空白文字をスキップ。
		char *p = ctx->p;
		for (; *p; p++) {
			if (!is_ascii_space(*p)) {
				break;
			}
		}
		if (*p == '\0') {
			// 空白をスキップしたら行末に来た。
			continue;
		}

		// ここから一単語。
		char *end = p;
		for (; *end; end++) {
			if (is_ascii_space(*end)) {
				*end++ = '\0';
				break;
			}
		}
		ctx->p = end;
		return p;
	}
}
