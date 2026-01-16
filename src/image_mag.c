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
// MAG 読み込み
//

#include "common.h"
#include "image_priv.h"
#include <err.h>
#include <string.h>

struct magctx {
	FILE *fp;

	uint8 *flagA;
	uint8 *flagB;
	uint16 *pixel;

	// 中間 VRAM。
	// 1ワード(16bit) で16色なら4ドット分、256色なら2ドット分を持つ。
	// ファイルからの展開と VRAM 内コピーはこの単位で行われるため。
	uint16 *vram;

	// ピクセルブロック上の現在位置。
	uint16 *pp;

	// VRAM 上の X, Y 方向の要素数。
	// X 方向は uint16 単位 (16色モードなら4ドットに相当) で数える。
	uint vwidth;
	uint vheight;

	size_t offset[16];

	uint32 flagA_size;

	uint ncolors;
	uint16 palette[256];
};

static bool mag_read_palette(struct magctx *);
static inline void mag_expand_b(struct magctx *, uint16 *, uint);
static void mag_expand(struct magctx *);
static void mag_expand_color16(struct magctx *, struct image *);
static void mag_expand_color256(struct magctx *, struct image *);

static const struct {
	int x;
	int y;
} mag_offset[16] = {
	{ 0, 0 },	// Dummy

	{ 1, 0 },	// 1
	{ 2, 0 },	// 2
	{ 4, 0 },	// 3

	{ 0, 1 },	// 4
	{ 1, 1 },	// 5

	{ 0, 2 },	// 6
	{ 1, 2 },	// 7
	{ 2, 2 },	// 8

	{ 0, 4 },	// 9
	{ 1, 4 },	// 10
	{ 2, 4 },	// 11

	{ 0, 8 },	// 12
	{ 1, 8 },	// 13
	{ 2, 8 },	// 14

	{ 0,16 },	// 15
};

bool
image_mag_match(FILE *fp, const struct diag *diag)
{
	static const char magic[8] = { 'M', 'A', 'K', 'I', '0', '2', ' ', ' ' };
	uint8 buf[8];

	size_t n = fread(buf, 8, 1, fp);
	if (n == 0) {
		Debug(diag, "%s: fread(magic) failed: %s", __func__, strerrno());
		return false;
	}

	if (memcmp(buf, magic, sizeof(magic)) != 0) {
		return false;
	}

	return true;
}

