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

#include <limits.h>
#include <stdio.h>
#include <stdint.h>
#include "imagereductor.native.h"

#ifndef __packed
#define __packed __attribute__((__packed__))
#endif

typedef struct ColorRGBint_t
{
	int r;
	int g;
	int b;
} ColorRGBint;

typedef struct ColorRGBuint8_t
{
	uint8_t r;
	uint8_t g;
	uint8_t b;
} ColorRGBuint8;


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

//////////////// パレット

// パレット記憶域
const ColorRGBuint8 *Palette;
int PaletteCount;
ColorRGBuint8 Palette_Custom[256];

// 色変換関数の関数ポインタ
typedef int (* FindColorFunc_t)(ColorRGBuint8 c);
static FindColorFunc_t ColorFinder;

// 前方参照
static int FindColor_Custom(ColorRGBuint8 c);

// 固定 2 色白黒パレット
static const ColorRGBuint8 Palette_Mono[] = {
 {  0,   0,   0},
 {255, 255, 255},
};

// 固定 2 色白黒パレットコードへ色を変換します。
static int
FindColor_Mono(ColorRGBuint8 c)
{
	return ((int)c.r + (int)c.g + (int)c.b > 128 * 3);
}

// グレーパレット
static void
SetPalette_Gray(int count)
{
	Palette = Palette_Custom;
	PaletteCount = count;
	for (int i = 0; i < count; i++) {
		uint8_t c = i * 255 / (count - 1);
		Palette_Custom[i].r = Palette_Custom[i].g = Palette_Custom[i].b = c;
	}
}

// グレースケールパレット時に、NTSC 輝度が最も近いパレット番号を返します。
static int
FindColor_Gray(ColorRGBuint8 c)
{
	int I = (((int)c.r * 76 + (int)c.g * 153 + (int)c.b * 26) * (PaletteCount - 1) + (255 / PaletteCount)) / 255 / 255;
	if (I >= PaletteCount) return PaletteCount - 1;
	return I;
}

// グレースケールパレット時に、RGB 平均で最も近いパレット番号を返します。
static int
FindColor_GrayMean(ColorRGBuint8 c)
{
	int I = ((int)c.r + (int)c.g + (int)c.b + (255 / PaletteCount) * 3) * (PaletteCount - 1) / 3 / 255;
	if (I >= PaletteCount) return PaletteCount - 1;
	return I;
}

// 固定 8 色パレット
static const ColorRGBuint8 Palette_Fixed8[] =
{
 {  0,   0,   0},
 {255,   0,   0},
 {  0, 255,   0},
 {255, 255,   0},
 {  0,   0, 255},
 {255,   0, 255},
 {  0, 255, 255},
 {255, 255, 255},
};

// 固定 8 色パレットコードへ色を変換します。
static int
FindColor_Fixed8(ColorRGBuint8 c)
{
	int R = (c.r >= 128);
	int G = (c.g >= 128);
	int B = (c.b >= 128);
	return R + (G << 1) + (B << 2);
}

// X68k 固定 16 色パレット
static const ColorRGBuint8 Palette_FixedX68k[] =
{
 {  0,   0,   0},
 {255,   0,   0},
 {  0, 255,   0},
 {255, 255,   0},
 {  0,   0, 255},
 {255,   0, 255},
 {  0, 255, 255},
 {255, 255, 255},
 {192, 192, 192},
 {128,   0,   0},
 {  0, 128,   0},
 {128, 128,   0},
 {  0,   0, 128},
 {128,   0, 128},
 {  0, 128, 128},
 {128, 128, 128},
};

