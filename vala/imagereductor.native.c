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
#include <stddef.h>
#include <stdint.h>
#include <jpeglib.h>
#include <glib.h>
#include "imagereductor.native.h"

#ifndef __packed
#define __packed __attribute__((__packed__))
#endif

extern gboolean gdiag_global_debug;
#define DEBUG_PRINTF(x...) if (gdiag_global_debug) fprintf(stderr, x)

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

ColorHSVuint8 HSVPalette[256];

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
// NetBSD/x68k デフォルトテキストパレットより
static const ColorRGBuint8 Palette_FixedX68k[] =
{
 {  0,   0,   0},	// 透明
 {252,   4,   4},
 {  4, 252,   4},
 {252, 252,   4},
 {  4,   4, 252},
 {252,   4, 252},
 {  4, 252, 252},
 {252, 252, 252},
 {  4,   4,   4},	// 黒
 {124,   4,   4},
 {  4, 124,   4},
 {124, 124,   4},
 {  4,   4, 124},
 {124,   4, 124},
 {  4, 124, 124},
 {124, 124, 124},
};

// X68k 固定 16 色パレットへ色を変換します。
static int
FindColor_FixedX68k(ColorRGBuint8 c)
{
	int R = (int)c.r;
	int G = (int)c.g;
	int B = (int)c.b;
	int min;
	int max;
	int H;
	int S;
	int V;
	int pal;

	min = MIN(MIN(R, G), B);
	max = MAX(MAX(R, G), B);

	if (min == max) {
		H = 0;
	} else if (min == B) {
		H = 60 * (G - R) / (max - min) + 60;
	} else if (min == R) {
		H = 60 * (B - G) / (max - min) + 180;
	} else if (min == G) {
		H = 60 * (R - B) / (max - min) + 300;
	}
	if (H < 0) {
		H += 360;
	} else if (H >= 360) {
		H -= 360;
	}
	V = max;
	S = max - min;

	// X68k 固定16色のうち前8色(のうち黒と白を除いた6色)が原色なので
	// 色相からそのままマッピングできる
	if (H < 30) {
		pal = 1;		// H = 0
	} else if (H < 90) {
		pal = 3;		// H = 60
	} else if (H < 150) {
		pal = 2; 		// H = 120
	} else if (H < 210) {
		pal = 6;		// H = 180
	} else if (H < 270) {
		pal = 4;		// H = 240
	} else if (H < 330) {
		pal = 5;		// H = 300
	} else {
		pal = 1;		// H = 360
	}

	if (V < 255 / 3) {
		// V が低いので黒
		pal = 8;
	} else if (V < 255 * 2 / 3) {
		// V が中間。
		// S が高ければ暗色、そうでなければ灰
		if (S < V / 2) {
			pal = 15;
		} else {
			pal += 8;
		}
	} else {
		// V が高い
		// S が高ければ原色、そうでなければ白
		if (S < V / 2) {
			pal = 7;
		}
	}

	return pal;
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

// R2,G2,B2,I2 bit の 256色固定パレットを生成します。
static void
SetPalette_Fixed256RGBI()
{
	Palette = Palette_Custom;
	PaletteCount = 256;

	for (int i = 0; i < 256; i++) {
		uint8_t R, G, B, I;
		R = (i >> 6) & 3;
		G = (i >> 4) & 3;
		B = (i >> 2) & 3;
		I = (i     ) & 3;

//DEBUG_PRINTF("(%d,%d,%d,%d)\n", R, G, B, I);
		Palette_Custom[i].r = (R << 6) + (I * 63 / 3);
		Palette_Custom[i].g = (G << 6) + (I * 63 / 3);
		Palette_Custom[i].b = (B << 6) + (I * 63 / 3);
	}
}

// R2,G2,B2,I2 bit の 固定256色時に、最も近いパレット番号を返します。
static int
FindColor_Fixed256RGBI(ColorRGBuint8 c)
{
	uint8_t R, G, B, I;
	R = c.r >> 6;
	G = c.g >> 6;
	B = c.b >> 6;
	// 最も強い成分で I を決める
	if (R > G && R > B) {
		I = ((c.r & 0x3f) + 10) / 21;
	} else if (G > R && G > B) {
		I = ((c.g & 0x3f) + 10) / 21;
	} else if (B > R && B > G) {
		I = ((c.b & 0x3f) + 10) / 21;
	} else {
		// グレーなら I は平均で決める
		I = ((c.r & 0x3f) + (c.g & 0x3f) + (c.b & 0x3f) + 31) / 63;
	}
	return (R << 6)	| (G << 4) | (B << 2) | I;
}

// 円錐型 HSV を計算します。
// H = 0..239, 255
// S = 0..255
// V = 0..255
static ColorHSVuint8
RGBtoHSV(ColorRGBuint8 c)
{
	ColorHSVuint8 rv;

	int min = MIN(MIN(c.r, c.g), c.b);
	int max = MAX(MAX(c.r, c.g), c.b);
	rv.s = max - min;
	rv.v = max;
	if (rv.s == 0) {
		rv.h = 255;	// gray
	} else if (min == c.b) {
		rv.h = 40 * (c.g - c.r) / rv.s + 40;
	} else if (min == c.r) {
		rv.h = 40 * (c.b - c.g) / rv.s + 120;
	} else {
		rv.h = 40 * (c.r - c.b) / rv.s + 200;
	}
	return rv;
}

static void
CreateHSVPalette()
{
	for (int i = 0; i < PaletteCount; i++) {
		HSVPalette[i] = RGBtoHSV(Palette[i]);
	}
}

static int
FindColor_HSV_subr(ColorHSVuint8 hsvpal, ColorHSVuint8 hsv)
{
	int d;
	int dh, ds, dv;

	dv = (int)hsvpal.v - hsv.v;
	ds = (int)hsvpal.s - hsv.s;
	dh = (int)hsvpal.h - hsv.h;
	if (hsv.s != 0 && hsvpal.s == 0) {
		dh = 120;
		ds = 120;
	}
	if (dh > 120) dh -= 240;
	if (dh < -120) dh += 240;

	ds = ds;
	dv = dv;

DEBUG_PRINTF("%d,%d,%d ", dh,ds,dv);
	d = abs(dh)*(hsv.s + 1) / 32 + abs(ds) * 3 + abs(dv) * 5;
	return d;
}

static int
FindColor_HSV(ColorRGBuint8 c)
{
	ColorHSVuint8 hsv = RGBtoHSV(c);

	int min_d = FindColor_HSV_subr(HSVPalette[0], hsv);
	int min_d_i = 0;
	for (int i = 1; i < PaletteCount; i++) {
		int d = FindColor_HSV_subr(HSVPalette[i], hsv);
		if (min_d > d) {
			min_d = d;
			min_d_i = i;
		}
	}
	return min_d_i;
}

// カラーモードとカラーファインダを設定します。
void
ImageReductor_SetColorMode(ReductorColorMode mode, ReductorFinderMode finder, /*optional*/ int count)
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
			ColorFinder = FindColor_HSV;
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
		case RCM_Fixed256RGBI:
			SetPalette_Fixed256RGBI();
			ColorFinder = FindColor_Fixed256RGBI;
			break;
		case RCM_Custom:
			ColorFinder = FindColor_HSV;
			break;
	}

	switch (finder) {
	 case RFM_HSV:
		CreateHSVPalette();
		ColorFinder = FindColor_HSV;
		break;
	 default:
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

static int
rnd()
{
	static uint32_t y = 24539283060;
	y = y ^ (y << 13);
	y = y ^ (y >> 17);
	y = y ^ (y << 5);
	return ((y >> 4) % 31) - 15;
}

//////////////// 変換関数

int AddNoizeMode = 0;

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
DEBUG_PRINTF("dst=(%d,%d) src=(%d,%d)\n", dstWidth, dstHeight, srcWidth, srcHeight);

	// 螺旋状に一次元誤差分散させる。
	// 当然画像処理的には正しくないが、視覚的にはそんなに遜色が無い。

	ColorRGBint col = {0, 0, 0};
	const int level = 256;

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

				ColorRGBuint8 c8 = {
					Saturate_uint8(col.r),
					Saturate_uint8(col.g),
					Saturate_uint8(col.b),
				};

				int colorCode = ColorFinder(c8);

				col.r = (col.r - Palette[colorCode].r) * level / 256;
				col.g = (col.g - Palette[colorCode].g) * level / 256;
				col.b = (col.b - Palette[colorCode].b) * level / 256;

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

		DEBUG_PRINTF("sw=%d meanShift=%d\n", sw, meanShift);
		for (int y = 0; y < dstHeight; y++) {
			uint8_t *srcRaster = &src[sr_y.I * srcStride];
			StepRationalAdd(&sr_y, &sr_ystep);

			sr_x.I = sr_x.N = 0;

			ColorRGBint ce = {0,0,0};

			for (int x = 0; x < dstWidth; x++) {

				int sx0 = sr_x.I;
				StepRationalAdd(&sr_x, &sr_xstep);

				col.r = col.g = col.b = 0;

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

				col.r += ce.r;
				col.g += ce.g;
				col.b += ce.b;

				ColorRGBuint8 c8 = {
					Saturate_uint8(col.r),
					Saturate_uint8(col.g),
					Saturate_uint8(col.b),
				};

				int colorCode = ColorFinder(c8);

				ce.r = (col.r - Palette[colorCode].r) * level / 256;
				ce.g = (col.g - Palette[colorCode].g) * level / 256;
				ce.b = (col.b - Palette[colorCode].b) * level / 256;

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

static int16_t
Saturate_adderr(int16_t a, int b)
{
	int16_t x = a + b;
	if (x < -512) {
		return -512;
	} else if (x > 511) {
		return 511;
	} else {
		return x;
	}
}

// eb[x] += col * ratio / 256;
static void
set_err(ColorRGBint16 eb[], int x, ColorRGBint col, int ratio)
{
	eb[x].r = Saturate_adderr(eb[x].r,  col.r * ratio / 256);
	eb[x].g = Saturate_adderr(eb[x].g,  col.g * ratio / 256);
	eb[x].b = Saturate_adderr(eb[x].b,  col.b * ratio / 256);
}

// High 誤差分散アルゴリズム
ReductorDiffuseMethod HighQualityDiffuseMethod = RDM_FS;

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
	const int errbuf_count = 3;
	const int errbuf_left = 2;
	const int errbuf_right = 2;
	int errbuf_width = dstWidth + errbuf_left + errbuf_right;
	int errbuf_len = errbuf_width * sizeof(ColorRGBint16);
	int errbuf_mem_len = errbuf_len * errbuf_count;

	ColorRGBint16 *errbuf_mem;
	ColorRGBint16 *errbuf[errbuf_count];
	errbuf_mem = malloc(errbuf_mem_len);
	memset(errbuf_mem, 0, errbuf_mem_len);
	for (int i = 0; i < errbuf_count; i++) {
		errbuf[i] = errbuf_mem + errbuf_left + errbuf_width * i;
	}

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

			col.r += errbuf[0][x].r;
			col.g += errbuf[0][x].g;
			col.b += errbuf[0][x].b;

			ColorRGBuint8 c8 = {
				Saturate_uint8(col.r),
				Saturate_uint8(col.g),
				Saturate_uint8(col.b),
			};

			int colorCode = ColorFinder(c8);

			col.r -= Palette[colorCode].r;
			col.g -= Palette[colorCode].g;
			col.b -= Palette[colorCode].b;

			// ランダムノイズを加える
			if (AddNoizeMode) {
				col.r += rnd();
				col.g += rnd();
				col.b += rnd();
			}

			switch (HighQualityDiffuseMethod) {
			 case RDM_FS:
				// Floyd Steinberg Method
				set_err(errbuf[0], x + 1, col, 112);
				set_err(errbuf[1], x - 1, col, 48);
				set_err(errbuf[1], x    , col, 80);
				set_err(errbuf[1], x + 1, col, 16);
				break;
			 case RDM_ATKINSON:
				// Atkinson
				set_err(errbuf[0], x + 1,   col, 32);
				set_err(errbuf[0], x + 2,   col, 32);
				set_err(errbuf[1], x - 1,   col, 32);
				set_err(errbuf[1], x,       col, 32);
				set_err(errbuf[1], x + 1,   col, 32);
				set_err(errbuf[2], x,       col, 32);
				break;
			 case RDM_JAJUNI:
				// Jarvis, Judice, Ninke
				set_err(errbuf[0], x + 1, col, 37);
				set_err(errbuf[0], x + 2, col, 27);
				set_err(errbuf[1], x - 2, col, 16);
				set_err(errbuf[1], x - 1, col, 27);
				set_err(errbuf[1], x,     col, 37);
				set_err(errbuf[1], x + 1, col, 27);
				set_err(errbuf[1], x + 2, col, 16);
				set_err(errbuf[2], x - 2, col,  5);
				set_err(errbuf[2], x - 1, col, 16);
				set_err(errbuf[2], x,     col, 27);
				set_err(errbuf[2], x + 1, col, 16);
				set_err(errbuf[2], x + 2, col,  5);
				break;
			 case RDM_STUCKI:
				// Stucki
				set_err(errbuf[0], x + 1, col, 43);
				set_err(errbuf[0], x + 2, col, 21);
				set_err(errbuf[1], x - 2, col, 11);
				set_err(errbuf[1], x - 1, col, 21);
				set_err(errbuf[1], x,     col, 43);
				set_err(errbuf[1], x + 1, col, 21);
				set_err(errbuf[1], x + 2, col, 11);
				set_err(errbuf[2], x - 2, col,  5);
				set_err(errbuf[2], x - 1, col, 11);
				set_err(errbuf[2], x,     col, 21);
				set_err(errbuf[2], x + 1, col, 11);
				set_err(errbuf[2], x + 2, col,  5);
				break;
			 case RDM_BURKES:
				// Burkes
				set_err(errbuf[0], x + 1, col, 64);
				set_err(errbuf[0], x + 2, col, 32);
				set_err(errbuf[1], x - 2, col, 16);
				set_err(errbuf[1], x - 1, col, 32);
				set_err(errbuf[1], x,     col, 64);
				set_err(errbuf[1], x + 1, col, 32);
				set_err(errbuf[1], x + 2, col, 16);
				break;
			 case RDM_2:
				// (x+1,y), (x,y+1)
				set_err(errbuf[0], x + 1, col, 128);
				set_err(errbuf[1], x,     col, 128);
				break;
			 case RDM_3:
				// (x+1,y), (x,y+1), (x+1,y+1)
				set_err(errbuf[0], x + 1, col, 102);
				set_err(errbuf[1], x,     col, 102);
				set_err(errbuf[1], x + 1, col,  51);
				break;
			 case RDM_RGB:
				errbuf[0][x].r = Saturate_adderr(errbuf[0][x].r, col.r);
				errbuf[1][x].b = Saturate_adderr(errbuf[1][x].b, col.b);
				errbuf[1][x+1].g = Saturate_adderr(errbuf[1][x+1].g, col.g);
				break;
			}

			*dst++ = colorCode;
		}

		// 誤差バッファをローテート
		ColorRGBint16 *tmp = errbuf[0];
		for (int i = 0; i < errbuf_count - 1; i++) {
			errbuf[i] = errbuf[i + 1];
		}
		errbuf[errbuf_count - 1] = tmp;
		memset(errbuf[errbuf_count - 1], 0, errbuf_len);
	}

	free(errbuf_mem);

	return 0;
}


