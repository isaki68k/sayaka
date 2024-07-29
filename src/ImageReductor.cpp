/*
 * Copyright (C) 2016 Y.Sugahara (moveccr)
 * Copyright (C) 2021 Tetsuya Isaki
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

#include "ImageReductor.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits.h>

#ifndef __packed
#define __packed __attribute__((__packed__))
#endif

//
// 分数計算機
// DDA 計算の基礎となる I + N / D 型の分数ステップ加減算計算機。
//
struct StepRational
{
	int I;	// 整数項
	int N;	// 分子
	int D;	// 分母
};

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
StepRationalAdd(StepRational *sr, StepRational *x)
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


//
// ImageReductor
//

// 初期化
void
ImageReductor::Init(const Diag& diag_)
{
	diag = diag_;
}

//
// パレット
//

// 固定 2 色白黒パレット
/*static*/ const ColorRGBuint8
ImageReductor::Palette_Mono[] = {
	{   0,   0,   0 },
	{ 255, 255, 255 },
};

// 色 c を固定 2 色白黒パレットコードへを変換する。
int
ImageReductor::FindColor_Mono(ColorRGBuint8 c)
{
	return ((int)c.r + (int)c.g + (int)c.b > 128 * 3);
}

// count 階調のグレースケールパレットを設定する。
void
ImageReductor::SetPalette_Gray(int count)
{
	Palette = Palette_Custom;
	PaletteCount = count;
	for (int i = 0; i < count; i++) {
		uint8 c = i * 255 / (count - 1);
		Palette_Custom[i].r = c;
		Palette_Custom[i].g = c;
		Palette_Custom[i].b = c;
	}
}

// グレースケールパレット時に、NTSC 輝度が最も近いパレット番号を返す。
int
ImageReductor::FindColor_Gray(ColorRGBuint8 c)
{
	int I = (((int)c.r * 76 + (int)c.g * 153 + (int)c.b * 26) *
	          (PaletteCount - 1) + (255 / PaletteCount)) / 255 / 255;
	if (I >= PaletteCount)
		return PaletteCount - 1;
	return I;
}

// グレースケールパレット時に、RGB 平均で最も近いパレット番号を返す。
int
ImageReductor::FindColor_GrayMean(ColorRGBuint8 c)
{
	int I = ((int)c.r + (int)c.g + (int)c.b + (255 / PaletteCount) * 3) *
	        (PaletteCount - 1) / 3 / 255;
	if (I >= PaletteCount)
		return PaletteCount - 1;
	return I;
}

// 固定 8 色パレット
/*static*/ const ColorRGBuint8
ImageReductor::Palette_Fixed8[] = {
	{   0,   0,   0 },
	{ 255,   0,   0 },
	{   0, 255,   0 },
	{ 255, 255,   0 },
	{   0,   0, 255 },
	{ 255,   0, 255 },
	{   0, 255, 255 },
	{ 255, 255, 255 },
};

// 色 c を固定 8 色パレットコードへ変換する。
int
ImageReductor::FindColor_Fixed8(ColorRGBuint8 c)
{
	int R = (c.r >= 128);
	int G = (c.g >= 128);
	int B = (c.b >= 128);
	return R + (G << 1) + (B << 2);
}

// X68k 固定 16 色パレット
// NetBSD/x68k デフォルトテキストパレットより
/*static*/ const ColorRGBuint8
ImageReductor::Palette_FixedX68k[] = {
	{   0,   0,   0 },	// 透明
	{ 252,   4,   4 },
	{   4, 252,   4 },
	{ 252, 252,   4 },
	{   4,   4, 252 },
	{ 252,   4, 252 },
	{   4, 252, 252 },
	{ 252, 252, 252 },
	{   4,   4,   4 },	// 黒
	{ 124,   4,   4 },
	{   4, 124,   4 },
	{ 124, 124,   4 },
	{   4,   4, 124 },
	{ 124,   4, 124 },
	{   4, 124, 124 },
	{ 124, 124, 124 },
};

