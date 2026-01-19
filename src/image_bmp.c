/* vi:set ts=4: */
/*
 * Copyright (C) 2024-2026 Tetsuya Isaki
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
// BMP 読み込み/書き出し
//

#include "sixelv.h"
#include "image_priv.h"
#include "image_bmp.h"
#include <err.h>
#include <string.h>

// ファイルヘッダ(共通)
typedef struct __packed {
	uint8  bfType[2];		// "BM"
	uint32 bfSize;			// ファイルサイズ
	uint32 bfReserved;
	uint32 bfOffBits;		// ファイル先頭から数えた画像データ開始位置
} BITMAPFILEHEADER;

typedef union {
	uint32 dib_size;
	BITMAPCOREHEADER bc;
	BITMAPINFOHEADER bi;
	BITMAPV4HEADER   bv4;
	BITMAPV5HEADER   bv5;
} DIBHEADER;

static void bmp_read_core_header(struct bmpctx *, const BITMAPCOREHEADER *);
static bool bmp_read_palette3(struct bmpctx *);
static bool raster_rgb1(struct bmpctx *, int);
static bool raster_rgb4(struct bmpctx *, int);
static bool raster_rgb8(struct bmpctx *, int);
static bool raster_rgb16(struct bmpctx *, int);
static bool raster_rgb24(struct bmpctx *, int);
static bool raster_rgb32(struct bmpctx *, int);
static void set_colormask(struct bmpctx *, uint32 *);
static bool raster_bitfield16(struct bmpctx *, int);
static bool raster_bitfield32(struct bmpctx *, int);
static bool raster_rle4(struct bmpctx *, int);
static bool raster_rle8(struct bmpctx *, int);
static bool raster_rle(struct bmpctx *, int, bool);
static uint8 extend_to8bit(const struct bmpctx *, uint32, uint);
static struct image *image_coloring(const struct image *);

// R8,G8,B8 を内部形式に変換。
static inline uint16
RGB888_to_ARGB16(uint8 r, uint8 g, uint8 b)
{
	r >>= 3;
	g >>= 3;
	b >>= 3;
	return (r << 10) | (g << 5) | b;
}

bool
image_bmp_match(FILE *fp, const struct diag *diag)
{
	uint8 magic[2];

	size_t n = fread(magic, sizeof(magic), 1, fp);
	if (n == 0) {
		Debug(diag, "%s: fread failed: %s", __func__, strerrno());
		return false;
	}

	if (magic[0] != 'B' || magic[1] != 'M') {
		return false;
	}

	return true;
}

struct image *
image_bmp_read(FILE *fp, const image_read_hint *hint, const struct diag *diag)
{
	BITMAPFILEHEADER hdr;
	DIBHEADER info;
	struct bmpctx bmp0;
	struct bmpctx *bmp;
	size_t n;

	bmp = &bmp0;
	memset(bmp, 0, sizeof(*bmp));
	bmp->fp = fp;

	// ファイルヘッダ(共通)。
	n = fread(&hdr, sizeof(hdr), 1, fp);
	if (n == 0) {
		warn("%s: fread(hdr) failed", __func__);
		return NULL;
	}

	uint32 offbits = le32toh(hdr.bfOffBits);

	// 次のヘッダは先頭の長さフィールドで区別する。
	uint8 *buf = (uint8 *)&info.dib_size;
	size_t len = sizeof(info.dib_size);
	n = fread(buf, len, 1, fp);
	if (n == 0) {
		Debug(diag, "%s: fread(dib_size) failed: %s", __func__, strerrno());
		return NULL;
	}
	uint32 dib_size = le32toh(info.dib_size);

	// 残りを読み込む。
	buf += sizeof(info.dib_size);
	len = dib_size - sizeof(info.dib_size);
	n = fread(buf, 1, len, fp);
	if (n < len) {
		Debug(diag, "%s: fread(remaining %zu bytes)=%zu: %s", __func__,
			len, n, strerrno());
		return NULL;
	}

	// ヘッダの内容を読み込む。
	bmp->bottom_up = true;
	if (dib_size == sizeof(BITMAPCOREHEADER)) {
		bmp_read_core_header(bmp, &info.bc);
	} else {
		bmp_read_info_header(bmp, &info.bi);
	}

	// デバッグ表示。
	if (diag_get_level(diag) >= 1) {
		bmp_print_debuginfo(bmp, diag, __func__, dib_size);
	}

	// 知らないヘッダなら表示までは頑張ってみるけど、終了。
	switch (dib_size) {
	 case sizeof(BITMAPCOREHEADER):
	 case sizeof(BITMAPINFOHEADER):
	 case sizeof(BITMAPV4HEADER):
	 case sizeof(BITMAPV5HEADER):
		break;
	 default:
		Debug(diag, "%s: Unknown header format (dib_size=%u)",
			__func__, dib_size);
		return NULL;
	}

	// 圧縮形式によってラスター処理関数を用意。
	switch (bmp->compression) {
	 case BI_RGB:
		if (bmp_select_raster_rgb(bmp) == false) {
			warnx("%s: BI_RGB but BitCount=%u not supported",
				__func__, bmp->bitcount);
			return NULL;
		}
		break;
	 case BI_RLE8:
		bmp->rasterop = raster_rle8;
		break;
	 case BI_RLE4:
		bmp->rasterop = raster_rle4;
		break;
	 case BI_BITFIELDS:
		switch (bmp->bitcount) {
		 case 16:	bmp->rasterop = raster_bitfield16;	break;
		 case 32:	bmp->rasterop = raster_bitfield32;	break;
		 default:
			warnx("%s: BI_BITFIELDS but BitCount=%u not supported",
				__func__, bmp->bitcount);
			return NULL;
		}
		break;

#if defined(USE_LIBJPEG) || defined(USE_STB_IMAGE)
	 case BI_JPEG:
		// 以降が JPEG 生データ。
# if defined(USE_LIBJPEG)
		return image_jpeg_read(fp, hint, diag);
# else
		return image_stb_read(fp, hint, diag);
# endif
#endif

#if defined(USE_LIBPNG) || defined(USE_STB_IMAGE)
	 case BI_PNG:
		// 以降が PNG 生データ。
# if defined(USE_LIBPNG)
		return image_png_read(fp, hint, diag);
# else
		return image_stb_read(fp, hint, diag);
# endif
#endif

	 default:
		warnx("%s: compression=%u not supported", __func__,
			bmp->compression);
		return NULL;
	}

	// 圧縮形式がビットフィールドならカラーマスクを加工する。
	// DIB=INFO なら、ヘッダ直後に R,G,B のマスクが各32ビットで配置される。
	// DIB=V4/V5 なら、ヘッダに含まれている。
	if (bmp->compression == BI_BITFIELDS) {
		uint32 maskbuf[3];
		if (dib_size == sizeof(BITMAPINFOHEADER)) {
			n = fread(maskbuf, sizeof(maskbuf), 1, fp);
			if (n == 0) {
				warn("%s: fread(colormask) failed", __func__);
				return NULL;
			}
		} else {
			// V4/V5 ヘッダ内の値は4バイトに整列してないため一旦コピーする。
			memcpy(maskbuf, &info.bv4.bv4RedMask, sizeof(maskbuf));
		}
		set_colormask(bmp, maskbuf);
		Debug(diag, "%s: RGB=%u:%u:%u", __func__,
			bmp->maskbits[0],
			bmp->maskbits[1],
			bmp->maskbits[2]);
	}

	// パレットは BiBitCount が 8 以下の時にある。
	if (bmp->bitcount <= 8) {
		bool r;
		if (dib_size == sizeof(BITMAPCOREHEADER)) {
			r = bmp_read_palette3(bmp);
		} else {
			r = bmp_read_palette4(bmp);
		}
		if (r == false) {
			warnx("%s: fread(palette) failed", __func__);
			return NULL;
		}
	}

	// ラスター開始位置。
	fseek(fp, offbits, SEEK_SET);

	// 直接内部形式にする。
	bmp->img = image_create(bmp->width, bmp->height, IMAGE_FMT_ARGB16);
	if (bmp->img == NULL) {
		return NULL;
	}

	// 全ラスターを展開。
	if (bmp_extract(bmp) == false) {
		image_free(bmp->img);
		bmp->img = NULL;
	}

	return bmp->img;
}

// CORE ヘッダから必要なパラメータを読み込む。
static void
bmp_read_core_header(struct bmpctx *bmp, const BITMAPCOREHEADER *info)
{
	bmp->width     = le16toh(info->bcWidth);
	bmp->height    = le16toh(info->bcHeight);
	bmp->bitcount  = le16toh(info->bcBitCount);
	bmp->bottom_up = true;
}

// INFO ヘッダから必要なパラメータを読み込む。(ICO からも使う)
void
bmp_read_info_header(struct bmpctx *bmp, const BITMAPINFOHEADER *info)
{
	bmp->width     = le32toh(info->biWidth);
	int  height    = le32toh(info->biHeight);
	if (height >= 0) {
		bmp->bottom_up = true;
	} else {
		height = -height;
		bmp->bottom_up = false;
	}
	bmp->height    = height;
	bmp->bitcount  = le32toh(info->biBitCount);
	bmp->compression = le32toh(info->biCompression);
	bmp->clrused   = le32toh(info->biClrUsed);
}

void
bmp_print_debuginfo(struct bmpctx *bmp, const struct diag *diag,
	const char *funcname, uint32 dib_size)
{
	static const char * const compstr[] = {
		"RGB",
		"RLE8",
		"RLE4",
		"Bitfield",
		"JPEG",
		"PNG",
	};
	char dibbuf[16];
	const char *hdrname;

	if (dib_size == 0) {
		// ICO から呼ぶ時は DIB の区別はない。
		dibbuf[0] = '\0';
	} else {
		if (dib_size == sizeof(BITMAPCOREHEADER)) {
			hdrname = "CORE";
		} else if (dib_size == sizeof(BITMAPINFOHEADER)) {
			hdrname = "INFO";
		} else if (dib_size == sizeof(BITMAPV4HEADER)) {
			hdrname = "V4";
		} else if (dib_size == sizeof(BITMAPV5HEADER)) {
			hdrname = "V5";
		} else {
			hdrname = "?";
		}
		snprintf(dibbuf, sizeof(dibbuf), " DIB=%s", hdrname);
	}

	diag_print(diag, "%s:%s width=%u height=%d %s", funcname,
		dibbuf, bmp->width, bmp->height,
		(bmp->bottom_up ? "bottom-to-top" : "top-to-bottom"));
	diag_print(diag, "%s: compression=%s bitcount=%u colorused=%u", funcname,
		(bmp->compression < countof(compstr) ? compstr[bmp->compression] : "?"),
		bmp->bitcount, bmp->clrused);
}

// BI_RGB のラスター処理関数を設定する。(ICO からも使う)
bool
bmp_select_raster_rgb(struct bmpctx *bmp)
{
	switch (bmp->bitcount) {
	 case  1:	bmp->rasterop = raster_rgb1;	break;
	 case  4:	bmp->rasterop = raster_rgb4;	break;
	 case  8:	bmp->rasterop = raster_rgb8;	break;
	 case 16:	bmp->rasterop = raster_rgb16;	break;
	 case 24:	bmp->rasterop = raster_rgb24;	break;
	 case 32:	bmp->rasterop = raster_rgb32;	break;
	 default:
		return false;
	}
	return true;
}

// パレットセクション (3バイト/パレット) を読み込む。npal はパレット数。
static bool
bmp_read_palette3(struct bmpctx *bmp)
{
	// パレット数は BiClrUsed。
	// BiClrUsed が 0 ならデフォルト (1 << BiBitCount) を意味する。
	uint npal = bmp->clrused ?: (1U << bmp->bitcount);

	uint8 palbuf[npal * 3];
	size_t n = fread(palbuf, 3, npal, bmp->fp);
	if (n < npal) {
		return false;
	}
	const uint8 *s = palbuf;
	for (uint i = 0; i < npal; i++) {
		uint8 b = *s++;
		uint8 g = *s++;
		uint8 r = *s++;
		bmp->palette[i] = RGB888_to_ARGB16(r, g, b);
	}

	return true;
}

// パレットセクション (4バイト/パレット) を読み込む。npal はパレット数。
bool
bmp_read_palette4(struct bmpctx *bmp)
{
	// パレット数は BiClrUsed。
	// BiClrUsed が 0 ならデフォルト (1 << BiBitCount) を意味する。
	uint npal = bmp->clrused ?: (1U << bmp->bitcount);

	uint32 palbuf[npal];
	size_t n = fread(palbuf, 4, npal, bmp->fp);
	if (n < npal) {
		return false;
	}
	for (uint i = 0; i < npal; i++) {
		uint xrgb = le32toh(palbuf[i]);
		uint8 r = (xrgb >> 16) & 0xff;
		uint8 g = (xrgb >>  8) & 0xff;
		uint8 b = (xrgb      ) & 0xff;
		bmp->palette[i] = RGB888_to_ARGB16(r, g, b);
	}

	return true;
}

// 全ラスターを展開する。
bool
bmp_extract(struct bmpctx *bmp)
{
	if (bmp->bottom_up) {
		for (int y = bmp->height - 1; y >= 0; y--) {
			if (__predict_false((*bmp->rasterop)(bmp, y) == false)) {
				return false;
			}
		}
	} else {
		for (int y = 0; y < bmp->height; y++) {
			if (__predict_false((*bmp->rasterop)(bmp, y) == false)) {
				return false;
			}
		}
	}
	return true;
}

static bool
raster_rgb1(struct bmpctx *bmp, int y)
{
	struct image *img = bmp->img;
	// 1バイトに8ピクセル収まっているのを4バイト単位に切り上げる。
	uint bytewidth = howmany(img->width, 8);
	uint bmpstride = roundup(bytewidth, 4);
	uint8 srcbuf[bmpstride];

	size_t n = fread(srcbuf, 1, bmpstride, bmp->fp);
	uint width = n * 8;
	if (width > img->width) {
		width = img->width;
	}

	const uint8 *s = srcbuf;
	uint16 *d = (uint16 *)img->buf + img->width * y;
	uint8 bits = 0;
	for (uint x = 0; x < width; x++) {
		// 左のピクセルが MSB 側。
		if (__predict_false((x % 8) == 0)) {
			bits = *s++;
		}
		uint idx = (bits & 0x80) ? 1 : 0;
		bits <<= 1;
		*d++ = bmp->palette[idx];
	}

	// 構わず成功扱いにしておく。
	return true;
}

static bool
raster_rgb4(struct bmpctx *bmp, int y)
{
	struct image *img = bmp->img;
	// 1バイトに2ピクセル収まっているのを4バイト単位に切り上げる。
	uint bytewidth = howmany(img->width, 2);
	uint bmpstride = roundup(bytewidth, 4);
	uint8 srcbuf[bmpstride];

	size_t n = fread(srcbuf, 1, bmpstride, bmp->fp);
	uint width = n * 2;
	if (width > img->width) {
		width = img->width;
	}

	const uint8 *s = srcbuf;
	uint16 *d = (uint16 *)img->buf + img->width * y;
	// 偶数で回せるだけ回す。
	uint width2 = width & ~1U;
	for (uint x = 0; x < width2; x += 2) {
		// 左のピクセルが上位ニブル側。
		uint32 packed = *s++;
		uint32 h = packed >> 4;
		uint32 l = packed & 0xf;
		*d++ = bmp->palette[h];
		*d++ = bmp->palette[l];
	}
	// 奇数ならもう1ピクセル。
	if ((width & 1)) {
		uint32 packed = *s++;
		uint32 h = packed >> 4;
		*d++ = bmp->palette[h];
	}

	// 構わず成功扱いにしておく。
	return true;
}

static bool
raster_rgb8(struct bmpctx *bmp, int y)
{
	struct image *img = bmp->img;
	uint bmpstride = roundup(img->width, 4);
	uint8 srcbuf[bmpstride];

	size_t n = fread(srcbuf, 1, bmpstride, bmp->fp);
	uint width = n;
	if (width > img->width) {
		width = img->width;
	}

	const uint8 *s = srcbuf;
	uint16 *d = (uint16 *)img->buf + img->width * y;
	for (uint x = 0; x < width; x++) {
		uint8 idx = *s++;
		*d++ = bmp->palette[idx];
	}

	// 構わず成功扱いにしておく。
	return true;
}

static bool
raster_rgb16(struct bmpctx *bmp, int y)
{
	struct image *img = bmp->img;
	uint bmpstride = roundup(img->width * 2, 4);
	uint16 srcbuf[bmpstride / 2];

	size_t n = fread(srcbuf, 2, bmpstride / 2, bmp->fp);
	uint width = n;
	if (width > img->width) {
		width = img->width;
	}

	const uint16 *s = srcbuf;
	uint16 *d = (uint16 *)img->buf + img->width * y;
	for (uint x = 0; x < width; x++) {
		// 16ビットは RGB555 形式らしいので、エンディアンを揃えるだけ。
		uint16 cc = le16toh(*s++);
		*d++ = cc;
	}

	// 構わず成功扱いにしておく。
	return true;
}

static bool
raster_rgb24(struct bmpctx *bmp, int y)
{
	struct image *img = bmp->img;
	uint bmpstride = roundup(img->width * 3, 4);
	uint8 srcbuf[bmpstride];

	size_t n = fread(srcbuf, 1, bmpstride, bmp->fp);
	uint width = n / 3;
	if (width > img->width) {
		width = img->width;
	}

	const uint8 *s = srcbuf;
	uint16 *d = (uint16 *)img->buf + img->width * y;
	for (uint x = 0; x < width; x++) {
		// BMP はメモリ上 B,G,R の順。
		uint8 b = *s++;
		uint8 g = *s++;
		uint8 r = *s++;
		*d++ = RGB888_to_ARGB16(r, g, b);
	}

	// 構わず成功扱いにしておく。
	return true;
}

static bool
raster_rgb32(struct bmpctx *bmp, int y)
{
	struct image *img = bmp->img;
	uint32 srcbuf[img->width];

	size_t n = fread(srcbuf, 4, img->width, bmp->fp);
	uint width = n;

	const uint32 *s = srcbuf;
	uint16 *d = (uint16 *)img->buf + img->width * y;
	for (uint x = 0; x < width; x++) {
		// BMP はメモリ上 B,G,R,0 の順。
		uint32 xrgb = le32toh(*s++);
		uint8 r = (xrgb >> 16) & 0xff;
		uint8 g = (xrgb >>  8) & 0xff;
		uint8 b = (xrgb      ) & 0xff;
		*d++ = RGB888_to_ARGB16(r, g, b);
	}

	// 構わず成功扱いにしておく。
	return true;
}

// カラーマスクブロックから必要な値を計算しておく。
// maskbuf は LE のままのバッファを渡す。
static void
set_colormask(struct bmpctx *bmp, uint32 *maskbuf)
{
	// マスクブロックは R,G,B の順に32ビットずつ。
	for (uint i = 0; i < 3; i++) {
		uint32 mask = le32toh(maskbuf[i]);
		uint32 offset = __builtin_ctz(mask);
		// マスクビットは連続してるはず。
		uint32 bits = __builtin_popcount(mask);

		bmp->mask[i] = mask;
		bmp->offset[i] = offset;
		bmp->maskbits[i] = bits;
	}
}

static bool
raster_bitfield16(struct bmpctx *bmp, int y)
{
	struct image *img = bmp->img;
	uint bmpstride = roundup(img->width * 2, 4);
	uint16 srcbuf[bmpstride / 2];

	size_t n = fread(srcbuf, 2, bmpstride / 2, bmp->fp);
	uint width = n;
	if (width > img->width) {
		width = img->width;
	}

	const uint16 *s = srcbuf;
	uint16 *d = (uint16 *)img->buf + img->width * y;
	for (uint x = 0; x < width; x++) {
		uint16 data = le16toh(*s++);
		uint8 r = extend_to8bit(bmp, data, 0);
		uint8 g = extend_to8bit(bmp, data, 1);
		uint8 b = extend_to8bit(bmp, data, 2);
		*d++ = RGB888_to_ARGB16(r, g, b);
	}

	// 構わず成功扱いにしておく。
	return true;
}

static bool
raster_bitfield32(struct bmpctx *bmp, int y)
{
	struct image *img = bmp->img;
	uint32 srcbuf[img->width];

	size_t n = fread(srcbuf, 4, img->width, bmp->fp);
	uint width = n;

	const uint32 *s = srcbuf;
	uint16 *d = (uint16 *)img->buf + img->width * y;
	for (uint x = 0; x < width; x++) {
		uint32 data = le32toh(*s++);
		uint8 r = extend_to8bit(bmp, data, 0);
		uint8 g = extend_to8bit(bmp, data, 1);
		uint8 b = extend_to8bit(bmp, data, 2);
		*d++ = RGB888_to_ARGB16(r, g, b);
	}

	// 構わず成功扱いにしておく。
	return true;
}

// ビットフィールドのデータから指定のチャンネルの 8ビット値を取り出す。
// i はチャンネルインデックス (0=R, 1=G, 2=B)
static uint8
extend_to8bit(const struct bmpctx *bmp, uint32 data, uint i)
{
	uint32 v = (data & bmp->mask[i]) >> bmp->offset[i];

	// 足りないところは繰り返して重ねる。
	switch (bmp->maskbits[i]) {
	 case 1:
		v = v ? 0xff : 0;
		break;
	 case 2:
		v = (v << 6) | (v << 4) | (v << 2);
		break;
	 case 3:
		v = (v << 5) | (v << 2) | (v >> 1);
		break;
	 case 4:
		v = (v << 4) | v;
		break;
	 case 5:
		v = (v << 3) | (v >> 2);
		break;
	 case 6:
		v = (v << 2) | (v >> 4);
		break;
	 case 7:
		v = (v << 1) | (v >> 6);
		break;
	 case 8:
		break;
	 default:
		v >>= bmp->maskbits[i] - 8;
		break;
	}
	return v;
}

static bool
raster_rle4(struct bmpctx *bmp, int y)
{
	return raster_rle(bmp, y, true/*4*/);
}

