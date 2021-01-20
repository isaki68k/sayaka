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

#pragma once

#include "sayaka.h"
#include <vector>
#include <gdk-pixbuf/gdk-pixbuf.h>

// 減色モード
enum ReductorReduceMode {
	Fast,			// 速度優先法
	Simple,			// 単純一致法
	HighQuality,	// 二次元誤差分散法
};

// カラーモード
enum ReductorColorMode {
	RCM_Mono,
	RCM_Gray,
	RCM_GrayMean,
	RCM_Fixed8,
	RCM_FixedX68k,
	RCM_FixedANSI16,
	RCM_Fixed256,
	RCM_Fixed256RGBI,
	RCM_Custom,

	Mono			= RCM_Mono,
	Gray			= RCM_Gray,
	GrayMean		= RCM_GrayMean,
	Fixed8			= RCM_Fixed8,
	FixedX68k		= RCM_FixedX68k,
	FixedANSI16		= RCM_FixedANSI16,
	Fixed256		= RCM_Fixed256,
	Fixed256RGBI	= RCM_Fixed256RGBI,
	Custom			= RCM_Custom,
};

// ファインダーモード
enum ReductorFinderMode {
	RFM_Default,
	RFM_HSV,
};

// リターンコード
enum ReductorImageCode {
	RIC_OK = 0,
	RIC_ARG_NULL = 1,
	RIC_ABORT_JPEG = 2,
};

// 誤差拡散アルゴリズム
enum ReductorDiffuseMethod {
	RDM_FS,			// Floyd Steinberg
	RDM_ATKINSON,	// Atkinson
	RDM_JAJUNI,		// Jarvis, Judice, Ninke
	RDM_STUCKI,		// Stucki
	RDM_BURKES,		// Burkes
	RDM_2,			// (x+1,y), (x,y+1)
	RDM_3,			// (x+1,y), (x,y+1), (x+1,y+1)
	RDM_RGB,		// RGB color sepalated
};

enum ResizeAxisMode {
	// 幅が ResizeWidth になり、
	// 高さが ResizeHeight になるようにリサイズする。
	// ResizeWidth == 0 のときは Height と同じ動作をする。
	// ResizeHeight == 0 のときは Width と同じ動作をする。
	// ResizeWidth と ResizeHeight の両方が 0 のときは原寸大。
	Both,

	// 幅が ResizeWidth になるように縦横比を保持してリサイズする。
	// ResizeWidth == 0 のときは原寸大。
	Width,

	// 高さが ResizeHeight になるように縦横比を保持してリサイズする。
	// ResizeHeight == 0 のときは原寸大。
	Height,

	// 長辺優先リサイズ
	// 原寸 Width >= Height のときは Width と同じ動作をする。
	// 原寸 Width < Height のときは Height と同じ動作をする。
	// 例:
	// 長辺を特定のサイズにしたい場合は、ResizeWidth と ResizeHeight に
	// 同じ値を設定する。
	Long,

	// 短辺優先リサイズ
	// 原寸 Width <= Height のときは Width と同じ動作をする。
	// 原寸 Width > Height のときは Height と同じ動作をする。
	Short,

	// 縮小のみの Both
	// 幅が ResizeWidth より大きいときは ResizeWidth になり、
	// 高さが ResizeHeight より大きいときは ResizeHeight になるように
	// リサイズする。
	// ResizeWidth == 0 のときは ScaleDownHeight と同じ動作をする。
	// ResizeHeight == 0 のときは ScaleDownWidth と同じ動作をする。
	// ResizeWidth と ResizeHeight の両方が 0 のときは原寸大。
	ScaleDownBoth,

	// 縮小のみの Width
	// 幅が ResizeWidth より大きいときは ResizeWidth になるように
	// 縦横比を保持してリサイズする。
	// ResizeWidth == 0 のときは原寸大。
	ScaleDownWidth,

	// 縮小のみの Height
	// 幅が ResizeHeight より大きいときは ResizeHeight になるように
	// 縦横比を保持してリサイズする。
	// ResizeHeight == 0 のときは原寸大。
	ScaleDownHeight,

	// 縮小のみの長辺優先リサイズ
	// 原寸 Width >= Height のときは ScaleDownWidth と同じ動作をする。
	// 原寸 Width < Height のときは ScaleDownHeight と同じ動作をする。
	// 例:
	// 長辺を特定のサイズ以下にしたい場合は、ResizeWidth と ResizeHeight に
	// 同じ値を設定する。
	ScaleDownLong,

	// 縮小のみの短辺優先リサイズ
	// 原寸 Width <= Height のときは ScaleDownWidth と同じ動作をする。
	// 原寸 Width > Height のときは ScaleDownHeight と同じ動作をする。
	ScaleDownShort,
};

// ----- 色の型

struct ColorRGBint {
	int r;
	int g;
	int b;
};

struct ColorRGBuint8 {
	uint8_t r;
	uint8_t g;
	uint8_t b;
};

struct ColorRGBint8 {
	int8_t r;
	int8_t g;
	int8_t b;
};

struct ColorRGBint16 {
	int16_t r;
	int16_t g;
	int16_t b;
};