// 色 c を X68k 固定 16 色パレットへ変換する。
int
ImageReductor::FindColor_FixedX68k(ColorRGBuint8 c)
{
	int I = (int)c.r + (int)c.g + (int)c.b;
	int R;
	int G;
	int B;
	if (c.r >= 192 || c.g >= 192 || c.b >= 192) {
		R = (c.r >= 192);
		G = (c.g >= 192);
		B = (c.b >= 192);
		if (R == G && G == B) {
			return 7;
		}
		return R + (G << 1) + (B << 2);
	} else {
		R = (c.r >= 64);
		G = (c.g >= 64);
		B = (c.b >= 64);
		if (R == G && G == B) {
			if (I >= 64 * 3) {
				return 15;
			} else {
				return 8;
			}
		}
		return (R + (G << 1) + (B << 2)) | 8;
	}
}

// ANSI 固定 16 色パレット
// Standard VGA colors を基準とし、
// ただしパレット4 を Brown ではなく Yellow になるようにしてある。
/*static*/ const ColorRGBuint8
ImageReductor::Palette_FixedANSI16[] = {
	{   0,   0,   0 },
	{ 170,   0,   0 },
	{   0, 170,   0 },
	{ 170, 170,   0 },
	{   0,   0, 170 },
	{ 170,   0, 170 },
	{   0, 170, 170 },
	{ 170, 170, 170 },
	{  85,  85,  85 },
	{ 255,  85,  85 },
	{  85, 255,  85 },
	{ 255, 255,  85 },
	{  85,  85, 255 },
	{ 255,  85, 255 },
	{  85, 255, 255 },
	{ 255, 255, 255 },
};

