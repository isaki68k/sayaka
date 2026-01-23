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
// BMP/ICO
//

#ifndef sayaka_image_bmp_h
#define sayaka_image_bmp_h

// 圧縮方式
#define BI_RGB			(0)
#define BI_RLE8			(1)
#define BI_RLE4			(2)
#define BI_BITFIELDS	(3)
#define BI_JPEG			(4)
#define BI_PNG			(5)

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

// ?
typedef struct __packed {
	uint32 cSize;
	uint32 cWidth;
	uint32 cHeight;
	uint16 cPlanes;
	uint16 cBitCount;
	uint32 cCompression;
	uint32 cSizeImage;
	uint32 cXResolution;
	uint32 cYResolution;
	uint32 cClrUsed;
	uint32 cClrImportant;
	uint16 cUnits;
	uint16 cReserved;
	uint16 cRecording;
	uint16 cRendering;
	uint32 cSize1;
	uint32 cSize2;
	uint32 cColorEncoding;
	uint32 cIdentifier;
} BITMAPINFOHEADER2;

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

// ラスター処理関数の戻り値
#define RASTER_OK	(0)		// 正常に処理した
#define RASTER_EOF	(1)		// ここでファイル終わり (RLE)
#define RASTER_ERR	(-1)	// エラー (エラーメッセージは表示済み)

struct bmpctx;
typedef int (*bmp_rasterop_t)(struct bmpctx *, int);

struct bmpctx {
	FILE *fp;
	struct image *img;

	uint32 width;
	int32 height;
	bool bottom_up;
	uint32 bitcount;
	uint32 compression;
	uint clrused;
	bmp_rasterop_t rasterop;

	// BI_BITFIELDS、各 R,G,B の順。
	uint32 mask[3];		// マスクビット (例えば 0x00ff0000)
	uint offset[3];		// マスクの右側のゼロの数
	uint maskbits[3];	// マスクのビット数

	uint16 palette[256];
};

extern void bmp_read_info_header(struct bmpctx *, const BITMAPINFOHEADER *);
extern void bmp_print_debuginfo(struct bmpctx *, const struct diag *,
	const char *, uint32);
extern bool bmp_select_raster_rgb(struct bmpctx *);
extern bool bmp_read_palette4(struct bmpctx *);
extern bool bmp_extract(struct bmpctx *);

#endif /* sayaka_image_bmp_h */
