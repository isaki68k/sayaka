/* vi:set ts=4: */
/*
 * Copyright (C) 2024-2025 Tetsuya Isaki
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

#include "common.h"

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
	REDUCT_HIGH_QUALITY,	// 二次元誤差分散法
} ReductorMethod;

// 誤差拡散アルゴリズム
typedef enum {
	DIFFUSE_NONE,		// No Diffusion
	DIFFUSE_SFL,		// Sierra Filter Lite
	DIFFUSE_FS,			// Floyd Steinberg
	DIFFUSE_ATKINSON,	// Atkinson
	DIFFUSE_JAJUNI,		// Jarvis, Judice, Ninke
	DIFFUSE_STUCKI,		// Stucki
	DIFFUSE_BURKES,		// Burkes
	DIFFUSE_2,			// 2 pixels (right, down)
	DIFFUSE_3,			// 3 pixels (right, down, rightdown)
	DIFFUSE_RGB,		// RGB color sepalated
} ReductorDiffuse;

// 色モードは下位8ビットが enum。
// GRAY では bit15-8 の 8ビットに「階調-1」(=1-255) を格納する。
// ADAPTIVE も同様に「色数-1」(=7-255) を格納する。
typedef enum {
	COLOR_MODE_NONE,
	COLOR_MODE_GRAY,			// グレイスケール
	COLOR_MODE_8_RGB,			// RGB 8色
	COLOR_MODE_16_VGA,			// ANSI 16色 (VGA)
	COLOR_MODE_256_RGB332,		// 固定 256 色 (MSX SCREEN8 互換)
	COLOR_MODE_256_XTERM,		// 固定 256 色 (xterm 互換)
	COLOR_MODE_ADAPTIVE,		// 適応パレット (8-256 色)

	// 下位8ビットが色モード
	COLOR_MODE_MASK = 0xff,
} ColorMode;

#define MAKE_COLOR_MODE_GRAY(n)	\
	(COLOR_MODE_GRAY | (((uint)(n) - 1) << 8))
#define MAKE_COLOR_MODE_ADAPTIVE(n)	\
	(COLOR_MODE_ADAPTIVE | (((uint)(n) - 1) << 8))

#define GET_COLOR_MODE(fmt)		((fmt) & COLOR_MODE_MASK)
#define GET_COLOR_COUNT(fmt)	((((uint)(fmt)) >> 8) + 1)

#if BYTE_ORDER == LITTLE_ENDIAN
#define RGBToU32(r, g, b)	((uint32)(((r) <<  0) | ((g) << 8) | ((b) << 16)))
#else
#define RGBToU32(r, g, b)	((uint32)(((r) << 24) | ((g) << 16) | ((b) << 8)))
#endif

// 画像の形式
enum {
	IMAGE_FMT_ARGB16,	// 内部形式。ARGB 1:5:5:5
	IMAGE_FMT_AIDX16,	// 内部形式。bit15 が A、下位8ビットがインデックス
	IMAGE_FMT_RGB24,	// RGB  (メモリ上 R, G, B の順)
	IMAGE_FMT_ARGB32,	// RGBA (メモリ上 R, G, B, A の順)
};

typedef union ColorRGB_ {
	uint32 u32;
	struct {
		uint8 r;
		uint8 g;
		uint8 b;
		uint8 a;
	};
} ColorRGB;

// 読み込みをサポートしている画像形式。
enum {
	IMAGE_LOADER_BLURHASH = 0,
	IMAGE_LOADER_BMP,
	IMAGE_LOADER_GIF,
	IMAGE_LOADER_JPEG,
	IMAGE_LOADER_PNG,
	IMAGE_LOADER_PNM,
	IMAGE_LOADER_TIFF,
	IMAGE_LOADER_WEBP,
	IMAGE_LOADER_MAX,
};
// IMAGE_LOADER_* に対応する名前。
#define IMAGE_LOADER_NAMES \
	"blurhash",	\
	"bmp",	\
	"gif",	\
	"jpeg",	\
	"png",	\
	"pnm",	\
	"tiff",	\
	"webp"

struct image
{
	// buf はラスターパディングなし。
	// ストライドを足しやすいように uint8 のポインタにしておく。
	uint8 *buf;

	uint width;		// ピクセル幅
	uint height;	// ピクセル高さ
	uint format;	// 形式 (IMAGE_FMT_*)

	// この画像が透過ピクセルを持つ場合 true。
	// アルファチャンネルを持つ画像形式かではなく透過ピクセルを持つ画像か。
	// ただし実際に全数調査するか、入力形式から推定するだけかはある。
	bool has_alpha;

	// インデックスカラーの場合に使用したパレット。
	// インデックスカラーでない場合は参照しないこと。
	const ColorRGB *palette;

	// パレット数。インデックスカラーでない場合は参照しないこと。
	// ただし、適応パレットに変換する際の入力画像ならデバッグ表示用に
	// ここに色数を入れる。
	uint palette_count;

	// 動的に作成したパレットはここで所有する。
	ColorRGB *palette_buf;
};

// image_read() に対するヒント。
typedef struct image_read_hint_ {
	ResizeAxis axis;
	uint width;
	uint height;

	// 複数枚ある場合のページ(フレーム)番号。0 から始まる。
	uint page;
} image_read_hint;

struct image_opt {
	// 減色
	ReductorMethod method;
	ReductorDiffuse diffuse;
	ColorMode color;

	// 誤差の減衰率(?)。0 .. 256 で指定する。
	// 256以上は 256 と同じ効果となる。
	// 0 は機能オフ。
	uint cdm;

	// 出力ゲイン。0 .. 512 で指定する。256 が 1.0倍。
	// 負数なら適用しない (1.0 倍と同じ)。
	int gain;

	// SIXEL 出力
	bool output_ormode;
	bool output_transbg;
	bool suppress_palette;
};

// image.c
extern void image_opt_init(struct image_opt *);
extern ColorMode image_parse_color(const char *);
extern int  image_match(struct pstream *, const struct diag *);
extern struct image *image_read(struct pstream *, int,
	const image_read_hint *, const struct diag *);
extern void image_free(struct image *);
extern uint image_get_bytepp(const struct image *);
extern uint image_get_stride(const struct image *);
extern void image_get_preferred_size(uint, uint, ResizeAxis,
	uint, uint, uint *, uint *);
extern char **image_get_loaderinfo(void);
extern void image_convert_to16(struct image *);
extern struct image *image_reduct(struct image *, uint, uint,
	const struct image_opt *, const struct diag *);

extern const char *resizeaxis_tostr(ResizeAxis);
extern const char *reductordiffuse_tostr(ReductorDiffuse);
extern const char *colormode_tostr(ColorMode);

// image_blurhash.c
extern struct image *image_blurhash_read(FILE *, int, int, const struct diag *);

// image_sixel.c
extern void image_sixel_abort(FILE *);
extern bool image_sixel_write(FILE *, const struct image *,
	const struct image_opt *, const struct diag *);

#endif // !sayaka_image_h