// X68k 固定 16 色パレットへ色を変換します。
static int
FindColor_FixedX68k(ColorRGBuint8 c)
{
	int I = (int)c.r + (int)c.g + (int)c.b;
	int R;
	int G;
	int B;
	if (c.r >= 192 || c.g >= 192 || c.b >= 192) {
		R = c.r >= 192;
		G = c.g >= 192;
		B = c.b >= 192;
		if (R == G && G == B) {
			if (I >= 224 * 3) {
				return 7;
			} else if (I >= 160 * 3) {
				return 8;
			} else {
				return 15;
			}
		}
		return R + (G << 1) + (B << 2);
	} else {
		R = c.r >= 64;
		G = c.g >= 64;
		B = c.b >= 64;
		if (R == G && G == B) {
			if (I >= 160 * 3) {
				return 8;
			} else if (I >= 64 * 3) {
				return 15;
			} else {
				return 0;
			}
		}
		return (R + (G << 1) + (B << 2)) | 8;
	}
}

// x68k 16 色のパレットをシステムから取得して生成します。
static void
SetPalette_CustomX68k()
{
	char sname[] = "hw.ite.tpalette0";

	Palette = Palette_Custom;
	PaletteCount = 16;

	for (int i = 0; i < 16; i++) {
		int val;
		int r, g, b;
		sname[sizeof(sname) - 1] = (i <= 9) ? (i + 0x30) : (i - 10 + 'A');

		if (native_sysctlbyname(sname, &val, sizeof(val), NULL, 0) == -1) {
			// エラーになったらとりあえず内蔵固定16色
//fprintf(stderr, "sysctl error\n");
				Palette = Palette_FixedX68k;
				return;
		}
		// x68k のパレットは GGGG_GRRR_RRBB_BBBI
		g = (((val >> 11) & 0x1f) << 1) | (val & 1);
		r = (((val >>  6) & 0x1f) << 1) | (val & 1);
		b = (((val >>  1) & 0x1f) << 1) | (val & 1);

		Palette_Custom[i].r = r * 255 / 63;
		Palette_Custom[i].g = g * 255 / 63;
		Palette_Custom[i].b = b * 255 / 63;
	}
}

// X68k 16 色パレットへ色を変換します。
static int
FindColor_CustomX68k(ColorRGBuint8 c)
{
	// XXX unsupported
	return FindColor_Custom(c);
}

// ANSI 固定 16 色パレット
// Standard VGA colors を基準とし、
// ただしパレット4 を Brown ではなく Yellow になるようにしてある。
static const ColorRGBuint8 Palette_FixedANSI16[] =
{
 {  0,   0,   0},
 {170,   0,   0},
 {  0, 170,   0},
 {170, 170,   0},
 {  0,   0, 170},
 {170,   0, 170},
 {  0, 170, 170},
 {170, 170, 170},
 { 85,  85,  85},
 {255,  85,  85},
 { 85, 255,  85},
 {255, 255,  85},
 { 85,  85, 255},
 {255,  85, 255},
 { 85, 255, 255},
 {255, 255, 255},
};

// ANSI 固定 16 色パレットへ色を変換します。
static int
FindColor_FixedANSI16(ColorRGBuint8 c)
{
	int I = (int)c.r + (int)c.g + (int)c.b;
	int R;
	int G;
	int B;
	if (c.r >= 213 || c.g >= 213 || c.b >= 213) {
		R = c.r >= 213;
		G = c.g >= 213;
		B = c.b >= 213;
		if (R == G && G == B) {
			if (I >= 224 * 3) {
				return 15;
			} else {
				return 7;
			}
		}
		return R + (G << 1) + (B << 2) | 8;
	} else {
		R = c.r >= 85;
		G = c.g >= 85;
		B = c.b >= 85;
		if (R == G && G == B) {
			if (I >= 128 * 3) {
				return 7;
			} else if (I >= 42 * 3) {
				return 8;
			} else {
				return 0;
			}
		}
		return R + (G << 1) + (B << 2);
	}
}

// R3,G3,B2 bit の256色固定パレットを生成します。
static void
SetPalette_Fixed256()
{
	Palette = Palette_Custom;
	PaletteCount = 256;

	for (int i = 0; i < 256; i++) {
		Palette_Custom[i].r = (((i >> 5) & 0x07) * 255 / 7);
		Palette_Custom[i].g = (((i >> 2) & 0x07) * 255 / 7);
		Palette_Custom[i].b = (((i     ) & 0x03) * 255 / 3);
	}
}