static bool
raster_rle8(struct bmpctx *bmp, int y)
{
	return raster_rle(bmp, y, false/*8*/);
}

// RLE4,RLE8 の共通部分。
// ここはレアケースなので性能よりもコード量を減らす。
static bool
raster_rle(struct bmpctx *bmp, int y, bool rle4)
{
	struct image *img = bmp->img;

	uint16 *d = (uint16 *)img->buf + img->width * y;
	for (;;) {
		int count = fgetc(bmp->fp);
		if (__predict_false(count < 0)) {
			return false;
		}
		if (count == 0) {
			// エスケープ。
			// 00,00: 行末
			// 00,01: ビットマップの終端
			// 00,02,dx,dy: 移動
			// 00,nn: 絶対モード
			int cmd = fgetc(bmp->fp);
			if (__predict_false(cmd < 0)) {
				return false;
			}
			if (cmd <= 1) {
				break;
			} else if (__predict_false(cmd == 2)) {
				// Not supported
				return false;
			} else {
				// 絶対モードでは以後 nn ピクセル分の生データが並ぶ。
				// その後偶数に整列。
				if (rle4) {
					int cc = 0;
					for (uint i = 0; i < cmd; i++) {
						if ((i & 1) == 0) {
							cc = fgetc(bmp->fp);
							if (__predict_false(cc < 0)) {
								return false;
							}
							*d++ = bmp->palette[cc >> 4];
						} else {
							*d++ = bmp->palette[cc & 0xf];
						}
					}
				} else {
					for (uint i = 0; i < cmd; i++) {
						int cc = fgetc(bmp->fp);
						if (__predict_false(cc < 0)) {
							return false;
						}
						*d++ = bmp->palette[cc];
					}
				}
				int pos = ftell(bmp->fp);
				if ((pos & 1)) {
					fgetc(bmp->fp);
				}
			}
		} else {
			// 圧縮ブロック。
			// count は連続する展開後のピクセル数。
			int cc = fgetc(bmp->fp);
			if (__predict_false(cc < 0)) {
				return false;
			}
			if (rle4) {
				uint16 data[2];
				data[0] = bmp->palette[cc >> 4];
				data[1] = bmp->palette[cc & 0xf];
				for (uint i = 0; i < count; i++) {
					*d++ = data[(i & 1)];
				}
			} else {
				uint16 data = bmp->palette[cc];
				for (uint i = 0; i < count; i++) {
					*d++ = data;
				}
			}
		}
	}

	// BI_RLE(4|8) の各ラスターは2バイト境界なので、自然に整列している。

	return true;
}

