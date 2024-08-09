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
	ResizeAxis_Both = 0,

	ResizeAxis_Width,

	ResizeAxis_Height,

	ResizeAxis_Long,

	ResizeAxis_Short,

	ResizeAxis_ScaleDownBoth = 8,

	ResizeAxis_ScaleDownWidth,

	ResizeAxis_ScaleDownHeight,

	ResizeAxis_ScaleDownLong,

	ResizeAxis_ScaleDownShort,

	ResizeAxis_ScaleDownBit = 0x08,
} ResizeAxis;

// 減色&リサイズ方法。
typedef enum {
	ReductorMethod_Fast,

	// 単純間引き。
	ReductorMethod_Simple,

	ReductorMethod_HighQuality,
} ReductorMethod;

// 誤差拡散アルゴリズム
typedef enum {
	RDM_FS,			// Floyd Steinberg
	RDM_ATKINSON,	// Atkinson
	RDM_JAJUNI,		// Jarvis, Judice, Ninke
	RDM_STUCKI,		// Stucki
	RDM_BURKES,		// Burkes
	RDM_2,			// (x+1,y), (x,y+1)
	RDM_3,			// (x+1,y), (x,y+1), (x+1,y+1)
	RDM_RGB,		// RGB color sepalated
} ReductorDiffuse;

// 色モードは下位8ビットが enum。
// Gray、GrayMean では bit15-8 の 8ビットに「階調-1」(1-255) を格納する。
typedef enum {
	ReductorColor_Mono,		// 二値
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
		uint8 x;
		uint8 r;
		uint8 g;
		uint8 b;
	};
} ColorRGB;

#if BYTE_ORDER == LITTLE_ENDIAN
#define RGBToU32(r, g, b)	((uint32)(((r) << 8) | ((g) << 16) | ((b) << 24)))
#else
#define RGBToU32(r, g, b)	((uint32)(((r) << 16) | ((g) << 8) | (b)))
#endif

struct image
{
	// buf はラスターパディングなし。
	// ストライドを足しやすいように uint8 のポインタにしておく。
	// channels==1 ならインデックス。
	// channels==3 なら R, G, B 順。
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
extern struct image *image_create_fp(FILE *, const struct diag *);
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