// 固定256色時に、最も近いパレット番号を返します。
static int
FindColor_Fixed256(ColorRGBuint8 c)
{
	// 0 1 2 3 4 5 6 7 8 9 a b c d e f
	// 0 0 1 1 2 2 3 3 4 4 5 5 6 6 7 7
	int R = c.r >> 5;
	int G = c.g >> 5;
	int B = c.b >> 6;
	return (R << 5) + (G << 2) + B;
}

// カスタムパレット時に、最も近いパレット番号を返します。
static int
FindColor_Custom(ColorRGBuint8 c)
{
	// RGB の各色の距離の和が最小、にしてある。
	// YCC で判断したほうが良好なのは知ってるけど、そこまで必要じゃない。
	// とおもったけどやっぱり品質わるいので色差も考えていく。

	// 色差情報を重みにしていく。
	int K1 = ((int)c.r*2 - (int)c.g - (int)c.b); if (K1 < 1) K1 = 1; if (K1 > 8) K1 = 4;
	int K2 = ((int)c.g*2 - (int)c.r - (int)c.b); if (K2 < 1) K2 = 1; if (K2 > 8) K2 = 4;
	int K3 = ((int)c.b*2 - (int)c.r - (int)c.g); if (K3 < 1) K3 = 1; if (K3 > 8) K3 = 4;
	int rv = 0;
	int min_d = INT_MAX;
	for (int i = 0; i < PaletteCount; i++) {
		int dR = (int)Palette[i].r - (int)c.r;
		int dG = (int)Palette[i].g - (int)c.g;
		int dB = (int)Palette[i].b - (int)c.b;
		int d = abs(dR) * K1 + abs(dG) * K2 + abs(dB) * K3;

		if (d < min_d) {
			rv = i;
			min_d = d;
			if (d == 0) break;
		}
	}
	return rv;
}

// カラーモードを設定します。
void
ImageReductor_SetColorMode(ReductorColorMode mode, /*optional*/ int count)
{
	switch (mode) {
		case RCM_Mono:
			Palette = Palette_Mono;
			PaletteCount = 2;
			ColorFinder = FindColor_Mono;
			break;
		case RCM_Gray:
			SetPalette_Gray(count);
			ColorFinder = FindColor_Gray;
			break;
		case RCM_GrayMean:
			SetPalette_Gray(count);
			ColorFinder = FindColor_GrayMean;
			break;
		case RCM_Fixed8:
			Palette = Palette_Fixed8;
			PaletteCount = 8;
			ColorFinder = FindColor_Fixed8;
			break;
		case RCM_FixedX68k:
			Palette = Palette_FixedX68k;
			PaletteCount = 16;
			ColorFinder = FindColor_FixedX68k;
			break;
		case RCM_CustomX68k:
			SetPalette_CustomX68k();
			ColorFinder = FindColor_CustomX68k;
			break;
		case RCM_FixedANSI16:
			Palette = Palette_FixedANSI16;
			PaletteCount = 16;
			ColorFinder = FindColor_FixedANSI16;
			break;
		case RCM_Fixed256:
			SetPalette_Fixed256();
			ColorFinder = FindColor_Fixed256;
			break;
		case RCM_Custom:
			ColorFinder = FindColor_Custom;
			break;
	}
}

//////////////// その他のサブルーチン

