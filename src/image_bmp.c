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
#include <string.h>

// 圧縮方式
#define BI_RGB			(0)
#define BI_RLE8			(1)
#define BI_RLE4			(2)
#define BI_BITFIELDS	(3)
#define BI_JPEG			(4)
#define BI_PNG			(5)

// ファイルヘッダ(共通)
typedef struct __packed {
	uint8  bfType[2];		// "BM"
	uint32 bfSize;			// ファイルサイズ
	uint32 bfReserved;
	uint32 bfOffBits;		// ファイル先頭から数えた画像データ開始位置
} BITMAPFILEHEADER;

// OS/2 形式の DIB
typedef struct __packed {
	uint32 bcSize;			// ヘッダサイズ (12)
	uint16 bcWidth;			// 画像のピクセル幅
	uint16 bcHeight;		// 画像のピクセル高さ
	uint16 bcPlanes;		// プレーン数(1)
	uint16 bcBitCount;		// bits per pixel
} BITMAPCOREHEADER;

// Windows 形式の DIB
typedef struct __packed {
	uint32 biSize;			// ヘッダサイズ (40)
	uint32 biWidth;			// 画像のピクセル幅
	uint32 biHeight;		// 画像のピクセル高さ (負なら上から下)
	uint16 biPlanes;		// プレーン数 (1)
	uint16 biBitCount;		// bits per pixel
	uint32 biCompression;	// 圧縮方式
	uint32 biSizeImage;		// データ部のバイト数
	uint32 biXPelsPerMeter;	// X 解像度
	uint32 biYPelsPerMeter;	// Y 解像度
	uint32 biClrUsed;
	uint32 biClrImportant;
} BITMAPINFOHEADER;

// Windows (V4) 形式の DIB
typedef struct __packed {
	uint32 bv4Size;			// ヘッダサイズ (108)
	uint32 bv4Width;		// 画像のピクセル幅
	uint32 bv4Height;		// 画像のピクセル高さ
	uint16 bv4Planes;		// プレーン数 (1)
	uint16 bv4BitCount;		// bits per pixel
	uint32 bv4Compression;	// 圧縮方式
	uint32 bv4SizeImage;
	uint32 bv4XPelsPerMeter;
	uint32 bv4YPelsPerMeter;
	uint32 bv4ClrUsed;
	uint32 bv4ClrImportant;
	uint32 bv4RedMask;		// BI_BITFIELDS のマスク
	uint32 bv4GreenMask;	// BI_BITFIELDS のマスク
	uint32 bv4BlueMask;		// BI_BITFIELDS のマスク
	uint32 bv4AlphaMask;	// BI_BITFIELDS のマスク
	uint32 bv4CSType;		// 色空間 (0)
	uint32 bv4Endpoints[3 * 3];
	uint32 bv4GammaRed;
	uint32 bv4GammaGreen;
	uint32 bv4GammaBlue;
} BITMAPV4HEADER;

// Windows (V5) 形式の DIB
typedef struct __packed {
	uint32 bv5Size;
	uint32 bv5Width;
	uint32 bv5Height;
	uint16 bv5Planes;
	uint16 bv5BitCount;
	uint32 bv5Compression;
	uint32 bv5SizeImage;
	uint32 bv5XPelsPerMeter;
	uint32 bv5YPelsPerMeter;
	uint32 bv5ClrUsed;
	uint32 bv5ClrImportant;
	uint32 bv5RedMask;
	uint32 bv5GreenMask;
	uint32 bv5BlueMask;
	uint32 bv5AlphaMask;
	uint32 bv5CSType;
	uint32 bv5Endpoints[3 * 3];
	uint32 bv5GammaRed;
	uint32 bv5GammaGreen;
	uint32 bv5GammaBlue;
	uint32 bv5Intent;
	uint32 bv5ProfileData;
	uint32 bv5ProfileSize;
	uint32 bv5Reserved;
} BITMAPV5HEADER;

