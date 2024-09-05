/* vi:set ts=4: */
/*
 * Copyright (C) 2024 Tetsuya Isaki
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
// BMP 書き出し
// (BMP の読み込みは stb_image で行う)
//

#include "sixelv.h"
#include "image_priv.h"
#include <string.h>

typedef struct __packed {
	uint8  bfType[2];
	uint32 bfSize;
	uint32 bfReserved;
	uint32 bfOffBits;
} BITMAPFILEHEADER;

typedef struct __packed {
	uint32 biSize;			// ヘッダサイズ (40)
	uint32 biWidth;			// 画像のピクセル幅
	uint32 biHeight;		// 画像のピクセル高さ (正なら下から上へ)
	uint16 biPlanes;		// プレーン数 (1)
	uint16 biBitCount;		// bits per pixel
	uint32 biCompression;	// 圧縮方式 (BI_RGB = 0)
#define BI_RGB	(0)
	uint32 biSizeImage;		// データ部のバイト数
	uint32 biXPelsPerMeter;	// X 解像度
	uint32 biYPelsPerMeter;	// Y 解像度
	uint32 biClrUsed;
	uint32 biCirImportant;
} BITMAPINFOHEADER;

static image *image_coloring(const image *);

// image を BMP 形式で fp に出力する。
bool
image_bmp_write(FILE *fp, const image *srcimg, const diag *diag)
{
	image *img;
	BITMAPFILEHEADER hdr;
	BITMAPINFOHEADER info;
	uint32 istride;	// 入力ストライド
	uint32 ostride;	// 出力ストライド (4バイトの倍数でなければならない)
	uint32 padding;
	uint32 datasize;
	bool rv = false;

	if (srcimg->format == IMAGE_FMT_AIDX16) {	// XXX 未対応
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

// インデックスカラーの srcimg をパレットで着色した画像を返す。
static image *
image_coloring(const image *srcimg)
{
	image *dstimg;

	assert(srcimg->format == IMAGE_FMT_AIDX16);	// XXX 未対応
	assert(srcimg->palette != NULL);

	dstimg = image_create(srcimg->width, srcimg->height, IMAGE_FMT_RGB24);
	if (dstimg == NULL) {
		return NULL;
	}

	const uint8 *s = srcimg->buf;
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