//////////// JPEG イメージ


typedef struct ImageReductor_Image_t ImageReductor_Image;
typedef int (* ImageReductor_ReadCallback)(ImageReductor_Image* img);

struct ImageReductor_Image_t
{
	uint8_t *Data;
	int32_t DataLen;
	int32_t Width;
	int32_t Height;
	int32_t ChannelCount;
	int32_t RowStride;
	int32_t OriginalWidth;
	int32_t OriginalHeight;

	ImageReductor_ReadCallback ReadCallback;

	// ユーザが自由に使っていい。コールバック元の this 入れるとか。
	void *UserObject;

	uint8_t ReadBuffer[4096];
};

ImageReductor_Image*
ImageReductor_AllocImage()
{
#if 0
fprintf(stderr, "OffsetOf(DataLen)=%d\n", offsetof(ImageReductor_Image, DataLen));
fprintf(stderr, "OffsetOf(Width)=%d\n", offsetof(ImageReductor_Image, Width));
fprintf(stderr, "OffsetOf(Height)=%d\n", offsetof(ImageReductor_Image, Height));
fprintf(stderr, "OffsetOf(ChannelCount)=%d\n", offsetof(ImageReductor_Image, ChannelCount));
fprintf(stderr, "OffsetOf(RowStride)=%d\n", offsetof(ImageReductor_Image, RowStride));
fprintf(stderr, "OffsetOf(OriginalWidth)=%d\n", offsetof(ImageReductor_Image, OriginalWidth));
fprintf(stderr, "OffsetOf(OriginalHeight)=%d\n", offsetof(ImageReductor_Image, OriginalHeight));
fprintf(stderr, "OffsetOf(ReadCallback)=%d\n", offsetof(ImageReductor_Image, ReadCallback));
#endif
	return calloc(1, sizeof(ImageReductor_Image));
}