typedef union {
	uint32 dib_size;
	BITMAPCOREHEADER bc;
	BITMAPINFOHEADER bi;
	BITMAPV4HEADER   bv4;
	BITMAPV5HEADER   bv5;
} DIBHEADER;

struct bmpctx {
	FILE *fp;
	struct image *img;

	// BI_BITFIELDS、各 R,G,B の順。
	uint32 mask[3];		// マスクビット (例えば 0x00ff0000)
	uint offset[3];		// マスクの右側のゼロの数
	uint maskbits[3];	// マスクのビット数

	uint16 palette[256];
};

typedef bool (*rasterop_t)(struct bmpctx *, int);

static bool read_palette3(struct bmpctx *, uint);
static bool read_palette4(struct bmpctx *, uint);
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
	struct bmpctx ctx0;
	struct bmpctx *ctx;
	rasterop_t rasterop;
	size_t n;

	ctx = &ctx0;
	memset(ctx, 0, sizeof(*ctx));
	ctx->fp = fp;

	// ファイルヘッダ(共通)。
	n = fread(&hdr, sizeof(hdr), 1, fp);
	if (n == 0) {
		Debug(diag, "%s: fread(hdr) failed: %s", __func__, strerrno());
		return NULL;
	}

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

	// ヘッダの内容を一旦変数に読み込む。エンディアンを吸収するためと、
	// COREHEADER と INFOHEADER はメンバが似てるけど位置/サイズが違ったりする。
	uint32 width;
	int32  height;
	bool bottom_up = true;
	uint32 bitcount;
	uint32 compression = 0;
	uint32 clrused = 0;
	if (dib_size == sizeof(BITMAPCOREHEADER)) {
		width    = le16toh(info.bc.bcWidth);
		height   = le16toh(info.bc.bcHeight);
		bitcount = le16toh(info.bc.bcBitCount);
	} else {
		width    = le32toh(info.bi.biWidth);
		height   = le32toh(info.bi.biHeight);
		if (height < 0) {
			height = -height;
			bottom_up = false;
		}
		bitcount = le32toh(info.bi.biBitCount);
		compression = le32toh(info.bi.biCompression);
		clrused  = le32toh(info.bi.biClrUsed);
	}

	if (diag_get_level(diag) >= 1) {
		static const char * const compstr[6] = {
			"RGB",
			"RLE8",
			"RLE4",
			"Bitfield",
			"JPEG",
			"PNG",
		};
		const char *hdrname;

		if (dib_size == sizeof(BITMAPCOREHEADER)) {
			hdrname = "CORE";
		} else if (dib_size == sizeof(BITMAPINFOHEADER)) {
			hdrname = "INFO";
		} else if (dib_size == sizeof(BITMAPV4HEADER)) {
			hdrname = "V4";
		} else if (dib_size == sizeof(BITMAPV5HEADER)) {
			hdrname = "V5";
		} else {
			hdrname = "unknown";
		}
		Debug(diag, "%s: DIB=%s width=%u height=%d %s", __func__,
			hdrname, width, height,
			(bottom_up ? "bottom-to-top" : "top-to-bottom"));
		Debug(diag, "%s: compression=%s bitcount=%u colorused=%u", __func__,
			(compression < 6 ? compstr[compression] : "?"),
			bitcount, clrused);
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
	switch (compression) {
	 case BI_RGB:
		switch (bitcount) {
		 case  1:	rasterop = raster_rgb1;		break;
		 case  4:	rasterop = raster_rgb4;		break;
		 case  8:	rasterop = raster_rgb8;		break;
		 case 16:	rasterop = raster_rgb16;	break;
		 case 24:	rasterop = raster_rgb24;	break;
		 case 32:	rasterop = raster_rgb32;	break;
		 default:
			Debug(diag, "%s: BI_RGB but BitCount=%u not supported",
				__func__, bitcount);
			return false;
		}
		break;
	 case BI_RLE8:
		rasterop = raster_rle8;
		break;
	 case BI_RLE4:
		rasterop = raster_rle4;
		break;
	 case BI_BITFIELDS:
		switch (bitcount) {
		 case 16:	rasterop = raster_bitfield16;	break;
		 case 32:	rasterop = raster_bitfield32;	break;
		 default:
			Debug(diag, "%s: BI_BITFIELDS but BitCount=%u not supported",
				__func__, bitcount);
			return false;
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
		Debug(diag, "%s: compression=%u not supported", __func__,
			compression);
		return NULL;
	}

	// 直接内部形式にする。
	ctx->img = image_create(width, height, IMAGE_FMT_ARGB16);
	if (ctx->img == NULL) {
		return NULL;
	}

	// 圧縮形式がビットフィールドならカラーマスクを加工する。
	// DIB=INFO なら、ヘッダ直後に R,G,B のマスクが各32ビットで配置される。
	// DIB=V4/V5 なら、ヘッダに含まれている。
	if (compression == BI_BITFIELDS) {
		uint32 maskbuf[3];
		if (dib_size == sizeof(BITMAPINFOHEADER)) {
			n = fread(maskbuf, sizeof(maskbuf[0]), countof(maskbuf), fp);
			if (n < countof(maskbuf)) {
				Debug(diag, "%s: fread(colormask) failed: %s",
					__func__, strerrno());
				goto abort;
			}
		} else {
			// V4/V5 ヘッダ内の値は4バイトに整列してないため一旦コピーする。
			memcpy(maskbuf, &info.bv4.bv4RedMask, sizeof(maskbuf));
		}
		set_colormask(ctx, maskbuf);
		Debug(diag, "%s: RGB=%u:%u:%u", __func__,
			ctx->maskbits[0],
			ctx->maskbits[1],
			ctx->maskbits[2]);
	}

	// パレットは BiBitCount が 8 以下の時にある。
	if (bitcount <= 8) {
		// パレット数は BiClrUsed。
		// BiClrUsed が 0 ならデフォルト (1 << BiBitCount) を意味する。
		uint npal = clrused ?: (1U << bitcount);

		bool r;
		if (dib_size == sizeof(BITMAPCOREHEADER)) {
			r = read_palette3(ctx, npal);
		} else {
			r = read_palette4(ctx, npal);
		}
		if (r == false) {
			Debug(diag, "%s: fread(palette) failed: %s", __func__, strerrno());
			goto abort;
		}
	}

	// ラスター開始位置。
	uint32 offbits = le32toh(hdr.bfOffBits);
	fseek(fp, offbits, SEEK_SET);

	// ラスターごとに展開。
	if (bottom_up) {
		for (int y = height - 1; y >= 0; y--) {
			if (__predict_false((*rasterop)(ctx, y) == false)) {
				goto abort;
			}
		}
	} else {
		for (int y = 0; y < height; y++) {
			if (__predict_false((*rasterop)(ctx, y) == false)) {
				goto abort;
			}
		}
	}

	return ctx->img;

 abort:
	image_free(ctx->img);
	return NULL;
}

// パレットセクション (3バイト/パレット) を読み込む。npal はパレット数。
static bool
read_palette3(struct bmpctx *ctx, uint npal)
{
	uint8 palbuf[npal * 3];

	size_t n = fread(palbuf, 3, npal, ctx->fp);
	if (n < npal) {
		return false;
	}
	const uint8 *s = palbuf;
	for (uint i = 0; i < npal; i++) {
		uint8 b = *s++;
		uint8 g = *s++;
		uint8 r = *s++;
		ctx->palette[i] = RGB888_to_ARGB16(r, g, b);
	}

	return true;
}

// パレットセクション (4バイト/パレット) を読み込む。npal はパレット数。
static bool
read_palette4(struct bmpctx *ctx, uint npal)
{
	uint32 palbuf[npal];

	size_t n = fread(palbuf, 4, npal, ctx->fp);
	if (n < npal) {
		return false;
	}
	for (uint i = 0; i < npal; i++) {
		uint xrgb = le32toh(palbuf[i]);
		uint8 r = (xrgb >> 16) & 0xff;
		uint8 g = (xrgb >>  8) & 0xff;
		uint8 b = (xrgb      ) & 0xff;
		ctx->palette[i] = RGB888_to_ARGB16(r, g, b);
	}

	return true;
}

static bool
raster_rgb1(struct bmpctx *ctx, int y)
{
	struct image *img = ctx->img;
	// 1バイトに8ピクセル収まっているのを4バイト単位に切り上げる。
	uint bytewidth = howmany(img->width, 8);
	uint bmpstride = roundup(bytewidth, 4);
	uint8 srcbuf[bmpstride];

	size_t n = fread(srcbuf, 1, bmpstride, ctx->fp);
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
		*d++ = ctx->palette[idx];
	}

	// 構わず成功扱いにしておく。
	return true;
}

