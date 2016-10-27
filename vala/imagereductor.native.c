/*
 * Copyright (C) 2016 Y.Sugahara (moveccr)
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

#include <stdio.h>
#include <stdint.h>

typedef struct colorRGBint_t
{
	int r;
	int g;
	int b;
} colorRGBint;


//////////////// 分数計算機
// DDA 計算の基礎となる I + N / D 型の分数ステップ加減算計算機です。

typedef struct StepRational_t
{
	// 整数項です。
	int I;
	// 分子です。
	int N;
	// 分母です。
	int D;
} StepRational;

static StepRational
StepRationalCreate(int i, int n, int d)
{
	StepRational rv;
	rv.I = i;
	if (n < d) {
		rv.N = n;
	} else {
		rv.I += n / d;
		rv.N = n % d;
	}
	rv.D = d;
	return rv;
}

static void
StepRationalAdd(StepRational* sr, StepRational* x)
{
	sr->I += x->I;
	sr->N += x->N;
	if (sr->N < 0) {
		sr->I--;
		sr->N += sr->D;
	} else if (sr->N >= sr->D) {
		sr->I++;
		sr->N -= sr->D;
	}
}

//////////////// 色探索

// 固定 8 色パレットコードへ色を変換します。
// P0 = (0, 0, 0)		Black
// P1 = (255, 0, 0)		Red
// P2 = (0, 255, 0)		Green
// P3 = (255, 255, 0)	Yellow
// P4 = (0, 0, 255)		Blue
// P5 = (255, 0, 255)	Magenta
// P6 = (0, 255, 255)	Cyan
// P7 = (255, 255, 255)	White
static int
imagereductor_findfixed8(uint8_t r, uint8_t g, uint8_t b)
{
	uint8_t R = (uint8_t)(r >= 128);
	uint8_t G = (uint8_t)(g >= 128);
	uint8_t B = (uint8_t)(b >= 128);
	return R + (G << 1) + (B << 2);
}

// 固定 8 色パレットコードへ色を変換します。
// P0 = (0, 0, 0)		Black
//		色差 r-g, g-b = 0, 0
// P1 = (255, 0, 0)		Red
//		色差 r-g, g-b = 255, 0
// P2 = (0, 255, 0)		Green
//		色差 r-g, g-b = -255, 255
// P3 = (255, 255, 0)	Yellow
//		色差 r-g, g-b = 0, 255
// P4 = (0, 0, 255)		Blue
//		色差 r-g, g-b = 0, -255
// P5 = (255, 0, 255)	Magenta
//		色差 r-g, g-b = 255, -255
// P6 = (0, 255, 255)	Cyan
//		色差 r-g, g-b = -255, 0
// P7 = (255, 255, 255)	White
//		色差 r-g, g-b = 0, 0
// P8 = (192, 192, 192)	LightGray 23,23,23,1
//		色差 r-g, g-b = 0, 0
// P9 = (128, 0, 0)		DarkRed
//		色差 r-g, g-b = 128, 0
// Pa = (0, 128, 0)		DarkGreen
//		色差 r-g, g-b = -128, 128
// Pb = (128, 128, 0)	DarkYellow
//		色差 r-g, g-b = 0, 128
// Pc = (0, 0, 128)		DarkBlue
//		色差 r-g, g-b = 0, -128
// Pd = (128, 0, 128)	DarkMagenta
//		色差 r-g, g-b = 128, -128
// Pe = (0, 128, 128)	DarkCyan
//		色差 r-g, g-b = -128, 0
// Pf = (128, 128, 128)	Gray 15,15,15,1
//		色差 r-g, g-b = 0, 0
static int
imagereductor_findfixed16(uint8_t r, uint8_t g, uint8_t b)
{
	int rg = (int)r - (int)g;
	int gb = (int)g - (int)b;

	if (rg >= 192) {
		// RG=255
		if (gb > -128) {
			return 1;
		} else {
			return 5;
		}
	} else if (rg >= 64) {
		// RG=128
		if (gb >= 0) {
			return 9;
		} else {
			return 13;
		}
	} else if (rg > -64) {
		// RG=0
		if (gb >= 192) {
			return 3;
		} else if (gb >= 64) {
			return 11;
		} else if (gb > -64) {
			if (r >= 224) {
				return 7;
			} else if (r >= 160) {
				return 8;
			} else if (r >= 64) {
				return 15;
			} else {
				return 0;
			}
		} else if (gb > -192) {
			return 12;
		} else {
			return 4;
		}
	} else if (rg > -192) {
		// RG=-128
		if (gb >= 64) {
			return 10;
		} else {
			return 14;
		}
	} else {
		// RG=-255
		if (gb >= 128) {
			return 2;
		} else {
			return 6;
		}
	}
}

//////////////// その他のサブルーチン

static uint8_t
saturate_byte(int x)
{
	if (x < 0) return 0;
	if (x > 255) return 255;
	return (uint8_t)x;
}

static int
RoundDownPow2(int x)
{
	x |= x >> 1;
	x |= x >> 2;
	x |= x >> 4;
	x |= x >> 8;
	x |= x >> 16;
	x += 1;
	return x >> 1;
}

//////////////// 変換関数

// 画像を縮小しながら減色して変換します。
// 出来る限り高速に、それなりの品質で変換します。
// 色は固定 8 色です。
// dst : 色コードを出力するバッファです。
//       dstWidth * dstHeight バイト以上を保証してください。
// dstWidth : 出力の幅です。
// dstHeight : 出力の高さです。
// src : 入力ピクセルデータ (R,G,B または R,G,B,A) です。
// srcWidth : 入力の幅です。
// srcHeight : 入力の高さです。
// srcNch : 入力のチャンネル数です。3 か 4 を保証してください。
// srcStride : 入力のストライドのバイト長さです。
int
imagereductor_resize_reduce_fast_fixed8(
	uint8_t *dst, int dstLen,
	int dstWidth, int dstHeight,
	uint8_t *src, int srcLen,
	int srcWidth, int srcHeight,
	int srcNch, int srcStride)
{
// 水平方向は Pow2 になるピクセルをサンプリングして平均
// 垂直方向はスキップサンプリング

//fprintf(stderr, "dst=(%d,%d) src=(%d,%d)\n", dstWidth, dstHeight, srcWidth, srcHeight);

	colorRGBint col;
	col.r = col.g = col.b = 0;
	StepRational sr_y = StepRationalCreate(0, 0, dstHeight);
	StepRational sr_ystep = StepRationalCreate(0, srcHeight, dstHeight);

	StepRational sr_x = StepRationalCreate(0, 0, dstWidth);
	StepRational sr_xstep = StepRationalCreate(0, srcWidth, dstWidth);

	int sw = RoundDownPow2(sr_xstep.I);
	if (sw == 0) sw = 1;
	int meanShift = 31 - __builtin_clz(sw);

	for (int y = 0; y < dstHeight; y++) {
		uint8_t *srcRaster = &src[sr_y.I * srcStride];
		StepRationalAdd(&sr_y, &sr_ystep);

		sr_x.I = sr_x.N = 0;

		for (int x = 0; x < dstWidth; x++) {

			int sx0 = sr_x.I;
			StepRationalAdd(&sr_x, &sr_xstep);

			uint8_t *srcPix = &srcRaster[sx0 * srcNch];
			for (int sx = 0; sx < sw; sx++) {
				col.r += srcPix[0];
				col.g += srcPix[1];
				col.b += srcPix[2];
				srcPix += srcNch;
			}

			col.r >>= meanShift;
			col.g >>= meanShift;
			col.b >>= meanShift;

			uint8_t f8 = imagereductor_findfixed8(
				saturate_byte(col.r),
				saturate_byte(col.g),
				saturate_byte(col.b));

			col.r -= (f8 & 1) << 8;
			col.g -= ((f8 >> 1) & 1) << 8;
			col.b -= ((f8 >> 2) & 1) << 8;

			*dst++ = f8;
		}
	}

	return 0;
}

// 画像を縮小しながら減色して変換します。
// 出来る限り高速に、それなりの品質で変換します。
// 色は固定 16 色です。
// dst : 色コードを出力するバッファです。
//       dstWidth * dstHeight バイト以上を保証してください。
// dstWidth : 出力の幅です。
// dstHeight : 出力の高さです。
// src : 入力ピクセルデータ (R,G,B または R,G,B,A) です。
// srcWidth : 入力の幅です。
// srcHeight : 入力の高さです。
// srcNch : 入力のチャンネル数です。3 か 4 を保証してください。
// srcStride : 入力のストライドのバイト長さです。
int
imagereductor_resize_reduce_fast_fixed16(
	uint8_t *dst, int dstLen,
	int dstWidth, int dstHeight,
	uint8_t *src, int srcLen,
	int srcWidth, int srcHeight,
	int srcNch, int srcStride)
{
// 水平方向は Pow2 になるピクセルをサンプリングして平均
// 垂直方向はスキップサンプリング

//fprintf(stderr, "dst=(%d,%d) src=(%d,%d)\n", dstWidth, dstHeight, srcWidth, srcHeight);

	colorRGBint col;
	col.r = col.g = col.b = 0;
	StepRational sr_y = StepRationalCreate(0, 0, dstHeight);
	StepRational sr_ystep = StepRationalCreate(0, srcHeight, dstHeight);

	StepRational sr_x = StepRationalCreate(0, 0, dstWidth);
	StepRational sr_xstep = StepRationalCreate(0, srcWidth, dstWidth);

	int sw = RoundDownPow2(sr_xstep.I);
	if (sw == 0) sw = 1;
	int meanShift = 31 - __builtin_clz(sw);

	for (int y = 0; y < dstHeight; y++) {
		uint8_t *srcRaster = &src[sr_y.I * srcStride];
		StepRationalAdd(&sr_y, &sr_ystep);

		sr_x.I = sr_x.N = 0;

		for (int x = 0; x < dstWidth; x++) {

			int sx0 = sr_x.I;
			StepRationalAdd(&sr_x, &sr_xstep);

			uint8_t *srcPix = &srcRaster[sx0 * srcNch];
			for (int sx = 0; sx < sw; sx++) {
				col.r += srcPix[0];
				col.g += srcPix[1];
				col.b += srcPix[2];
				srcPix += srcNch;
			}

			col.r >>= meanShift;
			col.g >>= meanShift;
			col.b >>= meanShift;

			uint8_t f8 = imagereductor_findfixed16(
				saturate_byte(col.r),
				saturate_byte(col.g),
				saturate_byte(col.b));

#if 0
			col.r -= (f8 & 1) << 8;
			col.g -= ((f8 >> 1) & 1) << 8;
			col.b -= ((f8 >> 2) & 1) << 8;
#else
			col.r = col.g = col.b = 0;
#endif

			*dst++ = f8;
		}
	}

	return 0;
}

