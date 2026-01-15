/* vi:set ts=4: */
/*
 * Copyright (C) 2026 Tetsuya Isaki
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

//
// 柳沢 PIC 読み込み
//

// PIC フォーマット仕様書
// (https://www.vector.co.jp/soft/dl/data/art/se003198.html) を基に実装。

#include "common.h"
#include "image_priv.h"
#include <err.h>
#include <string.h>

struct colorcache
{
	uint16 color;
	uint8 next;
	uint8 prev;
};

struct ypicctx
{
	FILE *fp;
	struct image *img;
	int width;
	int height;

	uint8 bits;			// 読み込んだビットデータ(左詰めしていく)
	int blen;			// bits の有効ビット長(MSB 側から数える)

	uint ncolors;			// 色数。
	uint16 palette[256];	// パレットは最大256個。

	// 色キャッシュ。
	// このキャッシュ機構は便宜的に用意したものではなく仕様にあるもの。
	struct colorcache table[128];
	int current;
};

static bool ypic_read_header(FILE *, uint8 *);
static bool ypic_expand(struct ypicctx *);
static void ypic_expand_chain(struct ypicctx *, int, int, uint);
static uint32 read_len(struct ypicctx *);
static uint16 read_color(struct ypicctx *);
static void color_init(struct ypicctx *);
static uint16 color_new(struct ypicctx *, uint16);
static uint16 color_get(struct ypicctx *, int);
static uint16 image_get_pixel(const struct image *, int, int);
static void image_set_pixel(struct image *, int, int, uint);
static uint32 readbit(struct ypicctx *, int);

// PIC の GGGGG'RRRRR'BBBBB'I 形式を 0'RRRRR'GGGGG'BBBBB に変換する。
static inline uint16
GRBI16_to_ARGB16(uint16 col)
{
	uint g = (col >> 11);
	uint r = (col >>  6) & 0x1f;
	uint b = (col >>  1) & 0x1f;
	return (r << 10) | (g << 5) | b;
}

bool
image_ypic_match(FILE *fp, const struct diag *diag)
{
	uint8 magic[3];

	size_t n = fread(magic, 3, 1, fp);
	if (n == 0) {
		Debug(diag, "%s: fread(magic) failed: %s", __func__, strerrno());
		return false;
	}

	if (magic[0] != 'P' || magic[1] != 'I' || magic[2] != 'C') {
		return false;
	}

	return true;
}

struct image *
image_ypic_read(FILE *fp, const image_read_hint *hint, const struct diag *diag)
{
	struct ypicctx ctx0;
	struct ypicctx *ctx;

	ctx = &ctx0;
	memset(ctx, 0, sizeof(*ctx));
	ctx->fp = fp;

	// 3バイトのマジック("PIC") をスキップ。
	fgetc(fp);
	fgetc(fp);
	fgetc(fp);
	// コメントとかをスキップして、ヘッダの情報部分を読み出す。
	// +$00.B*1: $00 予約
	// +$01.B*1: 機種情報
	// +$02.W*1: 色のビット数(256色なら8)
	// +$04.W*1: 横ピクセル数
	// +$06.W*1: 縦ピクセル数
	uint8 hdr[8];
	if (ypic_read_header(fp, hdr) == false) {
		warn("%s: fread(header) failed", __func__);
		return NULL;
	}

	// 機種情報は、下位ニブルが機種情報。上位は機種ごとのモード(未使用)。
	uint machtype = hdr[1] & 0x0f;
	uint colorbits = (hdr[2] << 8) | hdr[3];
	ctx->ncolors = 1U << colorbits;
	ctx->width  = (hdr[4] << 8) | hdr[5];
	ctx->height = (hdr[6] << 8) | hdr[7];

	if (diag_get_level(diag) >= 1) {
		static const char * const machtype_str[] = {
			"X68k",		// 0
			"PC-88VA",	// 1
			"FM-TOWNS",	// 2
			"MAC",		// 3
		};
		const char *t = NULL;
		if (machtype < countof(machtype_str)) {
			t = machtype_str[machtype];
		} else if (machtype == 15) {
			t = "Generic";
		} else {
			t = "?";
		}
		Debug(diag, "%s: %s (%u, %u) %u colors", __func__,
			t, ctx->width, ctx->height, ctx->ncolors);
	}
	// デバッグ表示した後でエラーにする。

	if (machtype != 0) {
		warnx("%s: Unsupported machine type: $%02x", __func__, machtype);
		return NULL;
	}

	if (colorbits != 15) {
		warnx("%s: Unsupported color mode: %u", __func__, ctx->ncolors);
		return NULL;
	}

	// 256色以下ならパレット。
	if (colorbits <= 8) {
		uint16 palbuf[ctx->ncolors];
		size_t n = fread(palbuf, 2, ctx->ncolors, fp);
		if (n < ctx->ncolors) {
			warn("%s: fread(palette) failed", __func__);
			return NULL;
		}
		for (uint i = 0; i < ctx->ncolors; i++) {
			uint piccol = be16toh(palbuf[i]);
			ctx->palette[i] = GRBI16_to_ARGB16(piccol);
		}
	}

	// 色キャッシュを初期化。
	color_init(ctx);

	// 内部形式画像を作成。
	ctx->img = image_create(ctx->width, ctx->height, IMAGE_FMT_ARGB16);
	if (ctx->img == NULL) {
		warn("%s: image_create failed", __func__);
		return NULL;
	}

	// 圧縮データの展開。
	if (ypic_expand(ctx) == false) {
		image_free(ctx->img);
		ctx->img = NULL;
	}

	return ctx->img;
}

// PIC ファイルのヘッダの固定部分を返す。
// PIC ファイルの先頭は、マジック(3バイト) に続いて、任意長のコメント、
// 任意長のパディング(?)があり、そのあとに 8バイトの情報が続く構造。
// ここではその 8 バイトを dst に返す。
// fp は先頭のマジック3バイトを読んだ直後であること。
static bool
ypic_read_header(FILE *fp, uint8 *dst)
{
	// $1A までがコメント。(たぶん Shift_JIS)
	for (;;) {
		int ch = fgetc(fp);
		if (__predict_false(ch == EOF)) {
			return false;
		}
		if (ch == 0x1a) {
			break;
		}
	}

	// 続いて $00 までがダミー。
	for (;;) {
		int ch = fgetc(fp);
		if (__predict_false(ch == EOF)) {
			return false;
		}
		if (ch == 0x00) {
			break;
		}
	}

	// ここから8バイトの情報。
	size_t n = fread(dst, 8, 1, fp);
	if (n == 0) {
		return false;
	}
	return true;
}

// 端の処理。
#define ROUND_EDGE	do {								\
	if (__predict_false(++x >= ctx->width)) {			\
		if (__predict_false(++y >= ctx->height))		\
			goto done;									\
		x = 0;											\
	}													\
} while (0)

static bool
ypic_expand(struct ypicctx *ctx)
{
	int x = -1;
	int y = 0;
	uint c = 0;

	for (;;) {
		// 変化点間の長さ。
		int l = read_len(ctx);

		// 次の変化点まで繰り返す。
		while (--l) {
			ROUND_EDGE;

			// 連鎖点上を通過した時は、現在の色を変更。
			uint a = image_get_pixel(ctx->img, x, y);
			if (__predict_false(a != 0)) {
				c = a & 0x7fff;
			}
			// 現在の色を書き込む。
			image_set_pixel(ctx->img, x, y, c);
		}

		ROUND_EDGE;

		// 新しい色。
		c = read_color(ctx);
		image_set_pixel(ctx->img, x, y, c);

		if (readbit(ctx, 1) != 0) {
			ypic_expand_chain(ctx, x, y, c);
		}
	}

 done:
	return true;
}

static void
ypic_expand_chain(struct ypicctx *ctx, int x, int y, uint c)
{
	for (;;) {
		uint dir = readbit(ctx, 2);
		switch (dir) {
		 case 0:
			if (readbit(ctx, 1) == 0) {
				return;
			}
			if (readbit(ctx, 1) == 0) {
				x -= 2;
			} else {
				x += 2;
			}
			break;
		 case 1:
			x--;
			break;
		 case 2:
			break;
		 case 3:
			x++;
			break;
		}
		y++;
		if (__predict_true(y < ctx->height)) {
			// MSB を連鎖マークに使う。
			// (オリジナルも I ビットのある LSB を使っていた)
			image_set_pixel(ctx->img, x, y, c | 0x8000);
		}
	}
}

// 長さデータを読み込んで返す。
static uint32
read_len(struct ypicctx *ctx)
{
	uint b;

	b = 1;
	while (readbit(ctx, 1) != 0) {
		b++;
	}
	uint32 val = readbit(ctx, b) + (1U << b) - 1;
	return val;
}

// 色データを読み込んで返す。
static uint16
read_color(struct ypicctx *ctx)
{
	if (readbit(ctx, 1)) {
		// cache hit
		return color_get(ctx, readbit(ctx, 7));
	} else {
		// cache miss
		uint16 piccol = readbit(ctx, 15) << 1;
		return color_new(ctx, GRBI16_to_ARGB16(piccol));
	}
}

// 色キャッシュを初期化。
static void
color_init(struct ypicctx *ctx)
{
	for (uint i = 0; i < 128; i++) {
		ctx->table[i].color = 0;
		ctx->table[i].prev = i + 1;
		ctx->table[i].next = i - 1;
	}
	ctx->table[127].prev = 0;
	ctx->table[0].next = 127;
	ctx->current = 0;
}

// 新しい色をキャッシュに登録して、その色を返す。
static uint16
color_new(struct ypicctx *ctx, uint16 c)
{
	ctx->current = ctx->table[ctx->current].prev;
	ctx->table[ctx->current].color = c;
	return c;
}

// キャッシュから色を取り出す。
static uint16
color_get(struct ypicctx *ctx, int idx)
{
	struct colorcache *table = ctx->table;

	if (ctx->current != idx) {
		table[table[idx].prev].next = table[idx].next;
		table[table[idx].next].prev = table[idx].prev;
		table[table[ctx->current].prev].next = idx;
		table[idx].prev = table[ctx->current].prev;
		table[ctx->current].prev = idx;
		table[idx].next = ctx->current;
		ctx->current = idx;
	}

	return table[idx].color;
}

// img の (x, y) の位置を色を返す。
static uint16
image_get_pixel(const struct image *img, int x, int y)
{
	const uint16 *buf = (uint16 *)img->buf;
	return buf[(img->width * y) + x];
}

// img の (x, y) の位置に色を書き込む。
static void
image_set_pixel(struct image *img, int x, int y, uint c)
{
	uint16 *buf = (uint16 *)img->buf;
	buf[(img->width * y) + x] = c;
}

// n ビットを読み込んで返す。
static uint32
readbit(struct ypicctx *ctx, int n)
{
	uint32 val;

	val = 0;
	for (int i = 0; i < n; i++) {
		if (__predict_false(ctx->blen == 0)) {
			ctx->bits = fgetc(ctx->fp);
			ctx->blen = 8;
		}
		val <<= 1;
		if ((ctx->bits & 0x80)) {
			val++;
		}
		ctx->bits <<= 1;
		ctx->blen--;
	}

	return val;
}
