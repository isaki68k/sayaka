/*
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
#include "Diag.h"
#include <vector>

class PeekableStream;

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

struct Size
{
	int w {};
	int h {};
};

// 画像
// 今の所扱うのは RGB24 形式、パディングなしの画像フォーマットのみ。
// つまり channels = 3, bit_depth = 8, stride = width * 3 固定。
class Image
{
 public:
	// 空のビットマップを作成
	Image();
	// 指定の大きさのビットマップを作成 (ゼロ初期化)
	Image(int width, int height);

	// 初期化
	void Create(int width, int height);

	~Image();

	uint8 *GetBuf()			{ return buf.data(); }

	Size GetSize() const	{ return size; }
	int GetWidth() const	{ return size.w; }
	int GetHeight() const	{ return size.h; }
	int GetStride() const	{ return GetWidth() * GetChannels(); }
	int GetChannels() const	{ return 3; }
	int GetChDepth() const 	{ return 8; }

	std::vector<uint8> buf {};
	Size size {};			// 画像サイズ (pixel)
};

//
// 画像ローダの基本クラス
//
class ImageLoader
{
 public:
	ImageLoader(PeekableStream *stream, const Diag& diag);
	virtual ~ImageLoader();

	// サポートしている画像形式なら true を返す。
	// ストリームは必要なら呼び出し側が巻き戻すこと。
	virtual bool Check() const = 0;

	virtual bool Load(Image& img) = 0;

	// 共通パラメータ
	int resize_width {};
	int resize_height {};
	ResizeAxisMode resize_axis {};

 protected:
	// 所有していない。
	PeekableStream *stream {};

	Diag diag {};
};
