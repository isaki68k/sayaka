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
	uint32 biCompression;	// 圧縮方式 (BI_RGB = 0)
#define BI_RGB	(0)
	uint32 biSizeImage;		// データ部のバイト数
	uint32 biXPelsPerMeter;	// X 解像度
	uint32 biYPelsPerMeter;	// Y 解像度
	uint32 biClrUsed;
	uint32 biClrImportant;
} BITMAPINFOHEADER;

typedef union {
	uint32 dib_size;
	BITMAPCOREHEADER bc;
	BITMAPINFOHEADER bi;
} DIBHEADER;

struct bmpctx {
	FILE *fp;
	struct image *img;
	uint16 palette[256];
};

typedef bool (*rasterop_t)(struct bmpctx *, int);

static bool raster_rgb1(struct bmpctx *, int);
static bool raster_rgb4(struct bmpctx *, int);
static bool raster_rgb8(struct bmpctx *, int);
static bool raster_rgb16(struct bmpctx *, int);
static bool raster_rgb24(struct bmpctx *, int);
static bool raster_rgb32(struct bmpctx *, int);
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

	if (compression == BI_RGB) {
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
	} else {
		Debug(diag, "%s: compression=%u not supported", __func__,
			compression);
		return NULL;
	}

	// 直接内部形式にする。
	ctx->img = image_create(width, height, IMAGE_FMT_ARGB16);
	if (ctx->img == NULL) {
		return false;
	}

	// パレットは BiBitCount が 8 以下の時にある。
	if (bitcount <= 8) {
		// パレット数は BiClrUsed があればこれが色数、
		// 0 ならデフォルト (1 << BiBitCount)
		uint npal = clrused ?: (1U << bitcount);
		uint32 palbuf[npal];
		n = fread(palbuf, 4, npal, fp);
		if (n < npal) {
			Debug(diag, "%s: fread(npal=%u)=%zu: %s", __func__,
				npal, n, strerrno());
			goto abort;
		}
		for (uint i = 0; i < npal; i++) {
			uint xrgb = le32toh(palbuf[i]);
			uint8 r = (xrgb >> 16) & 0xff;
			uint8 g = (xrgb >>  8) & 0xff;
			uint8 b = (xrgb      ) & 0xff;
			ctx->palette[i] = RGB888_to_ARGB16(r, g, b);
		}
	}

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

static bool
raster_rgb1(struct bmpctx *ctx, int y)
{
	struct image *img = ctx->img;
	// 1バイトに8ピクセル収まっているのを4バイト単位に切り上げる。
	uint bytewidth = howmany(img->width, 8);
	uint bmpstride = roundup(bytewidth, 4);
	uint8 srcbuf[bmpstride];

	size_t n = fread(srcbuf, 1, bmpstride, ctx->fp);

	const uint8 *s = srcbuf;
	uint16 *d = (uint16 *)img->buf + img->width * y;
	uint xend = img->width;
	if (xend > n * 8) {
		xend = n * 8;
	}
	uint8 bits = 0;
	for (uint x = 0; x < xend; x++) {
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

	const uint8 *s = srcbuf;
	uint16 *d = (uint16 *)img->buf + img->width * y;
	uint xend = img->width;
	if (xend > n * 2) {
		xend = n * 2;
	}
	// 偶数で回せるだけ回す。
	uint xend2 = xend & ~1U;
	for (uint x = 0; x < xend2; x += 2) {
		// 左のピクセルが上位ニブル側。
		uint32 packed = *s++;
		uint32 h = packed >> 4;
		uint32 l = packed & 0xf;
		*d++ = ctx->palette[h];
		*d++ = ctx->palette[l];
	}
	// 奇数ならもう1ピクセル。
	if (xend2 != xend) {
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

	size_t n = fread(srcbuf, 1, img->width, ctx->fp);

	const uint8 *s = srcbuf;
	uint16 *d = (uint16 *)img->buf + img->width * y;
	for (uint x = 0; x < n; x++) {
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
	uint8 srcbuf[bmpstride];

	size_t n = fread(srcbuf, 2, img->width, ctx->fp);

	const uint16 *s = (uint16 *)srcbuf;
	uint16 *d = (uint16 *)img->buf + img->width * y;
	for (uint x = 0; x < n; x++) {
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

	size_t n = fread(srcbuf, 3, img->width, ctx->fp);

	const uint8 *s = srcbuf;
	uint16 *d = (uint16 *)img->buf + img->width * y;
	for (uint x = 0; x < n; x++) {
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

	const uint32 *s = srcbuf;
	uint16 *d = (uint16 *)img->buf + img->width * y;
	for (uint x = 0; x < n; x++) {
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
