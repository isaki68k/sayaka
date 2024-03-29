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

#include "header.h"
#include "Image.h"
#include <vector>

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

// ----- 色の型

struct ColorRGBint {
	int r;
	int g;
	int b;
};

struct ColorRGBuint8 {
	uint8 r;
	uint8 g;
	uint8 b;
};

struct ColorRGBint8 {
	int8 r;
	int8 g;
	int8 b;
};

struct ColorRGBint16 {
	int16 r;
	int16 g;
	int16 b;
};

struct ColorHSVuint8 {
	uint8 h;	// 0..239, 255=gray
	uint8 s;	// 0..255
	uint8 v;	// 0..255
};

class ImageReductor
{
	using FindColorFunc_t = int (ImageReductor::*)(ColorRGBuint8);

 public:
	// デバッグレベル設定
	void Init(const Diag& diag);

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
	void Convert(ReductorReduceMode mode, Image& img,
		std::vector<uint8>& dst, int toWidth, int toHeight);

	void ColorFactor(float factor);

	// High 誤差分散アルゴリズム
	ReductorDiffuseMethod HighQualityDiffuseMethod = RDM_FS;

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

	static uint8 Saturate_uint8(int x);
	static int RoundDownPow2(int x);
	static int rnd(int level);

	// 高速変換を行う
	void ConvertFast(Image& img, std::vector<uint8>& dst,
		int toWidth, int toHeight);

	// 単純変換を行う
	void ConvertSimple(Image& img, std::vector<uint8>& dst,
		int toWidth, int toHeight);

	// 高品質変換を行う
	void ConvertHighQuality(Image& img, std::vector<uint8>& dst,
		int toWidth, int toHeight);

	static int16 Saturate_adderr(int16 a, int b);
	static void set_err(ColorRGBint16 eb[], int x, ColorRGBint col, int ratio);

	Diag diag {};

 public:
	// enum 対応
	static const char *RRM2str(ReductorReduceMode n);
	static const char *RCM2str(ReductorColorMode n);
	static const char *RFM2str(ReductorFinderMode n);
	static const char *RDM2str(ReductorDiffuseMethod n);
	static const char *RAX2str(ResizeAxisMode n);
};
