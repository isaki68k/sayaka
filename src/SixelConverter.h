/*
 * Copyright (C) 2015 Y.Sugahara (moveccr)
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

#include "Diag.h"
#include "ImageReductor.h"
#include "StreamBase.h"
#include <vector>

// SIXEL 出力モード。
// SIXEL のカラーモード値と同じにする。
enum SixelOutputMode {
	Normal = 1,	// 通常の SIXEL を出力する。
	Or = 5,		// OR モード SIXEL を出力する。
};

// リサイズモード
enum SixelResizeMode
{
	// リサイズ処理をロード時にライブラリで行う。
	ByLoad,

	// ロードは等倍で行い、その後にリサイズ処理を ImageReductor で行う。
	ByImageReductor,
};

class SixelConverter
{
 public:
	// コンストラクタ
	SixelConverter();
	SixelConverter(int debuglv);

	// stream から読み込む
	bool LoadFromStream(InputStream *stream);

	// インデックスカラーに変換する
	void ConvertToIndexed();

	// Sixel を stream に出力する
	void SixelToStream(OutputStream *stream);

	// 画像の幅・高さを取得する
	int GetWidth() const { return Width; }
	int GetHeight() const { return Height; }

	// ImageReductor を取得する
	ImageReductor& GetImageReductor() { return ir; }

	// ----- 設定

	// Sixel の出力カラーモード値
	SixelOutputMode OutputMode = SixelOutputMode::Normal;

	// Sixel にパレットを出力するなら true
	bool OutputPalette = true;

	// カラーモード
	ReductorColorMode ColorMode = ReductorColorMode::Fixed256;

	// ファインダーモード
	ReductorFinderMode FinderMode = ReductorFinderMode::RFM_Default;

	// グレーカラーの時の色数。グレー以外の時は無視される。
	int GrayCount = 256;

	// 減色モード
	ReductorReduceMode ReduceMode = ReductorReduceMode::HighQuality;

	// リサイズモード
	SixelResizeMode ResizeMode = SixelResizeMode::ByLoad;

	// ノイズ付加
	// ベタ塗り画像で少ない色数に減色する時、ノイズを付加すると画質改善出来る
	int AddNoiseLevel {};

	// リサイズ情報 (リサイズで希望する幅と高さ)。
	// 0 を指定するとその情報は使われない。
	int ResizeWidth {};
	int ResizeHeight {};

	// リサイズ処理で使用する軸
	ResizeAxisMode ResizeAxis = ResizeAxisMode::Both;

 public:
	// インデックスカラー画像バッファ
	std::vector<uint8> Indexed {};

 private:
	void LoadAfter();

	void CalcResize(int *width, int *height);

	std::string SixelPreamble();
	void SixelToStreamCore_ORmode(OutputStream *stream);
	void SixelToStreamCore(OutputStream *stream);
	std::string SixelPostamble();
	static std::string SixelRepunit(int n, uint8 ptn);

	ImageReductor ir {};

	// 元画像
	Image img {};

	// (出力する画像の)幅と高さ
	// リサイズしなければ img.Size.{w,h} と同じ
	int Width {};
	int Height {};

	Diag diag {};

 public:
	// enum 対応
	static const char *SOM2str(SixelOutputMode val);
	static const char *SRM2str(SixelResizeMode val);
};

// SixelConverterOR.cpp
extern int sixel_image_to_sixel_h6_ormode(uint8* dst, const uint8* src,
	int w, int h, int plane_count);

#if defined(SELFTEST)
extern void test_SixelConverter();
#endif