static uint8_t
Saturate_uint8(int x)
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
ImageReductor_Fast(
	uint8_t *dst, int dstLen,
	int dstWidth, int dstHeight,
	uint8_t *src, int srcLen,
	int srcWidth, int srcHeight,
	int srcNch, int srcStride)
{
//fprintf(stderr, "dst=(%d,%d) src=(%d,%d)\n", dstWidth, dstHeight, srcWidth, srcHeight);

	// 螺旋状に一次元誤差分散させる。
	// 当然画像処理的には正しくないが、視覚的にはそんなに遜色が無い。

	ColorRGBint col = {0, 0, 0};

	if (dstWidth == srcWidth && dstHeight == srcHeight) {
		// 変形済み前提
		int nch_adj = srcNch - 3;
		uint8_t *srcRaster = src;
		for (int y = 0; y < dstHeight; y++) {
			uint8_t *srcPix = srcRaster;
			for (int x = 0; x < dstWidth; x++) {
				col.r += *srcPix++;
				col.g += *srcPix++;
				col.b += *srcPix++;
				srcPix += nch_adj;
//fprintf(stderr, "(%d %d %d) ", col.r, col.g, col.b);
				ColorRGBuint8 c8 = {
					Saturate_uint8(col.r),
					Saturate_uint8(col.g),
					Saturate_uint8(col.b),
				};

//fprintf(stderr, "%d ", (c8.r + c8.g + c8.b) / 3);
				int colorCode = ColorFinder(c8);
//fprintf(stderr, "%d ", colorCode);

				const int level = 160;
				col.r = (col.r - Palette[colorCode].r) * level / 256;
				col.g = (col.g - Palette[colorCode].g) * level / 256;
				col.b = (col.b - Palette[colorCode].b) * level / 256;

#if 1
				if (col.r < -511) col.r = 0;
				if (col.g < -511) col.g = 0;
				if (col.b < -511) col.b = 0;
#endif
				*dst++ = colorCode;
			}
			srcRaster += srcStride;
		}

	} else {

		// 水平方向は Pow2 になるピクセルをサンプリングして平均
		// 垂直方向はスキップサンプリング

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

				ColorRGBuint8 c8 = {
					Saturate_uint8(col.r),
					Saturate_uint8(col.g),
					Saturate_uint8(col.b),
				};

				int colorCode = ColorFinder(c8);

				const int level = 120;
				col.r = (col.r - Palette[colorCode].r) * level / 256;
				col.g = (col.g - Palette[colorCode].g) * level / 256;
				col.b = (col.b - Palette[colorCode].b) * level / 256;

#if 0
				if (col.r < -255) col.r = 0;
				if (col.g < -255) col.g = 0;
				if (col.b < -255) col.b = 0;
#endif

				*dst++ = colorCode;
			}
		}
	}

	return 0;
}

// 画像を縮小しながら減色して変換します。
// 単純減色法を適用します。
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
ImageReductor_Simple(
	uint8_t *dst, int dstLen,
	int dstWidth, int dstHeight,
	uint8_t *src, int srcLen,
	int srcWidth, int srcHeight,
	int srcNch, int srcStride)
{
// 水平方向はスキップサンプリング
// 垂直方向はスキップサンプリング

//fprintf(stderr, "dst=(%d,%d) src=(%d,%d)\n", dstWidth, dstHeight, srcWidth, srcHeight);

	ColorRGBuint8 col = {0, 0, 0};
	StepRational sr_y = StepRationalCreate(0, 0, dstHeight);
	StepRational sr_ystep = StepRationalCreate(0, srcHeight, dstHeight);

	StepRational sr_x = StepRationalCreate(0, 0, dstWidth);
	StepRational sr_xstep = StepRationalCreate(0, srcWidth, dstWidth);

	for (int y = 0; y < dstHeight; y++) {
		uint8_t *srcRaster = &src[sr_y.I * srcStride];
		StepRationalAdd(&sr_y, &sr_ystep);

		sr_x.I = sr_x.N = 0;

		for (int x = 0; x < dstWidth; x++) {

			int sx0 = sr_x.I;
			StepRationalAdd(&sr_x, &sr_xstep);

			uint8_t *srcPix = &srcRaster[sx0 * srcNch];
			col.r = srcPix[0];
			col.g = srcPix[1];
			col.b = srcPix[2];

			int colorCode = ColorFinder(col);

			*dst++ = colorCode;
		}
	}

	return 0;
}