// 色 c を ANSI 固定 16 色パレットへ変換する。
int
ImageReductor::FindColor_FixedANSI16(ColorRGBuint8 c)
{
	int I = (int)c.r + (int)c.g + (int)c.b;
	int R;
	int G;
	int B;
	if (c.r >= 213 || c.g >= 213 || c.b >= 213) {
		R = (c.r >= 213);
		G = (c.g >= 213);
		B = (c.b >= 213);
		if (R == G && G == B) {
			if (I >= 224 * 3) {
				return 15;
			} else {
				return 7;
			}
		}
		return (R + (G << 1) + (B << 2)) | 8;
	} else {
		R = (c.r >= 85);
		G = (c.g >= 85);
		B = (c.b >= 85);
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

// R3,G3,B2 bit の固定 256 色パレットを生成する。
void
ImageReductor::SetPalette_Fixed256()
{
	Palette = Palette_Custom;
	PaletteCount = 256;

	for (int i = 0; i < 256; i++) {
		Palette_Custom[i].r = (((i >> 5) & 0x07) * 255 / 7);
		Palette_Custom[i].g = (((i >> 2) & 0x07) * 255 / 7);
		Palette_Custom[i].b = (((i     ) & 0x03) * 255 / 3);
	}
}

// 固定 256 色時に、c に最も近いパレット番号を返す。
int
ImageReductor::FindColor_Fixed256(ColorRGBuint8 c)
{
	// 0 1 2 3 4 5 6 7 8 9 a b c d e f
	// 0 0 1 1 2 2 3 3 4 4 5 5 6 6 7 7
	int R = c.r >> 5;
	int G = c.g >> 5;
	int B = c.b >> 6;
	return (R << 5) + (G << 2) + B;
}

// R2,G2,B2,I2 bit の 256色固定パレットを生成する。
void
ImageReductor::SetPalette_Fixed256RGBI()
{
	Palette = Palette_Custom;
	PaletteCount = 256;

	for (int i = 0; i < 256; i++) {
		uint8 R, G, B, I;
		R = (i >> 6) & 3;
		G = (i >> 4) & 3;
		B = (i >> 2) & 3;
		I = (i     ) & 3;

		Palette_Custom[i].r = (R << 6) + (I * 63 / 3);
		Palette_Custom[i].g = (G << 6) + (I * 63 / 3);
		Palette_Custom[i].b = (B << 6) + (I * 63 / 3);
	}
}

// R2,G2,B2,I2 bit の 固定256色時に、最も近いパレット番号を返す。
int
ImageReductor::FindColor_Fixed256RGBI(ColorRGBuint8 c)
{
	uint8 R, G, B, I;
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

// 円錐型 HSV を計算する。
// H = 0..239, 255
// S = 0..255
// V = 0..255
/*static*/ ColorHSVuint8
ImageReductor::RGBtoHSV(ColorRGBuint8 c)
{
	ColorHSVuint8 rv;

	int min = std::min(std::min(c.r, c.g), c.b);
	int max = std::max(std::max(c.r, c.g), c.b);
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

// RGB パレットから HSV パレットを作成する。
void
ImageReductor::CreateHSVPalette()
{
	for (int i = 0; i < PaletteCount; i++) {
		HSVPalette[i] = RGBtoHSV(Palette[i]);
	}
}

/*static*/ int
ImageReductor::FindColor_HSV_subr(ColorHSVuint8 hsvpal, ColorHSVuint8 hsv)
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
	if (dh >  120) dh -= 240;
	if (dh < -120) dh += 240;

	d = abs(dh)*(hsv.s + 1) / 32 + abs(ds) * 3 + abs(dv) * 5;
	return d;
}

int
ImageReductor::FindColor_HSV(ColorRGBuint8 c)
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

// カラーモードとカラーファインダを設定する。
void
ImageReductor::SetColorMode(ReductorColorMode mode, ReductorFinderMode finder,
	int count)
{
	switch (mode) {
	 case RCM_Mono:
		Palette = Palette_Mono;
		PaletteCount = 2;
		ColorFinder = &ImageReductor::FindColor_Mono;
		break;
	 case RCM_Gray:
		SetPalette_Gray(count);
		ColorFinder = &ImageReductor::FindColor_Gray;
		break;
	 case RCM_GrayMean:
		SetPalette_Gray(count);
		ColorFinder = &ImageReductor::FindColor_GrayMean;
		break;
	 case RCM_Fixed8:
		Palette = Palette_Fixed8;
		PaletteCount = 8;
		ColorFinder = &ImageReductor::FindColor_Fixed8;
		break;
	 case RCM_FixedX68k:
		Palette = Palette_FixedX68k;
		PaletteCount = 16;
		ColorFinder = &ImageReductor::FindColor_FixedX68k;
		break;
	 case RCM_FixedANSI16:
		Palette = Palette_FixedANSI16;
		PaletteCount = 16;
		ColorFinder = &ImageReductor::FindColor_FixedANSI16;
		break;
	 case RCM_Fixed256:
		SetPalette_Fixed256();
		ColorFinder = &ImageReductor::FindColor_Fixed256;
		break;
	 case RCM_Fixed256RGBI:
		SetPalette_Fixed256RGBI();
		ColorFinder = &ImageReductor::FindColor_Fixed256RGBI;
		break;
	 case RCM_Custom:
		ColorFinder = &ImageReductor::FindColor_HSV;
		break;
	}

	switch (finder) {
	 case RFM_HSV:
		CreateHSVPalette();
		ColorFinder = &ImageReductor::FindColor_HSV;
		break;
	 default:
		break;
	}
}


// その他のサブルーチン

/*static*/ uint8
ImageReductor::Saturate_uint8(int x)
{
	if (x < 0)
		return 0;
	if (x > 255)
		return 255;
	return (uint8)x;
}

/*static*/ int
ImageReductor::RoundDownPow2(int x)
{
	x |= x >> 1;
	x |= x >> 2;
	x |= x >> 4;
	x |= x >> 8;
	x |= x >> 16;
	x += 1;
	return x >> 1;
}

// -level .. +level までの乱数を返します。
/*static*/ int
ImageReductor::rnd(int level)
{
	static uint32 y = (uint32)24539283060L;
	y = y ^ (y << 13);
	y = y ^ (y >> 17);
	y = y ^ (y << 5);
	int rv = (((int)(y >> 4) % ((level + 16) * 2 + 1)) - (level + 16)) / 16;
	return rv;
}

//
// 変換関数
//

void
ImageReductor::Convert(ReductorReduceMode mode, Image& img,
	std::vector<uint8>& dst, int toWidth, int toHeight)
{
	switch (mode) {
	 case ReductorReduceMode::Fast:
		ConvertFast(img, dst, toWidth, toHeight);
		break;
	 case ReductorReduceMode::Simple:
		ConvertSimple(img, dst, toWidth, toHeight);
		break;
	 case ReductorReduceMode::HighQuality:
		ConvertHighQuality(img, dst, toWidth, toHeight);
		break;
	 default:
		Debug(diag, "Unknown ReduceMode=%u", (uint)mode);
		break;
	}
}

// 画像を縮小しながら減色して変換する。
// 出来る限り高速に、それなりの品質で変換する。
// dst : 色コードを出力するバッファ。
//       dstWidth * dstHeight バイト以上を保証すること。
// dstWidth : 出力の幅。
// dstHeight : 出力の高さ。
// src : 入力ピクセルデータ (R,G,B または R,G,B,A)。
// srcWidth : 入力の幅。
// srcHeight : 入力の高さ。
// srcNch : 入力のチャンネル数。3 か 4 を保証すること。
// srcStride : 入力のストライドのバイト長さ。
void
ImageReductor::ConvertFast(Image& img, std::vector<uint8>& dst_,
	int dstWidth, int dstHeight)
{
	uint8 *dst = dst_.data();
	uint8 *src  = img.GetBuf();
	int srcWidth  = img.GetWidth();
	int srcHeight = img.GetHeight();
	int srcStride = img.GetStride();
	int srcNch    = img.GetChannels();

	Debug(diag, "%s dst=(%d,%d) src=(%d,%d)", __func__,
		dstWidth, dstHeight, srcWidth, srcHeight);

	// 螺旋状に一次元誤差分散させる。
	// 当然画像処理的には正しくないが、視覚的にはそんなに遜色が無い。

	ColorRGBint col;
	const int level = 256;

	// 水平方向はスキップサンプリング
	// 垂直方向はスキップサンプリング

	StepRational sr_y = StepRationalCreate(0, 0, dstHeight);
	StepRational sr_ystep = StepRationalCreate(0, srcHeight, dstHeight);

	StepRational sr_x = StepRationalCreate(0, 0, dstWidth);
	StepRational sr_xstep = StepRationalCreate(0, srcWidth, dstWidth);

	for (int y = 0; y < dstHeight; y++) {
		uint8 *srcRaster = &src[sr_y.I * srcStride];
		StepRationalAdd(&sr_y, &sr_ystep);

		sr_x.I = 0;
		sr_x.N = 0;

		ColorRGBint ce = { 0, 0, 0 };

		for (int x = 0; x < dstWidth; x++) {
			int sx0 = sr_x.I;
			StepRationalAdd(&sr_x, &sr_xstep);

			uint8 *srcPix = &srcRaster[sx0 * srcNch];
			col.r = srcPix[0];
			col.g = srcPix[1];
			col.b = srcPix[2];

			col.r += ce.r;
			col.g += ce.g;
			col.b += ce.b;

			ColorRGBuint8 c8 = {
				Saturate_uint8(col.r),
				Saturate_uint8(col.g),
				Saturate_uint8(col.b),
			};

			int colorCode = (this->*(ColorFinder))(c8);

			ce.r = (col.r - Palette[colorCode].r) * level / 256;
			ce.g = (col.g - Palette[colorCode].g) * level / 256;
			ce.b = (col.b - Palette[colorCode].b) * level / 256;

			// ランダムノイズを加える
			if (AddNoiseLevel > 0) {
				ce.r += rnd(AddNoiseLevel);
				ce.g += rnd(AddNoiseLevel);
				ce.b += rnd(AddNoiseLevel);
			}

			*dst++ = colorCode;
		}
	}
}

// 画像を縮小しながら減色して変換する。
// 単純減色法を適用する。
// dst : 色コードを出力するバッファ。
//       dstWidth * dstHeight バイト以上を保証すること。
// dstWidth : 出力の幅。
// dstHeight : 出力の高さ。
// src : 入力ピクセルデータ (R,G,B または R,G,B,A)。
// srcWidth : 入力の幅。
// srcHeight : 入力の高さ。
// srcNch : 入力のチャンネル数。3 か 4 を保証すること。
// srcStride : 入力のストライドのバイト長さ。
void
ImageReductor::ConvertSimple(Image& img, std::vector<uint8>& dst_,
	int dstWidth, int dstHeight)
{
	uint8 *dst = dst_.data();
	uint8 *src  = img.GetBuf();
	int srcWidth  = img.GetWidth();
	int srcHeight = img.GetHeight();
	int srcStride = img.GetStride();
	int srcNch    = img.GetChannels();

	Debug(diag, "%s dst=(%d,%d) src=(%d,%d)", __func__,
		dstWidth, dstHeight, srcWidth, srcHeight);

	// 水平方向はスキップサンプリング
	// 垂直方向はスキップサンプリング

	ColorRGBuint8 col = { 0, 0, 0 };
	StepRational sr_y = StepRationalCreate(0, 0, dstHeight);
	StepRational sr_ystep = StepRationalCreate(0, srcHeight, dstHeight);

	StepRational sr_x = StepRationalCreate(0, 0, dstWidth);
	StepRational sr_xstep = StepRationalCreate(0, srcWidth, dstWidth);

	for (int y = 0; y < dstHeight; y++) {
		uint8 *srcRaster = &src[sr_y.I * srcStride];
		StepRationalAdd(&sr_y, &sr_ystep);

		sr_x.I = 0;
		sr_x.N = 0;

		for (int x = 0; x < dstWidth; x++) {
			int sx0 = sr_x.I;
			StepRationalAdd(&sr_x, &sr_xstep);

			uint8 *srcPix = &srcRaster[sx0 * srcNch];
			col.r = srcPix[0];
			col.g = srcPix[1];
			col.b = srcPix[2];

			int colorCode = (this->*(ColorFinder))(col);

			*dst++ = colorCode;
		}
	}
}

/*static*/ int16
ImageReductor::Saturate_adderr(int16 a, int b)
{
	int16 x = a + b;
	if (x < -512) {
		return -512;
	} else if (x > 511) {
		return 511;
	} else {
		return x;
	}
}

// eb[x] += col * ratio / 256;
/*static*/ void
ImageReductor::set_err(ColorRGBint16 eb[], int x, ColorRGBint col, int ratio)
{
	eb[x].r = Saturate_adderr(eb[x].r,  col.r * ratio / 256);
	eb[x].g = Saturate_adderr(eb[x].g,  col.g * ratio / 256);
	eb[x].b = Saturate_adderr(eb[x].b,  col.b * ratio / 256);
}

// 画像を縮小しながら減色して変換する。
// 二次元誤差分散法を使用して、出来る限り高品質に変換する。
// dst : 色コードを出力するバッファ。
//       dstWidth * dstHeight バイト以上を保証すること。
// dstWidth : 出力の幅。
// dstHeight : 出力の高さ。
// src : 入力ピクセルデータ (R,G,B または R,G,B,A)。
// srcWidth : 入力の幅。
// srcHeight : 入力の高さ。
// srcNch : 入力のチャンネル数。3 か 4 を保証すること。
// srcStride : 入力のストライドのバイト長さ。
void
ImageReductor::ConvertHighQuality(Image& img, std::vector<uint8>& dst_,
	int dstWidth, int dstHeight)
{
	uint8 *dst = dst_.data();
	uint8 *src  = img.GetBuf();
	int srcWidth  = img.GetWidth();
	int srcHeight = img.GetHeight();
	int srcStride = img.GetStride();
	int srcNch    = img.GetChannels();

	Debug(diag, "%s dst=(%p,%d,%d) src=(%p,%d,%d)", __func__,
		dst, dstWidth, dstHeight, src, srcWidth, srcHeight);

	// 水平方向はピクセルを平均
	// 垂直方向はピクセルを平均
	// 真に高品質にするには補間法を適用するべきだがそこまではしない。

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
	errbuf_mem = (ColorRGBint16 *)malloc(errbuf_mem_len);
	memset(errbuf_mem, 0, errbuf_mem_len);
	for (int i = 0; i < errbuf_count; i++) {
		errbuf[i] = errbuf_mem + errbuf_left + errbuf_width * i;
	}

	int isAlpha = 0;
	if (srcNch == 4) {
		isAlpha = 1;
	}

	for (int y = 0; y < dstHeight; y++) {
		int sy0 = sr_y.I;
		StepRationalAdd(&sr_y, &sr_ystep);
		int sy1 = sr_y.I;

		if (sy0 == sy1)
			sy1 += 1;

		sr_x.I = 0;
		sr_x.N = 0;

		for (int x = 0; x < dstWidth; x++) {
			ColorRGBint col = { 0, 0, 0 };
			int alpha = 0;

			int sx0 = sr_x.I;
			StepRationalAdd(&sr_x, &sr_xstep);
			int sx1 = sr_x.I;
			if (sx0 == sx1)
				sx1 += 1;

			// 画素の平均を求める
			for (int sy = sy0; sy < sy1; sy++) {
				uint8 *srcRaster = &src[sy * srcStride];
				uint8 *srcPix = &srcRaster[sx0 * srcNch];
				for (int sx = sx0; sx < sx1; sx++) {
					col.r += srcPix[0];
					col.g += srcPix[1];
					col.b += srcPix[2];
					if (isAlpha) {
						alpha += srcPix[3];
					}
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

			int colorCode;
			if (isAlpha && alpha == 0) {
				// XXX パレットがアルファ対応かとか。
				colorCode = 0;
			} else {
				colorCode = (this->*(ColorFinder))(c8);
			}

			col.r -= Palette[colorCode].r;
			col.g -= Palette[colorCode].g;
			col.b -= Palette[colorCode].b;

			// ランダムノイズを加える
			if (AddNoiseLevel > 0) {
				col.r += rnd(AddNoiseLevel);
				col.g += rnd(AddNoiseLevel);
				col.b += rnd(AddNoiseLevel);
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
		// errbuf[y] には左マージンがあるのを考慮する
		memset(errbuf[errbuf_count - 1] - errbuf_left, 0, errbuf_len);
	}

	free(errbuf_mem);
}


static uint8
saturate_mul_f(uint8 a, float b)
{
	auto f = a * b;
	if (f < 0)
		return 0;
	if (f > 255)
		return 255;
	return (uint8)f;
}

void
ImageReductor::ColorFactor(float factor)
{
	// 定義済み(読み込み専用)パレットを使用中なら、
	// その内容をカスタムパレットに移す。
	if (Palette != Palette_Custom) {
		for (int i = 0; i < PaletteCount; i++) {
			Palette_Custom[i] = Palette[i];
		}
		Palette = Palette_Custom;
	}

	// 現在の値に factor をかける
	for (int i = 0; i < PaletteCount; i++) {
		auto& col = Palette_Custom[i];
		col.r = saturate_mul_f(col.r, factor);
		col.g = saturate_mul_f(col.g, factor);
		col.b = saturate_mul_f(col.b, factor);
	}
}

//
// enum を文字列にしたやつ orz
//

static const char *RRM2str_[] = {
	"Fast",
	"Simple",
	"HighQuality",
};

static const char *RCM2str_[] = {
	"Mono",
	"Gray",
	"GrayMean",
	"Fixed8",
	"FixedX68k",
	"FixedANSI16",
	"Fixed256",
	"Fixed256RGBI",
	"Custom",
};

static const char *RFM2str_[] = {
	"Default",
	"HSV",
};

static const char *RDM2str_[] = {
	"FS",
	"Atkinson",
	"Jajuni",
	"Stucki",
	"Burkes",
	"2",
	"3",
	"RGB",
};

static const char *RAX2str_[] = {
	"Both",
	"Width",
	"Height",
	"Long",
	"Short",
	"ScaleDownBoth",
	"ScaleDownWidth",
	"ScaleDownHeight",
	"ScaleDownLong",
	"ScaleDownShort",
};

/*static*/ const char *
ImageReductor::RRM2str(ReductorReduceMode val)
{
	return ::RRM2str_[(int)val];
}

/*static*/ const char *
ImageReductor::RCM2str(ReductorColorMode val)
{
	return ::RCM2str_[(int)val];
}

/*static*/ const char *
ImageReductor::RFM2str(ReductorFinderMode val)
{
	return ::RFM2str_[(int)val];
}

/*static*/ const char *
ImageReductor::RDM2str(ReductorDiffuseMethod val)
{
	return ::RDM2str_[(int)val];
}

/*static*/ const char *
ImageReductor::RAX2str(ResizeAxisMode val)
{
	return ::RAX2str_[(int)val];
}