struct image *
image_mag_read(FILE *fp, const image_read_hint *hint, const struct diag *diag)
{
	struct magctx ctx0;
	struct magctx *ctx;
	struct image *img = NULL;
	uint8 hdr[32];
	size_t n;
	bool ok = false;

	ctx = &ctx0;
	memset(ctx, 0, sizeof(*ctx));
	ctx->fp = fp;

	// 8バイトのマジックを読み捨てる。
	n = fread(hdr, 8, 1, fp);
	if (n == 0) {
		warn("%s: fread(magic) failed", __func__);
		return NULL;
	}

	// $1A までのコメントをスキップ。
	for (;;) {
		int ch = fgetc(fp);
		if (ch == EOF) {
			warnx("%s: Header not found", __func__);
			return NULL;
		}
		if (ch == '\x1a') {
			break;
		}
	}

	// ここが後続のオフセットの基準点。
	uint32 hdr_start = ftell(fp);

	// 続いて32バイトのヘッダ。
	n = fread(&hdr, sizeof(hdr), 1, fp);
	if (n == 0) {
		warn("%s: fread(header) failed", __func__);
		return NULL;
	}

	// +$00.b ヘッダ先頭マーク($00)
	// +$01.b 機種コード
	// +$02.b 機種依存フラグ
	// +$03.b スクリーンモード
	// +$04.w 表示開始 X
	// +$06.w 表示開始 Y
	// +$08.w 表示終了 X (閉区間、横640px なら 639)
	// +$0a.w 表示終了 Y (〃)
	// +$0c.l フラグA ブロックのオフセット (オフセットはヘッダ先頭から)
	// +$10.l フラグB ブロックのオフセット
	// +$14.l フラグB ブロックのサイズ
	// +$18.l ピクセルブロックのオフセット
	// +$1c.l ピクセルブロックのサイズ
	// 数値はリトルエンディアン。

	//uint machtype = hdr[1];
	//uint machflag = hdr[2];
	uint screenmode = hdr[3];
	uint start_x = le16toh(*(uint16 *)&hdr[0x04]);
	uint start_y = le16toh(*(uint16 *)&hdr[0x06]);
	uint end_x   = le16toh(*(uint16 *)&hdr[0x08]);
	uint end_y   = le16toh(*(uint16 *)&hdr[0x0a]);
	uint width  = end_x + 1 - start_x;
	uint height = end_y + 1 - start_y;
	// こちらのオフセットはファイル先頭から。
	uint32 flagA_offset = le32toh(*(uint32 *)&hdr[0x0c]) + hdr_start;
	uint32 flagB_offset = le32toh(*(uint32 *)&hdr[0x10]) + hdr_start;
	uint32 flagB_size   = le32toh(*(uint32 *)&hdr[0x14]);
	uint32 pixel_offset = le32toh(*(uint32 *)&hdr[0x18]) + hdr_start;
	uint32 pixel_size   = le32toh(*(uint32 *)&hdr[0x1c]);

	// スクリーンモードは、
	// bit0 が %1 なら、200 ライン(というか、ドットの縦横比が 2:1)。
	// bit2 が %1 ならデジタル色、%0 ならアナログ色。
	// 色は:
	// b7	b1
	// ---	---
	//	0	0	16色
	//	0	1	8色
	//	1	0	256色
	bool doubler = (screenmode & 0x01);
	bool digital = (screenmode & 0x02);
	if ((screenmode & 0x80)) {
		ctx->ncolors = 256;
	} else {
		if ((screenmode & 0x02)) {
			ctx->ncolors = 8;
		} else {
			ctx->ncolors = 16;
		}
	}

	Debug(diag, "%s: (%u, %u%s) lines, %s %u colors", __func__,
		width, height,
		(doubler ? " *2" : ""),
		(digital ? "digital" : "analog"),
		ctx->ncolors);
	if (start_x != 0 || start_y != 0) {
		Debug(diag, "%s: start=(%u, %u) end=(%u, %u)", __func__,
			start_x, start_y, end_x, end_y);
	}

	// 続いてパレットブロック。
	if (mag_read_palette(ctx) == false) {
		warn("%s: fread(palette) failed", __func__);
		return NULL;
	}

	// フラグAブロックを読み込む。
	fseek(fp, flagA_offset, SEEK_SET);
	ctx->flagA_size = flagB_offset - flagA_offset;
	ctx->flagA = malloc(ctx->flagA_size);
	if (ctx->flagA == NULL) {
		warn("%s: malloc flagA (%u) failed", __func__, ctx->flagA_size);
		goto abort;
	}
	n = fread(ctx->flagA, ctx->flagA_size, 1, fp);
	if (n == 0) {
		warnx("%s: fread(flagA) failed", __func__);
		goto abort;
	}

	// フラグBブロックを読み込む。
	fseek(fp, flagB_offset, SEEK_SET);
	ctx->flagB = malloc(flagB_size);
	if (ctx->flagB == NULL) {
		warn("%s: malloc flagB (%u) failed", __func__, flagB_size);
		goto abort;
	}
	n = fread(ctx->flagB, flagB_size, 1, fp);
	if (n == 0) {
		warnx("%s: fread(flagB) failed", __func__);
		goto abort;
	}

	// ピクセルブロックを読み込む。
	fseek(fp, pixel_offset, SEEK_SET);
	ctx->pixel = malloc(pixel_size);
	if (ctx->pixel == NULL) {
		warn("%s: malloc pixel (%u) failed", __func__, pixel_size);
		goto abort;
	}
	n = fread(ctx->pixel, pixel_size, 1, fp);
	if (n == 0) {
		warnx("%s: fread(pixel) failed", __func__);
		goto abort;
	}

	// 中間 VRAM を用意。
	uint32 vram_size;
	if (ctx->ncolors == 256) {
		// 横2ドットで VRAM 1ワード。
		ctx->vwidth = width / 2;
	} else {
		// 横4ドットで VRAM 1ワード。
		ctx->vwidth = width / 4;
	}
	ctx->vheight = height;
	vram_size = ctx->vheight * ctx->vwidth * sizeof(uint16);
	ctx->vram = calloc(1, vram_size);
	if (ctx->vram == NULL) {
		warnx("%s: malloc vram (%u) failed", __func__, vram_size);
		goto abort;
	}

	// 相対位置 (X, Y) を VRAM 上の相対位置(距離) にしておく。
	for (uint i = 1; i < 16; i++) {
		ctx->offset[i] = (ctx->vwidth * mag_offset[i].y) + mag_offset[i].x;
	}

	// 中間 VRAM に展開。
	mag_expand(ctx);

	// 内部形式で作成。
	img = image_create(width, height, IMAGE_FMT_ARGB16);
	if (img == NULL) {
		warnx("%s: image_create failed", __func__);
		goto abort;
	}

	if (ctx->ncolors == 256) {
		mag_expand_color256(ctx, img);
	} else {
		mag_expand_color16(ctx, img);
	}

	ok = true;
 abort:
	free(ctx->vram);
	free(ctx->pixel);
	free(ctx->flagB);
	free(ctx->flagA);
	if (ok == false) {
		image_free(img);
		img = NULL;
	}
	return img;
}