void
ImageReductor_FreeImage(ImageReductor_Image* img)
{
	if (img->Data != NULL) {
		free(img->Data);
	}
	free(img);
}

static const JOCTET fake_EOI[] = { 0xff, JPEG_EOI };

void
init_source(j_decompress_ptr cinfo)
{
	// nop
//fprintf(stderr, "init_source\n");
}

boolean
fill_input_buffer(j_decompress_ptr cinfo)
{
//fprintf(stderr, "fill_input\n");
	if (cinfo->client_data != NULL) {
		ImageReductor_Image *img = (ImageReductor_Image *)cinfo->client_data;
//fprintf(stderr, "fill_input img=%p\n", img);

		int n = img->ReadCallback(img);
//fprintf(stderr, "callback n=%d\n", n);
//fprintf(stderr, "readbuffer=%02X %02X\n", img->ReadBuffer[0], img->ReadBuffer[1]);
		if (n > 0) {
			cinfo->src->next_input_byte = img->ReadBuffer;
			cinfo->src->bytes_in_buffer = n;
			return TRUE;
		}
	}

	cinfo->src->next_input_byte = fake_EOI;
	cinfo->src->bytes_in_buffer = sizeof(fake_EOI);
	return TRUE;
}

void
skip_input_data(j_decompress_ptr cinfo, long num_bytes)
{
//fprintf(stderr, "skip_input\n");
	while (num_bytes > (long)(cinfo->src->bytes_in_buffer)) {
		num_bytes -= cinfo->src->bytes_in_buffer;
		fill_input_buffer(cinfo);
	}
	cinfo->src->next_input_byte += num_bytes;
	cinfo->src->bytes_in_buffer -= num_bytes;
	return;
}