static bool
raster_rgb4(struct bmpctx *ctx, int y)
{
	struct image *img = ctx->img;
	// 1バイトに2ピクセル収まっているのを4バイト単位に切り上げる。
	uint bytewidth = howmany(img->width, 2);
	uint bmpstride = roundup(bytewidth, 4);
	uint8 srcbuf[bmpstride];

	size_t n = fread(srcbuf, 1, bmpstride, ctx->fp);
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
		*d++ = ctx->palette[h];
		*d++ = ctx->palette[l];
	}
	// 奇数ならもう1ピクセル。
	if ((width & 1)) {
		uint32 packed = *s++;
		uint32 h = packed >> 4;
		*d++ = ctx->palette[h];
	}

	// 構わず成功扱いにしておく。
	return true;
}

static bool
raster_rgb8(struct bmpctx *ctx, int y)
{
	struct image *img = ctx->img;
	uint bmpstride = roundup(img->width, 4);
	uint8 srcbuf[bmpstride];

	size_t n = fread(srcbuf, 1, bmpstride, ctx->fp);
	uint width = n;
	if (width > img->width) {
		width = img->width;
	}

	const uint8 *s = srcbuf;
	uint16 *d = (uint16 *)img->buf + img->width * y;
	for (uint x = 0; x < width; x++) {
		uint8 idx = *s++;
		*d++ = ctx->palette[idx];
	}

	// 構わず成功扱いにしておく。
	return true;
}

