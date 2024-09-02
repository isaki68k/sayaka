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

typedef struct diag_ diag;
typedef struct pstream_ pstream;
typedef struct string_ string;
typedef union ColorRGB_ ColorRGB;

// リサイズの基準軸。
typedef enum {
	// 幅が width になり、高さが height になるようにリサイズする。
	// width == 0 のときは HEIGHT と同じ動作をする。
	// height == 0 のときは WIDTH と同じ動作をする。
	// width と height の両方が 0 のときは原寸大。
	RESIZE_AXIS_BOTH = 0,

	// 幅が width になるように縦横比を保持してリサイズする。
	// width == 0 のときは原寸大。
	RESIZE_AXIS_WIDTH,

	// 高さが height になるように縦横比を保持してリサイズする。
	// height == 0 のときは原寸大。
	RESIZE_AXIS_HEIGHT,

	// 長辺優先リサイズ。
	// 原寸 width >= height のときは WIDTH と同じ動作をする。
	// 原寸 width < height のときは HEIGHT と同じ動作をする。
	// 例:
	// 長辺を特定のサイズにしたい場合は width と height に同じ値を設定する。
	RESIZE_AXIS_LONG,

	// 短辺優先リサイズ。
	// 原寸 width <= height のときは WIDTH と同じ動作をする。
	// 原寸 width > height のときは HEIGHT と同じ動作をする。
	RESIZE_AXIS_SHORT,

	// 縮小のみの BOTH。
	// 幅が width より大きいときは width になり、
	// 高さが height より大きいときは height になるようにリサイズする。
	// width == 0 のときは SCALEDOWN_HEIGHT と同じ動作をする。
	// height == 0 のときは SCALEDOWN_WIDTH と同じ動作をする。
	// width と height の両方が 0 のときは原寸大。
	RESIZE_AXIS_SCALEDOWN_BOTH = 8,

	// 縮小のみの WIDTH。
	// 幅が width より大きいときは width になるように縦横比を保持して
	// リサイズする。
	// width == 0 のときは原寸大。
	RESIZE_AXIS_SCALEDOWN_WIDTH,

	// 縮小のみの HEIGHT。
	// 幅が height より大きいときは height になるように縦横比を保持して
	// リサイズする。
	// height == 0 のときは原寸大。
	RESIZE_AXIS_SCALEDOWN_HEIGHT,

	// 縮小のみの長辺優先リサイズ。
	// 原寸 width >= height のときは SCALEDOWN_WIDTH と同じ動作をする。
	// 原寸 width < height のときは SCALEDOWN_HEIGHT と同じ動作をする。
	// 例:
	// 長辺を特定のサイズ以下にしたい場合は width と height に同じ値を設定する。
	RESIZE_AXIS_SCALEDOWN_LONG,

	// 縮小のみの短辺優先リサイズ。
	// 原寸 width <= height のときは SCALEDOWN_WIDTH と同じ動作をする。
	// 原寸 width > height のときは SCALEDOWN_HEIGHT と同じ動作をする。
	RESIZE_AXIS_SCALEDOWN_SHORT,

	// SCALEDOWN* を判定するためのビットマスク。内部で使用。
	RESIZE_AXIS_SCALEDOWN_BIT = 0x08,
} ResizeAxis;

// 減色&リサイズ方法。
typedef enum {
	REDUCT_SIMPLE,			// 単純一致法
	REDUCT_FAST,			// 速度優先法
	REDUCT_HIGH_QUALITY,	// 二次元誤差分散法
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
#define ReductorColor_GrayLevel(n)	\
	(ReductorColor_Gray | (((unsigned int)n - 1) << 8))

#if BYTE_ORDER == LITTLE_ENDIAN
#define RGBToU32(r, g, b)	((uint32)(((r) <<  0) | ((g) << 8) | ((b) << 16)))
#else
#define RGBToU32(r, g, b)	((uint32)(((r) << 24) | ((g) << 16) | ((b) << 8)))
#endif

typedef struct image_
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
} image;

typedef struct image_opt_ {
	// 減色
	ReductorMethod method;
	ReductorDiffuse diffuse;
	ReductorColor color;

	// 出力ゲイン。0 .. 512 で指定する。256 が 1.0倍。
	uint gain;

	// SIXEL 出力
	bool output_ormode;
	bool suppress_palette;
} image_opt;

// image.c
extern void image_opt_init(image_opt *);
extern image *image_read_pstream(pstream *, const diag *);
extern void image_free(image *);
extern uint image_get_stride(const image *);
extern void image_get_preferred_size(uint, uint, ResizeAxis,
	uint, uint, uint *, uint *);
extern string *image_get_loaderinfo(void);
extern image *image_reduct(const image *, uint, uint, const image_opt *,
	const diag *);

extern const char *resizeaxis_tostr(ResizeAxis);
extern const char *reductormethod_tostr(ReductorMethod);
extern const char *reductordiffuse_tostr(ReductorDiffuse);
extern const char *reductorcolor_tostr(ReductorColor);

// image_blurhash.c
extern image *image_blurhash_read(FILE *, int, int, const diag *);

// image_sixel.c
extern void image_sixel_abort(FILE *);
extern bool image_sixel_write(FILE *, const image *, const image_opt *,
	const diag *);

#endif // !sayaka_image_h