void
term_source(j_decompress_ptr cinfo)
{
//fprintf(stderr, "term_source\n");
	// nop
}

static void
debug_handler(j_common_ptr cinfo)
{
	char buffer[JMSG_LENGTH_MAX];
	(*cinfo->err->format_message)(cinfo, buffer);

	DEBUG_PRINTF("%s\n", buffer);
}

int
ImageReductor_LoadJpeg(
	ImageReductor_Image* img,
	int requestWidth, int requestHeight)
{
DEBUG_PRINTF("LoadJpeg enter img=%p, w=%d, h=%d\n", img, requestWidth, requestHeight);

	if (img == NULL) return RIC_ARG_NULL;
	if (img->ReadCallback == NULL) return RIC_ARG_NULL;

	struct jpeg_decompress_struct jinfo;
	struct jpeg_error_mgr jerr;
	jinfo.err = jpeg_std_error(&jerr);
	jerr.output_message = debug_handler;

	jpeg_create_decompress(&jinfo);

	jinfo.client_data = img;

	struct jpeg_source_mgr src_mgr;
	memset(&src_mgr, 0, sizeof(src_mgr));

	src_mgr.init_source = init_source;
	src_mgr.fill_input_buffer = fill_input_buffer;
	src_mgr.skip_input_data = skip_input_data;
	src_mgr.resync_to_restart = jpeg_resync_to_restart;
	src_mgr.term_source = term_source;
	src_mgr.bytes_in_buffer = 0;
	src_mgr.next_input_byte = NULL;

	jinfo.src = &src_mgr;

DEBUG_PRINTF("LoadJpeg readheader\n");
	// ヘッダ読み込み
	jpeg_read_header(&jinfo, TRUE);
DEBUG_PRINTF("LoadJpeg readheader OK\n");

	img->OriginalWidth = jinfo.image_width;
	img->OriginalHeight = jinfo.image_height;

	// スケールの計算
	// libjpeg では 1/16 までサポート
	// 1/1, 1/2, 1/4, 1/8 しかサポートしないとも書いてある
	if (requestWidth <= 0) {
		requestWidth = img->OriginalWidth;
	}
	if (requestHeight <= 0) {
		requestHeight = img->OriginalHeight * requestWidth / img->OriginalWidth;
		if (requestHeight <= 0) {
			requestHeight = 1;
		}
	}
	int scalew = img->OriginalWidth / requestWidth;
	int scaleh = img->OriginalHeight / requestHeight;
	int scale = scalew < scaleh ? scalew : scaleh;
	if (scale < 1) {
		scale = 1;
	} else if (scale > 16) {
		scale = 16;
	}

DEBUG_PRINTF("LoadJpeg org=(%d,%d) scalewh=(%d,%d) scale=%d\n", img->OriginalWidth, img->OriginalHeight, scalew, scaleh, scale);

	jinfo.scale_num = 1;
	jinfo.scale_denom = scale;

	jinfo.do_fancy_upsampling = FALSE;
	jinfo.do_block_smoothing = FALSE;
	jinfo.dct_method = JDCT_FASTEST;
	jinfo.out_color_space = JCS_RGB;
	jinfo.output_components = 3;

	jpeg_calc_output_dimensions(&jinfo);

	img->Width = jinfo.output_width;
	img->Height = jinfo.output_height;
	img->ChannelCount = jinfo.output_components;
	img->RowStride = jinfo.output_width * jinfo.output_components;

	img->DataLen = img->RowStride * img->Height;
	img->Data = malloc(img->DataLen);

DEBUG_PRINTF("LoadJpeg dim wh=(%d,%d) datalen=%d\n", img->Width, img->Height, img->DataLen);

	// スキャンラインメモリのポインタ配列が必要
	JSAMPARRAY lines = malloc(jinfo.output_height * sizeof(uint8_t *));
	for (int y = 0; y < jinfo.output_height; y++) {
		lines[y] = img->Data + (y * img->RowStride);
	}

DEBUG_PRINTF("LoadJpeg startdecompress\n");
	jpeg_start_decompress(&jinfo);
DEBUG_PRINTF("LoadJpeg startdecompress OK\n");

	while (jinfo.output_scanline < jinfo.output_height) {
		int prev_scanline = jinfo.output_scanline;

		jpeg_read_scanlines(&jinfo,
			&lines[jinfo.output_scanline],
			jinfo.rec_outbuf_height);

		if (prev_scanline == jinfo.output_scanline) {
			// スキャンラインが進まない
			jpeg_abort_decompress(&jinfo);
			free(lines);
			return RIC_ABORT_JPEG;
		}
	}

DEBUG_PRINTF("LoadJpeg finishdecompress\n");
	jpeg_finish_decompress(&jinfo);
DEBUG_PRINTF("LoadJpeg finishdecompress OK\n");
	free(lines);

	return RIC_OK;
}

