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
#include <limits.h>
#include <string.h>

struct pnmctx
{
	FILE *fp;

	// 画像サイズ
	uint width;
	uint height;

	// 最大値 (bitdepth = 8 なら 255)。
	// PBM では使わない。
	uint maxval;

	// テキストモードでの行バッファ。
	// 仕様では1行70文字を超えてはならないとされている。
	char textbuf[256];

	// textbuf 中のカーソル位置。
	char *p;

	// バイナリ1ラスタ分のバッファ。
	uint8 *binbuf;
};

#define PNM_INIT(pnm, fp)	do {	\
	memset(pnm, 0, sizeof(*(pnm)));	\
	(pnm)->fp = (fp);	\
	(pnm)->p = (pnm)->textbuf;	\
} while (0)

// (val / maxval) を内部フォーマット用の 32 段階に変換。
#define PNMValToUInt5(val)	(((val) * 31) / pnm->maxval)

#define ARGB16_BLACK	RGB888_to_ARGB16(  0,   0,   0)
#define ARGB16_WHITE	RGB888_to_ARGB16(255, 255, 255)

typedef void (*rasterop_t)(struct pnmctx *, uint16 *);

static int  image_pnm_match(FILE *, const struct diag *);
static struct image *image_pnm_read_binary(FILE *, const struct diag *);
static void raster_pgm_byte(struct pnmctx *, uint16 *);
static void raster_pgm_word(struct pnmctx *, uint16 *);
static void raster_ppm_byte(struct pnmctx *, uint16 *);
static void raster_ppm_word(struct pnmctx *, uint16 *);
static int  parse_pnm_header(struct pnmctx *, const struct diag *);
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
	struct pnmctx pnm0;
	struct pnmctx *pnm = &pnm0;
	struct image *img;

	PNM_INIT(pnm, fp);

	if (parse_pnm_header(pnm, diag) < 0) {
		return NULL;
	}
	const uint width  = pnm->width;
	const uint height = pnm->height;

	img = image_create(width, height, IMAGE_FMT_ARGB16);
	if (img == NULL) {
		return NULL;
	}
	Debug(diag, "%s: width=%u height=%u", __func__, width, height);

	// PBM は '0' と '1' しかないので、ピクセル間の空白を無視出来る。
	const char *s;
	uint16 *d = (uint16 *)img->buf;
	const uint16 *dend = d + width * height;
	while ((s = getstr(pnm)) != NULL) {
		for (; *s && d < dend; s++) {
			// 色の情報はないがグレースケールとの親和性のため 0 を黒とする。
			uint16 cc = (*s == '0') ? ARGB16_BLACK : ARGB16_WHITE;
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
	struct pnmctx pnm0;
	struct pnmctx *pnm = &pnm0;
	struct image *img;

	PNM_INIT(pnm, fp);

	if (parse_pnm_header(pnm, diag) < 0) {
		return NULL;
	}
	const uint width  = pnm->width;
	const uint height = pnm->height;

	img = image_create(width, height, IMAGE_FMT_ARGB16);
	if (img == NULL) {
		return NULL;
	}
	Debug(diag, "%s: width=%u height=%u maxval=%u", __func__,
		width, height, pnm->maxval);

	uint16 *d = (uint16 *)img->buf;
	const uint16 *dend = d + width * height;
	while (d < dend) {
		int idx = getnum(pnm);
		if (__predict_false(idx < 0)) {
			break;
		}
		uint v = PNMValToUInt5(idx);
		*d++ = RGB555_to_ARGB16(v, v, v);
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
	struct pnmctx pnm0;
	struct pnmctx *pnm = &pnm0;
	struct image *img;

	PNM_INIT(pnm, fp);

	if (parse_pnm_header(pnm, diag) < 0) {
		return NULL;
	}
	const uint width  = pnm->width;
	const uint height = pnm->height;

	img = image_create(width, height, IMAGE_FMT_ARGB16);
	if (img == NULL) {
		return NULL;
	}
	Debug(diag, "%s: width=%u height=%u maxval=%u", __func__,
		width, height, pnm->maxval);

	uint16 *d = (uint16 *)img->buf;
	const uint16 *dend = d + width * height;
	while (d < dend) {
		int r = getnum(pnm);
		int g = getnum(pnm);
		int b = getnum(pnm);
		if (__predict_false(r < 0 || g < 0 || b < 0)) {
			break;
		}
		r = PNMValToUInt5(r);
		g = PNMValToUInt5(g);
		b = PNMValToUInt5(b);
		*d++ = RGB555_to_ARGB16(r, g, b);
	}

	return img;
}


//
// P5: PGM (Binary)
//

bool
image_pnm5_match(FILE *fp, const struct diag *diag)
{
	return (image_pnm_match(fp, diag) == '5');
}

struct image *
image_pnm5_read(FILE *fp, const image_read_hint *hint, const struct diag *diag)
{
	return image_pnm_read_binary(fp, diag);
}

// PGM(P5) で maxval < 256 の場合のラスターコールバック。
static void
raster_pgm_byte(struct pnmctx *pnm, uint16 *d)
{
	const uint8 *s = pnm->binbuf;
	const uint8 *send = s + pnm->width;

	while (s < send) {
		uint v = *s++;
		v = PNMValToUInt5(v);
		*d++ = RGB555_to_ARGB16(v, v, v);
	}
}

// PGM(P5) で maxval >= 256 の場合のラスターコールバック。
static void
raster_pgm_word(struct pnmctx *pnm, uint16 *d)
{
	const uint16 *s = (const uint16 *)pnm->binbuf;
	const uint16 *send = s + pnm->width;

	while (s < send) {
		uint v = be16toh(*s++);
		v = PNMValToUInt5(v);
		*d++ = RGB555_to_ARGB16(v, v, v);
	}
}


//
// P6: PPM (Binary)
//

bool
image_pnm6_match(FILE *fp, const struct diag *diag)
{
	return (image_pnm_match(fp, diag) == '6');
}

struct image *
image_pnm6_read(FILE *fp, const image_read_hint *hint, const struct diag *diag)
{
	return image_pnm_read_binary(fp, diag);
}

// PPM(P6) で maxval < 256 の場合のラスターコールバック。
static void
raster_ppm_byte(struct pnmctx *pnm, uint16 *d)
{
	const uint8 *s = pnm->binbuf;
	const uint8 *send = s + pnm->width * 3;

	while (s < send) {
		uint r = *s++;
		uint g = *s++;
		uint b = *s++;
		r = PNMValToUInt5(r);
		g = PNMValToUInt5(g);
		b = PNMValToUInt5(b);
		*d++ = RGB555_to_ARGB16(r, g, b);
	}
}

// PPM(P6) で maxval >= 256 の場合のラスターコールバック。
static void
raster_ppm_word(struct pnmctx *pnm, uint16 *d)
{
	const uint16 *s = (const uint16 *)pnm->binbuf;
	const uint16 *send = s + pnm->width * 3;

	while (s < send) {
		uint r = be16toh(*s++);
		uint g = be16toh(*s++);
		uint b = be16toh(*s++);
		r = PNMValToUInt5(r);
		g = PNMValToUInt5(g);
		b = PNMValToUInt5(b);
		*d++ = RGB555_to_ARGB16(r, g, b);
	}
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

// ヘッダ部分を解析。ヘッダは
// o 2文字のマジックナンバー (判定済みなので飛ばしてよい)
// o (1文字以上の?) 空白文字(isspace)
// o ASCII 10進数の横ピクセル数
// o (1文字以上の?) 空白文字
// o ASCII 10進数の縦ピクセル数
// if (PGM or PPM)
//  o (1文字以上の?) 空白文字
//  o ASCII 10進数の最大値
// endif
// o "1文字の" 空白文字(通常は改行)
// から構成されており、この次の文字からはテキストかバイナリデータとなる。
// 最後の空白文字以前には '#' から改行までを無視するコメントが置ける。
//
// 成功すれば width, height(, maxval) を pnm に書き戻してマジックナンバーの
// 2文字目を返す。
// 失敗すれば -1 を返す。
static int
parse_pnm_header(struct pnmctx *pnm, const struct diag *diag)
{
	char hdrbuf[128];
	uint maxwords;
	int pnmtype;
	int ch;
	enum {
		WORD,
		WSP,
		COMMENT,
	} state, prev;

	char *d = hdrbuf;
	const char *dend = d + sizeof(hdrbuf) - 1;

	// マジックナンバー。1文字目は無視。
	fgetc(pnm->fp);
	pnmtype = fgetc(pnm->fp);

	// maxwords 分の語を取り出す。
	if (pnmtype == '1' || pnmtype == '4') {
		maxwords = 2;
	} else {
		maxwords = 3;
	}
	prev = WORD;
	state = WSP;
	uint nwords = 0;
	while (d < dend && (ch = fgetc(pnm->fp)) != -1) {
	 again:
		if (state == WORD) {
			if (ch == '#') {
				prev = state;
				state = COMMENT;
			} else if (is_ascii_space(ch)) {
				nwords++;
				if (nwords == maxwords) {
					break;
				}
				*d++ = ' ';
				prev = state;
				state = WSP;
			} else {
				*d++ = ch;
			}
		} else if (state == WSP) {
			if (ch == '#') {
				prev = state;
				state = COMMENT;
			} else if (is_ascii_space(ch)) {
				// 連続する空白は無視。
			} else {
				*d++ = ch;
				prev = state;
				state = WORD;
			}
		} else if (state == COMMENT) {
			// '#' から CR か LF *の手前まで* をコメントとして無視。
			if (ch == '\r' || ch == '\n') {
				state = prev;
				prev = COMMENT;
				goto again;
			} else {
				// コメント中は無視。
			}
		}
	}
	*d = '\0';

	// hdrbuf は [<SP>] width <SP> height [ <SP> maxval ] '\0' のはず。
	// strto*() は冒頭の空白をスキップするのでここでは都合がいい。

	char *s;
	char *end;
	long width;
	long height;
	long maxval = 0;

	s = hdrbuf;
	errno = 0;
	width = strtol(s, &end, 10);
	if (end == s || (*end != '\0' && *end != ' ') || errno != 0) {
		Trace(diag, "%s: Invalid width string", __func__);
		return -1;
	}
	if (width < 0 || width > INT_MAX) {
		Trace(diag, "%s: Invalid width=%ld", __func__, width);
		return -1;
	}

	s = end;
	errno = 0;
	height = strtoul(s, &end, 10);
	if (end == s || (*end != '\0' && *end != ' ') || errno != 0) {
		Trace(diag, "%s: Invalid height string", __func__);
		return -1;
	}
	if (height < 0 || height > INT_MAX) {
		Trace(diag, "%s: Invalid height=%lu", __func__, height);
		return -1;
	}

	if (maxwords == 3) {
		s = end;
		errno = 0;
		maxval = strtoul(s, &end, 10);
		if (end == s || (*end != '\0' && *end != ' ') || errno != 0) {
			Trace(diag, "%s: Invalid maxval string", __func__);
			return -1;
		}
		if (maxval < 0 || maxval > 65535) {
			Trace(diag, "%s: Invalid maxval=%lu", __func__, maxval);
			return -1;
		}
	}

	pnm->width  = (uint)width;
	pnm->height = (uint)height;
	pnm->maxval = (uint)maxval;

	return pnmtype;
}

// 次の単語を非負の10進数として数値にして返す。数値に出来なければ 0 を返す。
// EOF なら -1 を返す。
static int
getnum(struct pnmctx *pnm)
{
	char *end;
	long lval;

	const char *p = getstr(pnm);
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
// 戻り値は pnm 内のバッファを指しているので解放不要。
static const char *
getstr(struct pnmctx *pnm)
{
	for (;;) {
		// ポインタが文字列の最後に達していれば、次の行を読み込む。
		while (*pnm->p == '\0') {
			if (fgets(pnm->textbuf, sizeof(pnm->textbuf), pnm->fp) == NULL) {
				return NULL;
			}

			// コメントがあれば削除。
			char *s = pnm->textbuf;
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

			pnm->p = s;
		}

		// まず空白文字をスキップ。
		char *p = pnm->p;
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
		pnm->p = end;
		return p;
	}
}

// バイナリ形式の共通読み込み部分。
static struct image *
image_pnm_read_binary(FILE *fp, const struct diag *diag)
{
	struct pnmctx pnm0;
	struct pnmctx *pnm = &pnm0;
	struct image *img;
	rasterop_t rasterop;
	int pnmtype;
	size_t bpp;		// 1ピクセルのバイト数

	PNM_INIT(pnm, fp);

	pnmtype = parse_pnm_header(pnm, diag);
	if (pnmtype < 0) {
		return NULL;
	}
	const uint width  = pnm->width;
	const uint height = pnm->height;

	img = image_create(width, height, IMAGE_FMT_ARGB16);
	if (img == NULL) {
		return NULL;
	}
	Debug(diag, "%s: width=%u height=%u maxval=%u", __func__,
		width, height, pnm->maxval);

	// ラスター処理関数を選択。
	switch (pnmtype) {
	 case '5':
		if (pnm->maxval < 256) {
			rasterop = raster_pgm_byte;
			bpp = 1;
		} else {
			rasterop = raster_pgm_word;
			bpp = 2;
		}
		break;
	 case '6':
		if (pnm->maxval < 256) {
			rasterop = raster_ppm_byte;
			bpp = 3;
		} else {
			rasterop = raster_ppm_word;
			bpp = 6;
		}
		break;
	 default:
		__unreachable();
	}

	pnm->binbuf = malloc(bpp * width);
	if (pnm->binbuf == NULL) {
		warn("%s: malloc(%zu) failed", __func__, bpp * width);
		image_free(img);
		return NULL;
	}

	uint16 *d = (uint16 *)img->buf;
	const uint16 *dend = d + width * height;
	for (; d < dend; d += width) {
		// 1ラスター分読み込んで..
		uint n = fread(pnm->binbuf, bpp, width, pnm->fp);
		if (n < width) {
			break;
		}

		// 内部形式に変換。
		(*rasterop)(pnm, d);
	}

	free(pnm->binbuf);
	return img;
}