struct ColorHSVuint8 {
	uint8_t h;	// 0..239, 255=gray
	uint8_t s;	// 0..255
	uint8_t v;	// 0..255
};

class ImageReductor
{
	using FindColorFunc_t = int (ImageReductor::*)(ColorRGBuint8);

 public:
	struct Image;
	struct Image
	{
		uint8_t *Data;
		int32_t DataLen;
		int32_t Width;
		int32_t Height;
		int32_t ChannelCount;
		int32_t RowStride;
		int32_t OriginalWidth;
		int32_t OriginalHeight;

		int (*ReadCallback)(Image *);
		// ユーザが自由に使っていい。
		void *UserObject;

		uint8_t ReadBuffer[4096];
	};

 public:
	// パレット数を取得
	int GetPaletteCount() const { return PaletteCount; }

	// n 番目のパレットデータを取得
	ColorRGBuint8 GetPalette(int n) const { return Palette[n]; }

	// カラーモードを設定する。
	// 変換関数を呼び出す前に、必ずカラーモードを設定すること。
	// count はグレースケールでのみ使用。
	void SetColorMode(ReductorColorMode mode, ReductorFinderMode finder,
		int count = 0);

	// ノイズ付加モードを設定する
	void SetAddNoiseLevel(int level) {
		AddNoiseLevel = level;
	}

	// 変換
	void Convert(ReductorReduceMode mode, GdkPixbuf *pix,
		std::vector<uint8_t>& dst, int toWidth, int toHeight);

	void ColorFactor(float factor);

	// High 誤差分散アルゴリズム
	ReductorDiffuseMethod HighQualityDiffuseMethod = RDM_FS;

	static int Debug;

 private:
	int PaletteCount {};

	const ColorRGBuint8 *Palette {};

	int AddNoiseLevel {};

	// 可変パレット用バッファ
	ColorRGBuint8 Palette_Custom[256] {};

	// 色変換関数の関数ポインタ
	FindColorFunc_t ColorFinder {};

	// 固定2色パレット
	static const ColorRGBuint8 Palette_Mono[];
	int FindColor_Mono(ColorRGBuint8 c);

	// グレースケールパレット
	void SetPalette_Gray(int count);
	int FindColor_Gray(ColorRGBuint8 c);
	int FindColor_GrayMean(ColorRGBuint8 c);

	// 固定8色パレット
	static const ColorRGBuint8 Palette_Fixed8[];
	int FindColor_Fixed8(ColorRGBuint8 c);

	// X68k 固定 16 色パレット
	static const ColorRGBuint8 Palette_FixedX68k[];
	int FindColor_FixedX68k(ColorRGBuint8 c);

	// ANSI 固定 16 色パレット
	static const ColorRGBuint8 Palette_FixedANSI16[];
	int FindColor_FixedANSI16(ColorRGBuint8 c);

	// 固定 256 色 (RGB) パレット
	void SetPalette_Fixed256();
	int FindColor_Fixed256(ColorRGBuint8 c);

	// 固定 256 色 (RGBI) パレット
	void SetPalette_Fixed256RGBI();
	int FindColor_Fixed256RGBI(ColorRGBuint8 c);

	// 円錐型 HSV パレット
	int FindColor_HSV(ColorRGBuint8 c);
	static ColorHSVuint8 RGBtoHSV(ColorRGBuint8 c);
	void CreateHSVPalette();
	static int FindColor_HSV_subr(ColorHSVuint8 hsvpal, ColorHSVuint8 hsv);
	ColorHSVuint8 HSVPalette[256];

	//
	// 変換関数
	//

	static uint8_t Saturate_uint8(int x);
	static int RoundDownPow2(int x);
	static int rnd(int level);

	// 高速変換を行う
	void ConvertFast(GdkPixbuf *pix, std::vector<uint8_t>& dst,
		int toWidth, int toHeight);

	// 単純変換を行う
	void ConvertSimple(GdkPixbuf *pix, std::vector<uint8_t>& dst,
		int toWidth, int toHeight);

	// 高品質変換を行う
	void ConvertHighQuality(GdkPixbuf *pix, std::vector<uint8_t>& dst,
		int toWidth, int toHeight);

	static int16_t Saturate_adderr(int16_t a, int b);
	static void set_err(ColorRGBint16 eb[], int x, ColorRGBint col, int ratio);

	//
	// JPEG イメージ
	//
 public:
	static Image *AllocImage();
	static void FreeImage(Image *img);

	static ReductorImageCode LoadJpeg(Image *img,
		int requestWidth, int requestHeight, ResizeAxisMode requestAxis);

 private:
	static void calcResize(int *req_w, int *req_h, int req_ax,
		int org_w, int org_h);

 public:
	// enum 対応
	static const char *RRM2str(ReductorReduceMode n);
	static const char *RCM2str(ReductorColorMode n);
	static const char *RFM2str(ReductorFinderMode n);
	static const char *RDM2str(ReductorDiffuseMethod n);
	static const char *RAX2str(ResizeAxisMode n);
};

#if defined(SELFTEST)
extern void test_ImageReductor();
#endif
