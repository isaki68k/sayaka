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
// 画像処理
//

#ifndef sayaka_image_h
#define sayaka_image_h

#include "header.h"

struct diag;
typedef struct string_ string;

// リサイズの基準軸。
typedef enum {
	// 幅が ResizeWidth になり、
	// 高さが ResizeHeight になるようにリサイズする。
	// ResizeWidth == 0 のときは Height と同じ動作をする。
	// ResizeHeight == 0 のときは Width と同じ動作をする。
	// ResizeWidth と ResizeHeight の両方が 0 のときは原寸大。
	ResizeAxis_Both = 0,

	// 幅が ResizeWidth になるように縦横比を保持してリサイズする。
	// ResizeWidth == 0 のときは原寸大。
	ResizeAxis_Width,

	// 高さが ResizeHeight になるように縦横比を保持してリサイズする。
	// ResizeHeight == 0 のときは原寸大。
	ResizeAxis_Height,

	// 長辺優先リサイズ。
	// 原寸 Width >= Height のときは Width と同じ動作をする。
	// 原寸 Width < Height のときは Height と同じ動作をする。
	// 例:
	// 長辺を特定のサイズにしたい場合は、ResizeWidth と ResizeHeight に
	// 同じ値を設定する。
	ResizeAxis_Long,

	// 短辺優先リサイズ。
	// 原寸 Width <= Height のときは Width と同じ動作をする。
	// 原寸 Width > Height のときは Height と同じ動作をする。
	ResizeAxis_Short,

	// 縮小のみの Both。
	// 幅が ResizeWidth より大きいときは ResizeWidth になり、
	// 高さが ResizeHeight より大きいときは ResizeHeight になるように
	// リサイズする。
	// ResizeWidth == 0 のときは ScaleDownHeight と同じ動作をする。
	// ResizeHeight == 0 のときは ScaleDownWidth と同じ動作をする。
	// ResizeWidth と ResizeHeight の両方が 0 のときは原寸大。
	ResizeAxis_ScaleDownBoth = 8,

	// 縮小のみの Width。
	// 幅が ResizeWidth より大きいときは ResizeWidth になるように
	// 縦横比を保持してリサイズする。
	// ResizeWidth == 0 のときは原寸大。
	ResizeAxis_ScaleDownWidth,

	// 縮小のみの Height。
	// 幅が ResizeHeight より大きいときは ResizeHeight になるように
	// 縦横比を保持してリサイズする。
	// ResizeHeight == 0 のときは原寸大。
	ResizeAxis_ScaleDownHeight,

	// 縮小のみの長辺優先リサイズ。
	// 原寸 Width >= Height のときは ScaleDownWidth と同じ動作をする。
	// 原寸 Width < Height のときは ScaleDownHeight と同じ動作をする。
	// 例:
	// 長辺を特定のサイズ以下にしたい場合は、ResizeWidth と ResizeHeight に
	// 同じ値を設定する。
	ResizeAxis_ScaleDownLong,

	// 縮小のみの短辺優先リサイズ。
	// 原寸 Width <= Height のときは ScaleDownWidth と同じ動作をする。
	// 原寸 Width > Height のときは ScaleDownHeight と同じ動作をする。
	ResizeAxis_ScaleDownShort,

	// ScaleDown* を判定するためのビットマスク。内部で使用。
	ResizeAxis_ScaleDownBit = 0x08,
} ResizeAxis;

// 減色&リサイズ方法。
typedef enum {
	ReductorMethod_Simple,		// 単純一致法
	ReductorMethod_Fast,		// 速度優先法
	ReductorMethod_HighQuality,	// 二次元誤差分散法
} ReductorMethod;

// 誤差拡散アルゴリズム
typedef enum {
	RDM_FS,			// Floyd Steinberg
	RDM_ATKINSON,	// Atkinson
	RDM_JAJUNI,		// Jarvis, Judice, Ninke
	RDM_STUCKI,		// Stucki
	RDM_BURKES,		// Burkes
	RDM_2,			// 2 pixels (right, down)
	RDM_3,			// 3 pixels (right, down, rightdown)
	RDM_RGB,		// RGB color sepalated
} ReductorDiffuse;

// 色モードは下位8ビットが enum。
// Gray、GrayMean では bit15-8 の 8ビットに「階調-1」(1-255) を格納する。
typedef enum {
	ReductorColor_Gray,		// グレイスケール
	ReductorColor_GrayMean,	// グレイスケール()
	ReductorColor_Fixed8,	// RGB 8色
	ReductorColor_X68k,		// X68030?
	ReductorColor_ANSI16,	// ANSI 16色 (といっても色合いが全員違う?)
	ReductorColor_Fixed256,	// 固定 256 色
	ReductorColor_Fixed256I,// ?

	// 下位8ビットが色モード
	ReductorColor_MASK = 0xff,
} ReductorColor;

typedef union {
	uint32 u32;
	struct {
		uint8 r;
		uint8 g;
		uint8 b;
		uint8 a;
	};
} ColorRGB;

#if BYTE_ORDER == LITTLE_ENDIAN
#define RGBToU32(r, g, b)	((uint32)(((r) <<  0) | ((g) << 8) | ((b) << 16)))
#else
#define RGBToU32(r, g, b)	((uint32)(((r) << 24) | ((g) << 16) | ((b) << 8)))
#endif

struct image
{
	// buf はラスターパディングなし。
	// ストライドを足しやすいように uint8 のポインタにしておく。
	// channels==1 ならインデックス。
	// channels==3 なら R, G, B 順。
	// channels==4 なら R, G, B, A 順。
	uint8 *buf;

	uint width;		// ピクセル幅
	uint height;	// ピクセル高さ
	uint channels;	// 1ピクセルあたりのチャンネル数

	// インデックスカラー(channels==1) の場合に使用したパレット。
	// インデックスカラーでない場合は参照しないこと。
	const ColorRGB *palette;

	// パレット数。インデックスカラーでない場合は参照しないこと。
	uint palette_count;

	// 動的に作成したパレットはここで所有する。
	ColorRGB *palette_buf;
};

// image.c
struct image_reduct_param {
	ReductorMethod method;
	ReductorDiffuse diffuse;
	ReductorColor color;
};
extern struct image *image_create(uint width_, uint height_, uint channels_);
extern struct image *image_read_fp(FILE *, const struct diag *);
extern void image_free(struct image *);
extern uint image_get_stride(const struct image *);
extern void image_get_preferred_size(uint current_width, uint current_height,
	ResizeAxis axis, uint request_width, uint request_height,
	uint *preferred_width, uint *preferred_height);
extern string *image_get_loaderinfo(void);
extern struct image *image_coloring(const struct image *);
extern struct image *image_reduct(const struct image *src,
	uint dst_width, uint dst_height, const struct image_reduct_param *,
	const struct diag *);

extern const char *resizeaxis_tostr(ResizeAxis);
extern const char *reductormethod_tostr(ReductorMethod);
extern const char *reductordiffuse_tostr(ReductorDiffuse);
extern const char *reductorcolor_tostr(ReductorColor);

// image_sixel.c
struct sixel_opt {
	bool output_ormode;
	bool disable_palette;
};
extern bool image_sixel_write(FILE *, const struct image *,
	const struct sixel_opt *, const struct diag *);

#endif // !sayaka_image_h