// image を BMP 形式で fp に出力する。
bool
image_bmp_write(FILE *fp, const struct image *srcimg, const struct diag *diag)
{
	struct image *img;
	BITMAPFILEHEADER hdr;
	BITMAPINFOHEADER info;
	uint32 istride;	// 入力ストライド
	uint32 ostride;	// 出力ストライド (4バイトの倍数でなければならない)
	uint32 padding;
	uint32 datasize;
	bool rv = false;

	if (srcimg->format == IMAGE_FMT_AIDX16) {
		// インデックスカラーを RGB に戻す。
		img = image_coloring(srcimg);
		if (img == NULL) {
			Debug(diag, "%s: image_coloring failed: %s", __func__, strerrno());
			return false;
		}
	} else if (srcimg->format == IMAGE_FMT_RGB24) {
		img = UNCONST(srcimg);
	} else {
		Debug(diag, "%s: Unsupported format: %u", __func__, srcimg->format);
		return false;
	}

	istride = image_get_stride(img);
	ostride = roundup(istride, 4);
	padding = ostride - istride;
	datasize = ostride * img->height;
	uint8 dst[ostride];

	// ファイルヘッダ。
	memset(&hdr, 0, sizeof(hdr));
	hdr.bfType[0] = 'B';
	hdr.bfType[1] = 'M';
	hdr.bfOffBits = htole32(sizeof(hdr) + sizeof(info));

	// 情報ヘッダ。
	memset(&info, 0, sizeof(info));
	info.biSize			= htole32(sizeof(info));
	info.biWidth		= htole32(img->width);
	info.biHeight		= htole32(img->height);
	info.biPlanes		= htole16(1);
	info.biBitCount		= htole16(24);		// XXX 24bpp 固定
	info.biCompression	= htole32(BI_RGB);
	info.biSizeImage	= htole32(datasize);
	info.biXPelsPerMeter= htole32(3780);	// 96dpi
	info.biYPelsPerMeter= htole32(3780);	// 96dpi

	if (fwrite(&hdr, sizeof(hdr), 1, fp) < 1) {
		Debug(diag, "%s: fwrite(hdr) failed: %s", __func__, strerrno());
		goto done;
	}

	if (fwrite(&info, sizeof(info), 1, fp) < 1) {
		Debug(diag, "%s: fwrite(info) failed: %s", __func__, strerrno());
		goto done;
	}

	if (padding > 0) {
		memset(&dst[ostride - padding], 0, padding);
	}
	for (int y = img->height - 1; y >= 0; y--) {
		const uint8 *s = img->buf + istride * y;
		uint8 *d = &dst[0];
		for (int x = 0; x < img->width; x++) {
			// struct image は R,G,B 順だが BMP は B,G,R 順。
			uint8 r, g, b;
			r = *s++;
			g = *s++;
			b = *s++;
			*d++ = b;
			*d++ = g;
			*d++ = r;
		}
		fwrite(dst, 1, ostride, fp);
	}

	rv = true;
 done:
	if (img != srcimg) {
		image_free(img);
	}
	return rv;
}

// AIDX16 形式の srcimg をパレットで着色した画像を返す。
// 透過色は #000000 (黒) になる。
static struct image *
image_coloring(const struct image *srcimg)
{
	struct image *dstimg;

	assert(srcimg->format == IMAGE_FMT_AIDX16);
	assert(srcimg->palette != NULL);

	dstimg = image_create(srcimg->width, srcimg->height, IMAGE_FMT_RGB24);
	if (dstimg == NULL) {
		return NULL;
	}

	const uint16 *s = (const uint16 *)srcimg->buf;
	uint8 *d = dstimg->buf;
	for (uint y = 0, yend = srcimg->height; y < yend; y++) {
		for (uint x = 0, xend = srcimg->width; x < xend; x++) {
			uint32 colorcode = *s++;
			ColorRGB c;
			if (colorcode < srcimg->palette_count) {
				c = srcimg->palette[colorcode];
			} else {
				c.u32 = 0;
			}
			*d++ = c.r;
			*d++ = c.g;
			*d++ = c.b;
		}
	}

	return dstimg;
}