static bool
raster_rgb16(struct bmpctx *ctx, int y)
{
	struct image *img = ctx->img;
	uint bmpstride = roundup(img->width * 2, 4);
	uint16 srcbuf[bmpstride / 2];

	size_t n = fread(srcbuf, 2, bmpstride / 2, ctx->fp);
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
raster_rgb24(struct bmpctx *ctx, int y)
{
	struct image *img = ctx->img;
	uint bmpstride = roundup(img->width * 3, 4);
	uint8 srcbuf[bmpstride];

	size_t n = fread(srcbuf, 1, bmpstride, ctx->fp);
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
raster_rgb32(struct bmpctx *ctx, int y)
{
	struct image *img = ctx->img;
	uint32 srcbuf[img->width];

	size_t n = fread(srcbuf, 4, img->width, ctx->fp);
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
set_colormask(struct bmpctx *ctx, uint32 *maskbuf)
{
	// マスクブロックは R,G,B の順に32ビットずつ。
	for (uint i = 0; i < 3; i++) {
		uint32 mask = le32toh(maskbuf[i]);
		uint32 offset = __builtin_ctz(mask);
		// マスクビットは連続してるはず。
		uint32 bits = __builtin_popcount(mask);

		ctx->mask[i] = mask;
		ctx->offset[i] = offset;
		ctx->maskbits[i] = bits;
	}
}

static bool
raster_bitfield16(struct bmpctx *ctx, int y)
{
	struct image *img = ctx->img;
	uint bmpstride = roundup(img->width * 2, 4);
	uint16 srcbuf[bmpstride / 2];

	size_t n = fread(srcbuf, 2, bmpstride / 2, ctx->fp);
	uint width = n;
	if (width > img->width) {
		width = img->width;
	}

	const uint16 *s = srcbuf;
	uint16 *d = (uint16 *)img->buf + img->width * y;
	for (uint x = 0; x < width; x++) {
		uint16 data = le16toh(*s++);
		uint8 r = extend_to8bit(ctx, data, 0);
		uint8 g = extend_to8bit(ctx, data, 1);
		uint8 b = extend_to8bit(ctx, data, 2);
		*d++ = RGB888_to_ARGB16(r, g, b);
	}

	// 構わず成功扱いにしておく。
	return true;
}

static bool
raster_bitfield32(struct bmpctx *ctx, int y)
{
	struct image *img = ctx->img;
	uint32 srcbuf[img->width];

	size_t n = fread(srcbuf, 4, img->width, ctx->fp);
	uint width = n;

	const uint32 *s = srcbuf;
	uint16 *d = (uint16 *)img->buf + img->width * y;
	for (uint x = 0; x < width; x++) {
		uint32 data = le32toh(*s++);
		uint8 r = extend_to8bit(ctx, data, 0);
		uint8 g = extend_to8bit(ctx, data, 1);
		uint8 b = extend_to8bit(ctx, data, 2);
		*d++ = RGB888_to_ARGB16(r, g, b);
	}

	// 構わず成功扱いにしておく。
	return true;
}

// ビットフィールドのデータから指定のチャンネルの 8ビット値を取り出す。
// i はチャンネルインデックス (0=R, 1=G, 2=B)
static uint8
extend_to8bit(const struct bmpctx *ctx, uint32 data, uint i)
{
	uint32 v = (data & ctx->mask[i]) >> ctx->offset[i];

	// 足りないところは繰り返して重ねる。
	switch (ctx->maskbits[i]) {
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
		v >>= ctx->maskbits[i] - 8;
		break;
	}
	return v;
}

static bool
raster_rle4(struct bmpctx *ctx, int y)
{
	return raster_rle(ctx, y, true/*4*/);
}

static bool
raster_rle8(struct bmpctx *ctx, int y)
{
	return raster_rle(ctx, y, false/*8*/);
}

// RLE4,RLE8 の共通部分。
// ここはレアケースなので性能よりもコード量を減らす。
static bool
raster_rle(struct bmpctx *ctx, int y, bool rle4)
{
	struct image *img = ctx->img;

	uint16 *d = (uint16 *)img->buf + img->width * y;
	for (;;) {
		int count = fgetc(ctx->fp);
		if (__predict_false(count < 0)) {
			return false;
		}
		if (count == 0) {
			// エスケープ。
			// 00,00: 行末
			// 00,01: ビットマップの終端
			// 00,02,dx,dy: 移動
			// 00,nn: 絶対モード
			int cmd = fgetc(ctx->fp);
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
							cc = fgetc(ctx->fp);
							if (__predict_false(cc < 0)) {
								return false;
							}
							*d++ = ctx->palette[cc >> 4];
						} else {
							*d++ = ctx->palette[cc & 0xf];
						}
					}
				} else {
					for (uint i = 0; i < cmd; i++) {
						int cc = fgetc(ctx->fp);
						if (__predict_false(cc < 0)) {
							return false;
						}
						*d++ = ctx->palette[cc];
					}
				}
				int pos = ftell(ctx->fp);
				if ((pos & 1)) {
					fgetc(ctx->fp);
				}
			}
		} else {
			// 圧縮ブロック。
			// count は連続する展開後のピクセル数。
			int cc = fgetc(ctx->fp);
			if (__predict_false(cc < 0)) {
				return false;
			}
			if (rle4) {
				uint16 data[2];
				data[0] = ctx->palette[cc >> 4];
				data[1] = ctx->palette[cc & 0xf];
				for (uint i = 0; i < count; i++) {
					*d++ = data[(i & 1)];
				}
			} else {
				uint16 data = ctx->palette[cc];
				for (uint i = 0; i < count; i++) {
					*d++ = data;
				}
			}
		}
	}

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