// 画像を縮小しながら減色して変換します。
// 二次元誤差分散法を使用して、出来る限り高品質に変換します。
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
ImageReductor_HighQuality(
	uint8_t *dst, int dstLen,
	int dstWidth, int dstHeight,
	uint8_t *src, int srcLen,
	int srcWidth, int srcHeight,
	int srcNch, int srcStride)
{
// 水平方向はピクセルを平均
// 垂直方向はピクセルを平均
// 真に高品質にするには補間法を適用するべきだがそこまではしない。

//fprintf(stderr, "dst=(%p,%d,%d) src=(%p,%d,%d)\n", dst, dstWidth, dstHeight, src, srcWidth, srcHeight);

	StepRational sr_y = StepRationalCreate(0, 0, dstHeight);
	StepRational sr_ystep = StepRationalCreate(0, srcHeight, dstHeight);

	StepRational sr_x = StepRationalCreate(0, 0, dstWidth);
	StepRational sr_xstep = StepRationalCreate(0, srcWidth, dstWidth);

	// 誤差バッファ
	// 2 ラスタ分を切り換えて使う。
	ColorRGBint *errbuf_0 = calloc(dstWidth, sizeof(ColorRGBint));
	ColorRGBint *errbuf_1 = calloc(dstWidth, sizeof(ColorRGBint));

	for (int y = 0; y < dstHeight; y++) {

		int sy0 = sr_y.I;
		StepRationalAdd(&sr_y, &sr_ystep);
		int sy1 = sr_y.I;
		if (sy0 == sy1) sy1 += 1;

		sr_x.I = sr_x.N = 0;

		for (int x = 0; x < dstWidth; x++) {

			ColorRGBint col = {0, 0, 0};

			int sx0 = sr_x.I;
			StepRationalAdd(&sr_x, &sr_xstep);
			int sx1 = sr_x.I;
			if (sx0 == sx1) sx1 += 1;

			// 画素の平均を求める
			for (int sy = sy0; sy < sy1; sy++) {
				uint8_t *srcRaster = &src[sy * srcStride];
				uint8_t *srcPix = &srcRaster[sx0 * srcNch];
				for (int sx = sx0; sx < sx1; sx++) {
					col.r += srcPix[0];
					col.g += srcPix[1];
					col.b += srcPix[2];
					srcPix += srcNch;
				}
			}

			int D = (sy1 - sy0) * (sx1 - sx0);

			col.r /= D;
			col.g /= D;
			col.b /= D;

			if (x > 0) {
				col.r += errbuf_0[x - 1].r * 72 / 256;
				col.g += errbuf_0[x - 1].g * 72 / 256;
				col.b += errbuf_0[x - 1].b * 72 / 256;
			}
			if (y > 0) {
				col.r += errbuf_1[x].r * 72 / 256;
				col.g += errbuf_1[x].g * 72 / 256;
				col.b += errbuf_1[x].b * 72 / 256;
			}
			if (x > 0 && y > 0) {
				col.r += errbuf_1[x - 1].r * 32 / 256;
				col.g += errbuf_1[x - 1].g * 32 / 256;
				col.b += errbuf_1[x - 1].b * 32 / 256;
			}

			ColorRGBuint8 c8 = {
				Saturate_uint8(col.r),
				Saturate_uint8(col.g),
				Saturate_uint8(col.b),
			};

			int colorCode = ColorFinder(c8);

			col.r = col.r - Palette[colorCode].r;
			col.g = col.g - Palette[colorCode].g;
			col.b = col.b - Palette[colorCode].b;

			errbuf_0[x] = col;

			*dst++ = colorCode;
		}

		// 誤差バッファをスワップ
		ColorRGBuint8* tmp = errbuf_0;
		errbuf_0 = errbuf_1;
		errbuf_1 = tmp;
	}

	free(errbuf_0);
	free(errbuf_1);

	return 0;
}