// パレットを読み込む。
static bool
mag_read_palette(struct magctx *ctx)
{
	// パレットは、256色なら256個、8/16色なら16個。
	// 1エントリは G,B,R 各1バイト。
	uint npal = ctx->ncolors;
	if (npal < 16) {
		npal = 16;
	}

	uint8 buf[3 * npal];
	size_t n = fread(buf, 3 * npal, 1, ctx->fp);
	if (n == 0) {
		return false;
	}

	const uint8 *s = buf;
	for (uint i = 0; i < npal; i++) {
		uint g = *s++;
		uint r = *s++;
		uint b = *s++;
		r >>= 3;
		g >>= 3;
		b >>= 3;
		ctx->palette[i] = (r << 10) | (g << 5) | b;
	}
	return true;
}

// 4ビットの相対位置フラグ b から仮想 VRAM に1ワード展開。
static inline void
mag_expand_b(struct magctx *ctx, uint16 *vp, uint b)
{
	uint16 data;

	if (b == 0) {
		data = be16toh(*ctx->pp);
		ctx->pp++;
	} else {
		data = *(vp - ctx->offset[b]);
	}
	*vp = data;
}

static void
mag_expand(struct magctx *ctx)
{
	const uint8 *ap = ctx->flagA;
	const uint8 *bp = ctx->flagB;
	uint16 *vp = ctx->vram;
	uint32 blen = ctx->vwidth / 2;
	uint8 bbuf[blen];

	memset(bbuf, 0, blen);
	ctx->pp = ctx->pixel;

	for (uint y = 0; y < ctx->vheight; y++) {
		for (uint x = 0; x < blen; ) {
			uint8 a = *ap++;
			for (uint i = 0; i < 8; i++) {
				if ((a & 0x80)) {
					bbuf[x] ^= *bp++;
				}
				x++;
				a <<= 1;
			}
		}

		for (uint x = 0; x < blen; x++) {
			uint8 b = bbuf[x];
			// b には4ビットフラグが2個。
			mag_expand_b(ctx, vp++, b >> 4);
			mag_expand_b(ctx, vp++, b & 0x0f);
		}
	}
}

// 仮想 VRAM から img に展開。
static void
mag_expand_color16(struct magctx *ctx, struct image *img)
{
	const uint16 *vp = ctx->vram;
	const uint16 *vend = vp + ctx->vwidth * ctx->vheight;
	uint16 *d = (uint16 *)img->buf;

	for (; vp < vend; vp++) {
		uint16 data = *vp;

		uint c0 = (data >> 12);
		uint c1 = (data >>  8) & 0xf;
		uint c2 = (data >>  4) & 0xf;
		uint c3 =  data        & 0xf;
		*d++ = ctx->palette[c0];
		*d++ = ctx->palette[c1];
		*d++ = ctx->palette[c2];
		*d++ = ctx->palette[c3];
	}
}

// 仮想 VRAM から img に展開。
static void
mag_expand_color256(struct magctx *ctx, struct image *img)
{
	const uint16 *vp = ctx->vram;
	const uint16 *vend = vp + ctx->vwidth * ctx->vheight;
	uint16 *d = (uint16 *)img->buf;

	for (; vp < vend; vp++) {
		uint16 data = *vp;

		uint c0 = data >> 8;
		uint c1 = data & 0xff;
		*d++ = ctx->palette[c0];
		*d++ = ctx->palette[c1];
	}
}
