/* vi:set ts=4: */
/*
 * Copyright (C) 2021-2026 Tetsuya Isaki
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

#include "common.h"
#include "image_priv.h"
#include <string.h>

//#define IMAGE_PROFILE

#if defined(IMAGE_PROFILE)
#include <time.h>
#define PROF(x)	clock_gettime(CLOCK_MONOTONIC, &x);
#define PROF_RESULT(msg, x)	do {	\
	uint32 x##_us = timespec_to_usec(&x##_end) - timespec_to_usec(&x##_start);\
	printf("%-12s %u.%06u sec\n", msg, x##_us / 1000000, x##_us % 1000000);	\
} while (0)
#else
#define PROF(x)	/**/
#define PROF_RESULT(msg, x)	/**/
#endif

// r, g, b を NTSC 輝度に変換する。高速化のため雑に近似。
// 元の式は (0.299 * R) + (0.587 * G) + (0.114 * B) で、
// これを整数演算にした (77 * R + 150 * G + 29 * B) / 256 とかが定番だが、
// gcc(m68k) だと乗算命令使わず筆算式に展開するっぽくそれなら立ってるビットが
// 少ないほうがいいので 4 ビットで済ませる。
// 元々この係数の厳密性はそんなにないので、この程度で十分。
// 普通に乗算命令を吐くアーキテクチャ/コンパイラなら、どっちでも一緒。
#define RGBToY(r, g, b) ((5 * (r) + 9 * (g) + 2 * (b)) / 16)

// 誤差分散用。
#define ERRBUF_LINES	(3)
#define ERRBUF_LEFT		(2)
#define ERRBUF_RIGHT	(2)

struct image_reductor_handle_;
typedef uint (*finder_t)(struct image_reductor_handle_ *, ColorRGB);

typedef struct {
	int16 r;
	int16 g;
	int16 b;
} ColorRGBint16;

typedef struct {
	int32 r;
	int32 g;
	int32 b;
} ColorRGBint32;

typedef struct image_reductor_handle_
{
	// 画像 (所有はしていない)
	struct image *dstimg;
	struct image *srcimg;

	bool is_gray;

	// オプション (所有はしていない)
	const struct image_opt *opt;

	// 誤差分散用バッファ。
	ColorRGBint16 *errbuf[ERRBUF_LINES];
	ColorRGBint16 *errbuf_mem;
	uint errbuf_stride;

	// 減衰。
	uint cdm;
	ColorRGBint32 prevcol;

	// 色からパレット番号を検索する関数。
	finder_t finder;

	// RGB555 から適応パレットのカラーコードを引くハッシュ。
	uint16 *colorhash;

	// 適応パレット時に使う検索範囲の上下限。
	int y_lo[8];
	int y_hi[8];
#if defined(IMAGE_PROFILE)
	uint y_count[8];	// ここで見つかったピクセル数
#endif

	const struct diag *diag;
} image_reductor_handle;

static uint finder_gray(image_reductor_handle *, ColorRGB);
static uint finder_fixed8(image_reductor_handle *, ColorRGB);
static uint finder_vga16(image_reductor_handle *, ColorRGB);
#if defined(SIXELV)
static uint finder_fixed256(image_reductor_handle *, ColorRGB);
static uint finder_xterm256(image_reductor_handle *, ColorRGB);
static inline uint8 finder_xterm256_channel(uint8);
#endif
static uint finder_adaptive(image_reductor_handle *, ColorRGB);
static void colorcvt_gray(ColorRGBint32 *);
static ColorRGB *image_alloc_gray_palette(uint);
#if defined(SIXELV)
static ColorRGB *image_alloc_fixed256_palette(void);
static ColorRGB *image_alloc_xterm256_palette(void);
#endif
static bool image_calc_adaptive_palette(image_reductor_handle *,
	struct image *, int);

#if defined(SIXELV)
static bool image_reduct_simple(image_reductor_handle *);
#endif
static bool image_reduct_highquality_fixed(image_reductor_handle *);
static bool image_reduct_highquality_adaptive(image_reductor_handle *);
static bool errbuf_init(image_reductor_handle *);
static inline __always_inline void errbuf_rotate(image_reductor_handle *);
static void errbuf_free(image_reductor_handle *);
static inline __always_inline ColorRGB pixel_mean(image_reductor_handle *,
	uint, uint, uint, uint);
static inline __always_inline uint16 pixel_filter_hq(image_reductor_handle *,
	ColorRGB, int);
#if defined(SIXELV)
static void set_err(ColorRGBint16 *, int, const ColorRGBint32 *, int);
#endif
static inline void set_err_asr(ColorRGBint16 *, int, const ColorRGBint32 *,
	int);
static inline uint8 saturate_uint8(int);
static inline int16 saturate_adderr(int16, int);

static const ColorRGB palette_fixed8[];
static const ColorRGB palette_vga16[];

// opt を初期化する。
void
image_opt_init(struct image_opt *opt)
{
	opt->method  = REDUCT_HIGH_QUALITY;
	opt->diffuse = DIFFUSE_SFL;
	opt->color   = MAKE_COLOR_MODE_ADAPTIVE(256);
	opt->cdm     = 0;
	opt->gain    = -1;
	opt->output_ormode = false;
	opt->output_transbg = false;
	opt->suppress_palette = false;
}

// (引数の) 文字列から ColorMode を返す。
// エラーなら COLOR_MODE_NONE を返す。
ColorMode
image_parse_color(const char *arg)
{
	ColorMode color = COLOR_MODE_NONE;
	int num = -1;

	// 数字とその前とに分ける。正規表現でいうと /(\w*)(\d*)/ みたいな感じ。
	size_t dp = strcspn(arg, "0123456789");
	char str[dp + 1];
	strncpy(str, arg, dp);
	str[dp] = '\0';
	if (arg[dp] != '\0') {
		char *end;
		num = stou32def(&arg[dp], -1, &end);
		if (*end != '\0') {
			goto abort;
		}
	}

	if (str[0] == '\0') {
		// 数字だけの場合。
		switch (num) {
		 case 2:	color = MAKE_COLOR_MODE_GRAY(2);		break;
		 case 8:	color = COLOR_MODE_8_RGB;				break;
		 case 16:	color = COLOR_MODE_16_VGA;				break;
		 case 256:	color = MAKE_COLOR_MODE_ADAPTIVE(256);	break;
		 default:
			break;
		}
	} else {
		// 文字から始まっている場合。
#if defined(SIXELV)
		if (strcmp(arg, "fixed256") == 0) {
			color = COLOR_MODE_256_RGB332;
		} else if (strcmp(arg, "xterm256") == 0) {
			color = COLOR_MODE_256_XTERM;
		} else if (strcmp(str, "adaptive") == 0) {
			if (num < 0) {
				color = MAKE_COLOR_MODE_ADAPTIVE(256);
			} else if (8 <= num && num <= 256) {
				color = MAKE_COLOR_MODE_ADAPTIVE(num);
			}
		} else
#endif
		if (strcmp(str, "gray") == 0 || strcmp(str, "grey") == 0) {
			if (num < 0) {
				color = MAKE_COLOR_MODE_GRAY(256);
			} else if (2 <= num && num <= 256) {
				color = MAKE_COLOR_MODE_GRAY(num);
			}
		}
	}

 abort:
	return color;
}

// width_ x height_ で形式が format_ の image を作成する。
// (バッファは未初期化)
struct image *
image_create(uint width_, uint height_, uint format_)
{
	struct image *img = calloc(1, sizeof(*img));
	if (img == NULL) {
		return NULL;
	}

	img->width = width_;
	img->height = height_;
	img->format = format_;
	img->buf = malloc(image_get_stride(img) * img->height);
	if (img->buf == NULL) {
		free(img);
		return NULL;
	}

	return img;
}

// image を解放する。NULL なら何もしない。
void
image_free(struct image *img)
{
	if (img != NULL) {
		free(img->buf);
		free(img->palette_buf);
		free(img);
	}
}

// 1ピクセルあたりのバイト数を返す。
uint
image_get_bytepp(const struct image *img)
{
	switch (img->format) {
	 case IMAGE_FMT_ARGB16:	return 2;
	 case IMAGE_FMT_AIDX16:	return 2;
	 case IMAGE_FMT_RGB24:	return 3;
	 case IMAGE_FMT_ARGB32:	return 4;
	 default:
		assert(0);
	}
}

// ストライドを返す。
uint
image_get_stride(const struct image *img)
{
	return img->width * image_get_bytepp(img);
}

// いい感じにリサイズした時の幅と高さを求める。
void
image_get_preferred_size(
	uint current_width,			// 現在の幅
	uint current_height,		// 現在の高さ
	ResizeAxis axis,			// リサイズの基準
	uint request_width,			// 要求するリサイズ後の幅 (optional)
	uint request_height,		// 要求するリサイズ後の高さ (optional)
	uint *preferred_width,		// 求めた幅を格納する先
	uint *preferred_height)		// 求めた高さを格納する先
{
#if !defined(SIXELV)
	assert(axis == RESIZE_AXIS_SCALEDOWN_LONG);
#endif

	// 条件を丸めていく
	switch (axis) {
	 default:
	 case RESIZE_AXIS_LONG:
	 case RESIZE_AXIS_SCALEDOWN_LONG:
		if (current_width >= current_height) {
			axis = RESIZE_AXIS_WIDTH;
		} else {
			axis = RESIZE_AXIS_HEIGHT;
		}
		break;

#if defined(SIXELV)
	 case RESIZE_AXIS_BOTH:
	 case RESIZE_AXIS_SCALEDOWN_BOTH:
		if (request_width == 0) {
			axis = RESIZE_AXIS_HEIGHT;
		} else if (request_height == 0) {
			axis = RESIZE_AXIS_WIDTH;
		} else {
			axis = RESIZE_AXIS_BOTH;
		}
		break;

	 case RESIZE_AXIS_SHORT:
	 case RESIZE_AXIS_SCALEDOWN_SHORT:
		if (current_width <= current_height) {
			axis = RESIZE_AXIS_WIDTH;
		} else {
			axis = RESIZE_AXIS_HEIGHT;
		}
		break;

	 case RESIZE_AXIS_SCALEDOWN_WIDTH:
		axis = RESIZE_AXIS_WIDTH;
		break;

	 case RESIZE_AXIS_SCALEDOWN_HEIGHT:
		axis = RESIZE_AXIS_HEIGHT;
		break;
#endif
	}

	if (request_width < 1) {
		request_width = current_width;
	}
	if (request_height < 1) {
		request_height = current_height;
	}

	// 縮小のみ指示。
#if defined(SIXELV)
	if ((axis & RESIZE_AXIS_SCALEDOWN_BIT))
#endif
	{
		if (request_width > current_width) {
			request_width = current_width;
		}
		if (request_height > current_height) {
			request_height = current_height;
		}
	}

	// 確定したので計算。
	uint width;
	uint height;
	switch (axis) {
	 case RESIZE_AXIS_WIDTH:
		width  = request_width;
		height = current_height * width / current_width;
		break;

	 case RESIZE_AXIS_HEIGHT:
		height = request_height;
		width  = current_width * height / current_height;
		break;

#if defined(SIXELV)
	 case RESIZE_AXIS_BOTH:
		width  = request_width;
		height = request_height;
		break;
#endif

	 default:
		__unreachable();
	}

	// 代入。
	if (preferred_width) {
		*preferred_width  = width;
	}
	if (preferred_height) {
		*preferred_height = height;
	}
}

// ローダごとにサポートしているファイル形式をビットマップフラグにしたもの。
#define LOADERMAP_bmp	(1U << IMAGE_LOADER_BMP)
#define LOADERMAP_gif	(1U << IMAGE_LOADER_GIF)
#define LOADERMAP_ico	(1U << IMAGE_LOADER_ICO)
#define LOADERMAP_jpeg	(1U << IMAGE_LOADER_JPEG)
#define LOADERMAP_jxl	(1U << IMAGE_LOADER_JXL)
#define LOADERMAP_mag	(1U << IMAGE_LOADER_MAG)
#define LOADERMAP_png	(1U << IMAGE_LOADER_PNG)
#define LOADERMAP_pnm1	(1U << IMAGE_LOADER_PNM1)
#define LOADERMAP_pnm2	(1U << IMAGE_LOADER_PNM2)
#define LOADERMAP_pnm3	(1U << IMAGE_LOADER_PNM3)
#define LOADERMAP_pnm4	(1U << IMAGE_LOADER_PNM4)
#define LOADERMAP_pnm5	(1U << IMAGE_LOADER_PNM5)
#define LOADERMAP_pnm6	(1U << IMAGE_LOADER_PNM6)
#define LOADERMAP_tiff	(1U << IMAGE_LOADER_TIFF)
#define LOADERMAP_webp	(1U << IMAGE_LOADER_WEBP)
#define LOADERMAP_ypic	(1U << IMAGE_LOADER_YPIC)
#define LOADERMAP_stb	((1U << IMAGE_LOADER_BMP)	| \
						 (1U << IMAGE_LOADER_GIF)	| \
						 (1U << IMAGE_LOADER_JPEG)	| \
						 (1U << IMAGE_LOADER_PNG)	| \
						 (1U << IMAGE_LOADER_PNM5)	| \
						 (1U << IMAGE_LOADER_PNM6))

// サポートしているローダ。処理順に並べること。
static const struct {
	image_match_t match;
	image_read_t  read;
	const char *libname;	// image_get_loaderinfo() で表示する名前
	const char *name;		// マクロ展開用とデバッグログとかで使う短縮名
	uint32 supported;		// このローダがサポートしている画像形式
} loader[] = {
#define ENTRY(name, libname, map)	\
	{ image_##name##_match, image_##name##_read, \
	  #libname, #name, map }
#if defined(USE_LIBWEBP)
	ENTRY(webp, libwebp, LOADERMAP_webp),
#endif
#if defined(USE_LIBJPEG)
	ENTRY(jpeg, libjpeg, LOADERMAP_jpeg),
#endif
#if defined(USE_LIBJXL)
	ENTRY(jxl, libjxl, LOADERMAP_jxl),
#endif
#if defined(USE_LIBPNG)
	ENTRY(png, libpng, LOADERMAP_png),
#endif
#if defined(USE_GIFLIB)
	ENTRY(gif, giflib, LOADERMAP_gif),
#endif
#if defined(USE_BUILTIN_BMP) && defined(SIXELV)
	ENTRY(bmp, builtin, LOADERMAP_bmp),
#endif
#if defined(USE_LIBTIFF) && defined(SIXELV)
	ENTRY(tiff, libtiff, LOADERMAP_tiff),
#endif
#if defined(USE_BUILTIN_PNM) && defined(SIXELV)
	ENTRY(pnm5, builtin, LOADERMAP_pnm5),
	ENTRY(pnm6, builtin, LOADERMAP_pnm6),
	// ASCII と P4 は実用上の価値はないので優先度後ろでいい。
	ENTRY(pnm1, builtin, LOADERMAP_pnm1),
	ENTRY(pnm2, builtin, LOADERMAP_pnm2),
	ENTRY(pnm3, builtin, LOADERMAP_pnm3),
	ENTRY(pnm4, builtin, LOADERMAP_pnm4),
#endif
#if defined(USE_BUILTIN_ICO) && defined(SIXELV)
	ENTRY(ico,  builtin, LOADERMAP_ico),
#endif
#if defined(USE_BUILTIN_MAG) && defined(SIXELV)
	ENTRY(mag,  builtin, LOADERMAP_mag),
#endif
#if defined(USE_BUILTIN_YPIC) && defined(SIXELV)
	ENTRY(ypic, builtin, LOADERMAP_ypic),
#endif
#if defined(USE_STB_IMAGE)
	ENTRY(stb, stb_image, LOADERMAP_stb),
#endif
#undef ENTRY
};

// サポートしているローダの一覧を返す。
// 戻り値は char * の配列で、{ name1, loader1, name2, loader2, ... }
// のように IMAGE_LOADER_* の画像形式とそのローダ名がペアで並んでおり、
// NULL で終端。
// 戻り値は使い終わったら free() すること。
char **
image_get_loaderinfo(void)
{
	static const char *names[] = { IMAGE_LOADER_NAMES };
	char **dst;
	char **d;

	dst = calloc(IMAGE_LOADER_MAX * 2 + 1, sizeof(char *));
	if (dst == NULL) {
		return NULL;
	}
	d = dst;

	for (uint n = 0; n < IMAGE_LOADER_MAX; n++) {
		uint32 filetype = (1U << n);
		int i;
		for (i = 0; i < countof(loader); i++) {
			if ((loader[i].supported & filetype)) {
				break;
			}
		}
		const char *lib = NULL;
		if (i < countof(loader)) {
			lib = UNCONST(loader[i].libname);
		} else if (n == IMAGE_LOADER_BLURHASH) {
			// Blurhash は loader[] テーブルには出てこないので手動処理。
			lib = "builtin";
		} else {
			continue;
		}
		*d++ = UNCONST(names[n]);
		*d++ = UNCONST(lib);
	}

	// 終端は1つ。
	*d = NULL;

	return dst;
}

// pstream の画像形式を判定する。
// 判定出来れば非負のローダ種別を返す。これは image_read() に渡すのに使う。
// 判定出来なければ -1 を返す。
// ここでは Blurhash は扱わない。
int
image_match(struct pstream *ps, const struct diag *diag)
{
	FILE *fp;
	int type = -1;

	fp = pstream_open_for_peek(ps);
	if (fp == NULL) {
		Debug(diag, "pstream_open_for_peek() failed");
		return -1;
	}

	if (countof(loader) == 0) {
		Debug(diag, "%s: no decoders available", __func__);
		goto done;
	}

	for (uint i = 0; i < countof(loader); i++) {
		int ok = loader[i].match(fp, diag);
		Trace(diag, "Checking %-4s.. %s",
			loader[i].name, (ok ? "matched" : "no"));
		fseek(fp, 0, SEEK_SET);
		if (ok) {
			type = i;
			goto done;
		}
	}
	Trace(diag, "%s: unsupported image format", __func__);

 done:
	fclose(fp);
	return type;
}

// pstream から画像を読み込んで image を作成して返す。
// type は image_match() で返されたローダ種別。
// axis, width, height はリサイズ用のヒントで、これを使うかどうかは
// 画像ローダによる (今の所 jpeg のみ)。
// デコードに失敗すると NULL を返す。
struct image *
image_read(struct pstream *ps, int type, const image_read_hint *hint,
	const struct diag *diag)
{
	FILE *fp;

	fp = pstream_open_for_read(ps);
	if (fp == NULL) {
		Debug(diag, "%s: pstream_open_for_read() failed", __func__);
		return NULL;
	}
	struct image *img = loader[type].read(fp, hint, diag);
	fclose(fp);

	return img;
}

// 入力画像を 16bit 内部形式にインプレース変換する。
void
image_convert_to16(struct image *img)
{
	const uint8 *s8 = img->buf;
	uint16 *d16 = (uint16 *)img->buf;
	uint count = img->width * img->height;

	if (img->format == IMAGE_FMT_RGB24) {
		for (uint i = 0; i < count; i++) {
			uint8 r = *s8++;
			uint8 g = *s8++;
			uint8 b = *s8++;
			*d16++ = RGB888_to_ARGB16(r, g, b);
		}
	} else if (img->format == IMAGE_FMT_ARGB32) {
		for (uint i = 0; i < count; i++) {
			uint8 r = *s8++;
			uint8 g = *s8++;
			uint8 b = *s8++;
			uint8 a = *s8++;
			uint16 v = RGB888_to_ARGB16(r, g, b);
			// A(不透明度)が半分以下なら透明(0x8000)とする。
			if (a < 0x80) {
				v |= 0x8000;
			}
			*d16++ = v;
		}

		// 実際にあるかどうかは調べてない。
		img->has_alpha = true;
	} else if (img->format != IMAGE_FMT_ARGB16) {
		assert(img->format == IMAGE_FMT_ARGB16);
	}

	img->format = IMAGE_FMT_ARGB16;
}

// src 画像を (dst_width, dst_height) にリサイズしながら同時に
// colormode に減色した新しい image を作成して返す。
// 適応パレット(COLOR_MODE_ADAPTIVE) なら、デバッグ表示用に
// src->palette_count に色数を書き戻す。
struct image *
image_reduct(
	struct image *src,			// 元画像
	uint dst_width,				// リサイズ後の幅
	uint dst_height,			// リサイズ後の高さ
	const struct image_opt *opt,	// パラメータ
	const struct diag *diag)
{
	struct image *dst;
	image_reductor_handle irbuf, *ir;
	bool ok = false;

	ir = &irbuf;
	memset(ir, 0, sizeof(*ir));
	ir->opt = opt;
	ir->diag = diag;

	dst = image_create(dst_width, dst_height, IMAGE_FMT_AIDX16);
	if (dst == NULL) {
		return NULL;
	}
	dst->has_alpha = src->has_alpha;

	ir->dstimg = dst;
	ir->srcimg = src;

	// 減色モードからパレットオペレーションを用意。
	switch (GET_COLOR_MODE(opt->color)) {
	 case COLOR_MODE_GRAY:
	 {
		uint graycount = GET_COLOR_COUNT(opt->color);
		dst->palette_buf = image_alloc_gray_palette(graycount);
		if (dst->palette_buf == NULL) {
			goto abort;
		}
		dst->palette = dst->palette_buf;
		dst->palette_count = graycount;
		ir->finder = finder_gray;
		ir->is_gray = true;
		break;
	 }

	 case COLOR_MODE_8_RGB:
		ir->finder  = finder_fixed8;
		dst->palette = palette_fixed8;
		dst->palette_count = 8;
		break;

	 case COLOR_MODE_16_VGA:
		ir->finder  = finder_vga16;
		dst->palette = palette_vga16;
		dst->palette_count = 16;
		break;

	 case COLOR_MODE_ADAPTIVE:
	 {
		uint palcount = GET_COLOR_COUNT(opt->color);
		dst->palette_buf = calloc(palcount, sizeof(ColorRGB));
		if (dst->palette_buf == NULL) {
			goto abort;
		}
		dst->palette = dst->palette_buf;
		dst->palette_count = palcount;
		ir->finder = finder_adaptive;
		break;
	 }

#if defined(SIXELV)
	 case COLOR_MODE_256_RGB332:
		dst->palette_buf = image_alloc_fixed256_palette();
		if (dst->palette_buf == NULL) {
			goto abort;
		}
		dst->palette = dst->palette_buf;
		dst->palette_count = 256;
		ir->finder = finder_fixed256;
		break;

	 case COLOR_MODE_256_XTERM:
		dst->palette_buf = image_alloc_xterm256_palette();
		if (dst->palette_buf == NULL) {
			goto abort;
		}
		dst->palette = dst->palette_buf;
		dst->palette_count = 256;
		ir->finder = finder_xterm256;
		break;
#endif

	 default:
		Debug(diag, "%s: Unsupported color 0x%x", __func__, opt->color);
		goto abort;
	}

#if defined(SIXELV)
	if (opt->method == REDUCT_SIMPLE) {
		ok = image_reduct_simple(ir);

	} else if (opt->method != REDUCT_HIGH_QUALITY) {
		Debug(diag, "%s: Unknown method %u", __func__, opt->method);
		ok = false;

	} else
#endif
	{
		if (IS_COLOR_MODE_ADAPTIVE(opt->color)) {
			ok = image_reduct_highquality_adaptive(ir);
		} else {
			ok = image_reduct_highquality_fixed(ir);
		}
	}

	if (diag_get_level(diag) >= 2 && IS_COLOR_MODE_ADAPTIVE(opt->color)) {
		const char *title = "";
		char buf[16];
#if defined(IMAGE_PROFILE)
		title = "  found pixels";
#else
		buf[0] = '\0';
#endif
		diag_print(diag, "[lo, hi )%s", title);
		for (uint i = 0; i < 8; i++) {
#if defined(IMAGE_PROFILE)
			snprintf(buf, sizeof(buf), " = %u", ir->y_count[i]);
#endif
			diag_print(diag, "%3u, %3u%s", ir->y_lo[i], ir->y_hi[i], buf);
		}
	}

 abort:
#if defined(SIXELV)
	free(ir->colorhash);
#endif
	if (!ok) {
		image_free(dst);
		dst = NULL;
	}
	return dst;
}


//
// 分数計算機
//

typedef struct {
	int I;	// 整数項
	int N;	// 分子
	int D;	// 分母
} Rational;

static void
rational_init(Rational *sr, int i, int n, int d)
{
	sr->I = i;
	if (n < d) {
		sr->N = n;
	} else {
		sr->I += n / d;
		sr->N = n % d;
	}
	sr->D = d;
}

// sr += x
static void
rational_add(Rational *sr, const Rational *x)
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

// リサイズ用の初期化マクロ。
#define RESIZE_INIT(dstwidth_, dstheight_, srcimg_)	\
	Rational ry;	\
	Rational rx;	\
	Rational ystep;	\
	Rational xstep;	\
	rational_init(&ry,    0, 0, dstheight_);	\
	rational_init(&ystep, 0, (srcimg_)->height, dstheight_);	\
	rational_init(&rx,    0, 0, dstwidth_);	\
	rational_init(&xstep, 0, (srcimg_)->width, dstwidth_)

// リサイズの X, Y 各方向のループ冒頭の処理。
#define RESIZE_STEP(S0, S1, RR, STEP)	\
		uint S0 = (RR).I;	\
		rational_add(&(RR), &(STEP));	\
		uint S1 = (RR).I;	\
		if (S0 == S1) {	\
			S1 += 1;	\
		}

// X ループ冒頭のリセット。
#define RESIZE_RESET_X()	do {	\
	rx.I = 0;	\
	rx.N = 0;	\
} while (0)


//
// 減色 & リサイズ
//

#if defined(SIXELV)
// 単純間引き
static bool
image_reduct_simple(image_reductor_handle *ir)
{
	struct image *dstimg = ir->dstimg;
	struct image *srcimg = ir->srcimg;
	uint16 *d = (uint16 *)dstimg->buf;
	const uint16 *src = (const uint16 *)srcimg->buf;
	uint dstwidth  = dstimg->width;
	uint dstheight = dstimg->height;

	// 適応パレットならここでパレットを作成。
	if (IS_COLOR_MODE_ADAPTIVE(ir->opt->color)) {
		if (image_calc_adaptive_palette(ir, srcimg, ir->opt->gain) == false) {
			return false;
		}
	}

	// 水平、垂直方向ともスキップサンプリング。
	RESIZE_INIT(dstwidth, dstheight, srcimg);
	for (uint y = 0; y < dstheight; y++) {
		RESIZE_RESET_X();
		const uint16 *s0 = &src[ry.I * srcimg->width];
		for (uint x = 0; x < dstwidth; x++) {
			ColorRGBint32 col;
			int a;
			uint16 v = s0[rx.I];
			a     =  (v >> 15);
			col.r = ((v >> 10) & 0x1f) << 3;
			col.g = ((v >>  5) & 0x1f) << 3;
			col.b = ( v        & 0x1f) << 3;

			if (ir->opt->gain >= 0) {
				col.r = saturate_uint8((uint32)col.r * ir->opt->gain / 256);
				col.g = saturate_uint8((uint32)col.g * ir->opt->gain / 256);
				col.b = saturate_uint8((uint32)col.b * ir->opt->gain / 256);
			}
			if (ir->is_gray) {
				colorcvt_gray(&col);
			}
			ColorRGB c8;
			c8.r = col.r;
			c8.g = col.g;
			c8.b = col.b;
			uint colorcode = ir->finder(ir, c8);
			if (a) {
				colorcode |= 0x8000;
			}
			*d++ = colorcode;

			rational_add(&rx, &xstep);
		}
		rational_add(&ry, &ystep);
	}

	return true;
}
#endif

// 誤差分散用のバッファを初期化する。
static bool
errbuf_init(image_reductor_handle *ir)
{
	int errbuf_width = ir->dstimg->width + ERRBUF_LEFT + ERRBUF_RIGHT;

	ir->errbuf_stride = errbuf_width * sizeof(ir->errbuf_mem[0]);
	ir->errbuf_mem = calloc(ERRBUF_LINES, ir->errbuf_stride);
	if (ir->errbuf_mem == NULL) {
		return false;
	}
	for (uint i = 0; i < ERRBUF_LINES; i++) {
		ir->errbuf[i] = ir->errbuf_mem + (errbuf_width * i) + ERRBUF_LEFT;
	}

	// ついでに減衰用のパラメータもここで初期化。
	ir->cdm = 256;
	memset(&ir->prevcol, 0, sizeof(ir->prevcol));

	return true;
}

// 誤差分散バッファをローテートする。
static inline __always_inline void
errbuf_rotate(image_reductor_handle *ir)
{
	ColorRGBint16 *tmp = ir->errbuf[0];
	int i;

	for (i = 0; i < ERRBUF_LINES - 1; i++) {
		ir->errbuf[i] = ir->errbuf[i + 1];
	}
	ir->errbuf[i] = tmp;
	// errbuf[y] には左マージンがあるのを考慮する。
	memset(ir->errbuf[i] - ERRBUF_LEFT, 0, ir->errbuf_stride);
}

// ir のうち誤差分散用に確保したリソースを解放する。
static void
errbuf_free(image_reductor_handle *ir)
{
	if (ir->errbuf_mem) {
		free(ir->errbuf_mem);
		ir->errbuf_mem = NULL;
	}
}

// 二次元誤差分散法を使用して、出来る限り高品質に変換する。固定パレットの場合。
static bool
image_reduct_highquality_fixed(image_reductor_handle *ir)
{
	const struct image *srcimg = ir->srcimg;
	struct image *dstimg = ir->dstimg;
	uint dstwidth  = dstimg->width;
	uint dstheight = dstimg->height;

#if !defined(SIXELV)
	// sayaka では選択出来ないようにしてある。
	assert(ir->opt->diffuse == DIFFUSE_SFL);
#endif

	if (errbuf_init(ir) == false) {
		return false;
	}
	RESIZE_INIT(dstwidth, dstheight, srcimg);
	uint16 *d = (uint16 *)dstimg->buf;
	for (uint y = 0; y < dstheight; y++) {
		RESIZE_STEP(sy0, sy1, ry, ystep);
		RESIZE_RESET_X();
		for (uint x = 0; x < dstwidth; x++) {
			RESIZE_STEP(sx0, sx1, rx, xstep);

			ColorRGB c8 = pixel_mean(ir, sy0, sy1, sx0, sx1);
			uint16 v = pixel_filter_hq(ir, c8, x);
			if (__predict_false(c8.a)) {
				v |= 0x8000;
			}
			*d++ = v;
		}

		// 誤差バッファをローテート。
		errbuf_rotate(ir);
	}

	errbuf_free(ir);
	return true;
}

// 二次元誤差分散法を使用して、出来る限り高品質に変換する。適応パレットの場合。
static bool
image_reduct_highquality_adaptive(image_reductor_handle *ir)
{
	struct image *srcimg = ir->srcimg;
	struct image *dstimg = ir->dstimg;
	uint dstwidth  = dstimg->width;
	uint dstheight = dstimg->height;
	bool rv = false;

#if !defined(SIXELV)
	// sayaka では選択出来ないようにしてある。
	assert(ir->opt->diffuse == DIFFUSE_SFL);
#endif

	// 水平、垂直ともピクセルを平均。
	// 真に高品質にするには補間法を適用するべきだがそこまではしない。

	// 中間画像(リサイズ後画像)。
	struct image *tmpimg = image_create(dstwidth, dstheight, IMAGE_FMT_ARGB16);
	if (tmpimg == NULL) {
		return false;
	}

	// 1パス目。リサイズしながら減色。
	RESIZE_INIT(dstwidth, dstheight, srcimg);
	uint16 *t = (uint16 *)tmpimg->buf;
	for (uint y = 0; y < dstheight; y++) {
		RESIZE_STEP(sy0, sy1, ry, ystep);
		RESIZE_RESET_X();
		for (uint x = 0; x < dstwidth; x++) {
			RESIZE_STEP(sx0, sx1, rx, xstep);

			ColorRGB c8 = pixel_mean(ir, sy0, sy1, sx0, sx1);
			uint16 v = RGB888_to_ARGB16(c8.r, c8.g, c8.b);
			if (__predict_false(c8.a)) {
				v |= 0x8000;
			}
			*t++ = v;
		}
	}

	// tmpimg の色集合に対して適応パレットを用意。
	if (image_calc_adaptive_palette(ir, tmpimg, -1) == false) {
		goto abort;
	}
	// 入力画像 (リサイズ前) が何色で構成されていたかは動作に必要ないので
	// 調べておらず、デバッグ表示出来るのはリサイズ後の色数になってしまうが
	// 仕方ない。
	srcimg->palette_count = tmpimg->palette_count;

	// 2パス目。
	// 出力画像サイズと同じサイズの tmpimg にフィルタを適用するだけ。
	const uint16 *s = (const uint16 *)tmpimg->buf;
	uint16 *d = (uint16 *)dstimg->buf;
	if (errbuf_init(ir) == false) {
		goto abort;
	}
	for (uint y = 0; y < dstheight; y++) {
		for (uint x = 0; x < dstwidth; x++) {
			uint cc = *s++;

			ColorRGB c8;
			c8.r = ((cc >> 10) & 0x1f) << 3;
			c8.g = ((cc >>  5) & 0x1f) << 3;
			c8.b = ( cc        & 0x1f) << 3;
			uint16 v = pixel_filter_hq(ir, c8, x);
			if (__predict_false((cc & 0x8000))) {
				v |= 0x8000;
			}
			*d++ = v;
		}

		// 誤差バッファをローテート。
		errbuf_rotate(ir);
	}

	rv = true;
	errbuf_free(ir);
 abort:
	image_free(tmpimg);
	return rv;
}

// src 画像の X = [sx0, sx1)、Y = [sy0, sy1) の画素の平均を求める。
// 真に高品質にするには補間法を適用するべきだがそこまではしない。
// ついでにここでゲインも適用して返す。
static inline __always_inline ColorRGB
pixel_mean(image_reductor_handle *ir, uint sy0, uint sy1, uint sx0, uint sx1)
{
	const uint16 *src = (const uint16 *)(ir->srcimg->buf);
	const uint srcwidth = ir->srcimg->width;
	ColorRGBint32 col;
	uint a;

	memset(&col, 0, sizeof(col));
	a = 0;
	for (uint sy = sy0; sy < sy1; sy++) {
		const uint16 *s = &src[sy * srcwidth + sx0];
		for (uint sx = sx0; sx < sx1; sx++) {
			uint16 v = *s++;
			a     +=  (v >> 15);
			// 5bitのまま足してループを出たところで8bitにする。
			col.r += ((v >> 10) & 0x1f);
			col.g += ((v >>  5) & 0x1f);
			col.b += ( v        & 0x1f);
		}
	}
	uint area = (sy1 - sy0) * (sx1 - sx0);
	col.r = (col.r << 3) / area;
	col.g = (col.g << 3) / area;
	col.b = (col.b << 3) / area;

	if (ir->opt->gain >= 0) {
		col.r = col.r * ir->opt->gain / 256;
		col.g = col.g * ir->opt->gain / 256;
		col.b = col.b * ir->opt->gain / 256;
	}

	ColorRGB c8;
	c8.r = col.r;
	c8.g = col.g;
	c8.b = col.b;
	// 過半数が透明なら透明ということにする。
	c8.a = (a > area / 2) ? 1 : 0;

	return c8;
}

// いろいろフィルタを適用した結果のカラーコードを返す。
// c は現在位置の色、x が X 座標(誤差分散で使う)。
// それ以外のパラメータは ir で維持されている。
// ここでは透明度 (c.a) は扱わない。
static inline __always_inline uint16
pixel_filter_hq(image_reductor_handle *ir, ColorRGB c, int x)
{
	ColorRGBint16 **errbuf = ir->errbuf;
	ColorRGBint32 col;
	uint cdm;

	col.r = c.r;
	col.g = c.g;
	col.b = c.b;

	cdm = ir->cdm;
	if (ir->opt->cdm != 0) {
		cdm /= 2;
		cdm = MAX(cdm, abs(col.r - ir->prevcol.r));
		cdm = MAX(cdm, abs(col.g - ir->prevcol.g));
		cdm = MAX(cdm, abs(col.b - ir->prevcol.b));
		cdm += ir->opt->cdm;
		if (cdm > 256) {
			cdm = 256;
		}
		ir->cdm = cdm;
		ir->prevcol = col;
	}

	col.r += errbuf[0][x].r;
	col.g += errbuf[0][x].g;
	col.b += errbuf[0][x].b;

	if (ir->is_gray) {
		colorcvt_gray(&col);
	}

	ColorRGB c8;
	c8.r = saturate_uint8(col.r);
	c8.g = saturate_uint8(col.g);
	c8.b = saturate_uint8(col.b);
	uint colorcode = ir->finder(ir, c8);

	ColorRGBint32 dif;
	dif.r = col.r - ir->dstimg->palette[colorcode].r;
	dif.g = col.g - ir->dstimg->palette[colorcode].g;
	dif.b = col.b - ir->dstimg->palette[colorcode].b;

	if (cdm != 256) {
		dif.r = dif.r * cdm / 256;
		dif.g = dif.g * cdm / 256;
		dif.b = dif.b * cdm / 256;
	}

	switch (ir->opt->diffuse) {
	 case DIFFUSE_SFL:
	 default:
		// Sierra Filter Lite
		set_err_asr(errbuf[0], x + 1, &dif, 1);
		set_err_asr(errbuf[1], x - 1, &dif, 2);
		set_err_asr(errbuf[1], x    , &dif, 2);
		break;

#if defined(SIXELV)
	 case DIFFUSE_NONE:
		// 比較用の何もしないモード
		break;
	 case DIFFUSE_FS:
		// Floyd Steinberg Method
		set_err(errbuf[0], x + 1, &dif, 112);
		set_err(errbuf[1], x - 1, &dif, 48);
		set_err(errbuf[1], x    , &dif, 80);
		set_err(errbuf[1], x + 1, &dif, 16);
		break;
	 case DIFFUSE_ATKINSON:
		// Atkinson
		set_err_asr(errbuf[0], x + 1, &dif, 3);	// 32
		set_err_asr(errbuf[0], x + 2, &dif, 3);	// 32
		set_err_asr(errbuf[1], x - 1, &dif, 3);	// 32
		set_err_asr(errbuf[1], x,     &dif, 3);	// 32
		set_err_asr(errbuf[1], x + 1, &dif, 3);	// 32
		set_err_asr(errbuf[2], x,     &dif, 3);	// 32
		break;
	 case DIFFUSE_JAJUNI:
		// Jarvis, Judice, Ninke
		set_err(errbuf[0], x + 1, &dif, 37);
		set_err(errbuf[0], x + 2, &dif, 27);
		set_err(errbuf[1], x - 2, &dif, 16);
		set_err(errbuf[1], x - 1, &dif, 27);
		set_err(errbuf[1], x,     &dif, 37);
		set_err(errbuf[1], x + 1, &dif, 27);
		set_err(errbuf[1], x + 2, &dif, 16);
		set_err(errbuf[2], x - 2, &dif,  5);
		set_err(errbuf[2], x - 1, &dif, 16);
		set_err(errbuf[2], x,     &dif, 27);
		set_err(errbuf[2], x + 1, &dif, 16);
		set_err(errbuf[2], x + 2, &dif,  5);
		break;
	 case DIFFUSE_STUCKI:
		// Stucki
		set_err(errbuf[0], x + 1, &dif, 43);
		set_err(errbuf[0], x + 2, &dif, 21);
		set_err(errbuf[1], x - 2, &dif, 11);
		set_err(errbuf[1], x - 1, &dif, 21);
		set_err(errbuf[1], x,     &dif, 43);
		set_err(errbuf[1], x + 1, &dif, 21);
		set_err(errbuf[1], x + 2, &dif, 11);
		set_err(errbuf[2], x - 2, &dif,  5);
		set_err(errbuf[2], x - 1, &dif, 11);
		set_err(errbuf[2], x,     &dif, 21);
		set_err(errbuf[2], x + 1, &dif, 11);
		set_err(errbuf[2], x + 2, &dif,  5);
		break;
	 case DIFFUSE_BURKES:
		// Burkes
		set_err_asr(errbuf[0], x + 1, &dif, 2);	// 64
		set_err_asr(errbuf[0], x + 2, &dif, 3);	// 32
		set_err_asr(errbuf[1], x - 2, &dif, 4);	// 16
		set_err_asr(errbuf[1], x - 1, &dif, 3);	// 32
		set_err_asr(errbuf[1], x,     &dif, 2);	// 64
		set_err_asr(errbuf[1], x + 1, &dif, 3);	// 32
		set_err_asr(errbuf[1], x + 2, &dif, 4);	// 16
		break;
	 case DIFFUSE_2:
		// (x+1,y), (x,y+1)
		set_err(errbuf[0], x + 1, &dif, 128);
		set_err(errbuf[1], x,     &dif, 128);
		break;
	 case DIFFUSE_3:
		// (x+1,y), (x,y+1), (x+1,y+1)
		set_err(errbuf[0], x + 1, &dif, 102);
		set_err(errbuf[1], x,     &dif, 102);
		set_err(errbuf[1], x + 1, &dif,  51);
		break;
	 case DIFFUSE_RGB:
		errbuf[0][x].r   = saturate_adderr(errbuf[0][x].r,   dif.r);
		errbuf[1][x].b   = saturate_adderr(errbuf[1][x].b,   dif.b);
		errbuf[1][x+1].g = saturate_adderr(errbuf[1][x+1].g, dif.g);
		break;
#endif
	}

	return colorcode;
}

#if defined(SIXELV)
// eb[x] += col * ratio / 256;
static void
set_err(ColorRGBint16 *eb, int x, const ColorRGBint32 *col, int ratio)
{
	eb[x].r = saturate_adderr(eb[x].r, col->r * ratio / 256);
	eb[x].g = saturate_adderr(eb[x].g, col->g * ratio / 256);
	eb[x].b = saturate_adderr(eb[x].b, col->b * ratio / 256);
}
#endif

// eb[x] += col >> shift
// シフト演算だけにしたもの。
static inline void
set_err_asr(ColorRGBint16 *eb, int x, const ColorRGBint32 *col, int shift)
{
	eb[x].r = saturate_adderr(eb[x].r, col->r >> shift);
	eb[x].g = saturate_adderr(eb[x].g, col->g >> shift);
	eb[x].b = saturate_adderr(eb[x].b, col->b >> shift);
}

static inline uint8
saturate_uint8(int val)
{
	if (val < 0) {
		return 0;
	}
	if (val > 255) {
		return 255;
	}
	return (uint8)val;
}

static inline int16
saturate_adderr(int16 a, int b)
{
	int16 val = a + b;
	if (val < -512) {
		return -512;
	} else if (val > 511) {
		return 511;
	} else {
		return val;
	}
}


//
// パレット
//

// グレースケール用のパレットを作成して返す。
static ColorRGB *
image_alloc_gray_palette(uint count)
{
	ColorRGB *pal = malloc(sizeof(ColorRGB) * count);
	if (pal == NULL) {
		return NULL;
	}
	for (uint i = 0; i < count; i++) {
		uint8 gray = i * 255 / (count - 1);
		ColorRGB c;
		c.u32 = RGBToU32(gray, gray, gray);
		pal[i] = c;
	}

	return pal;
}

// count 段階グレースケールになっている c からパレット番号を返す。
static uint
finder_gray(image_reductor_handle *ir, ColorRGB c)
{
	uint count = ir->dstimg->palette_count;

	int I = (((uint)c.r) * (count - 1) + (255 / count)) / 255;
	if (I >= count) {
		return count - 1;
	}
	return I;
}

// c をグレー (NTSC 輝度) に変換する。
static void
colorcvt_gray(ColorRGBint32 *c)
{
	int I = RGBToY(c->r, c->g, c->b);
	c->r = I;
	c->g = I;
	c->b = I;
}

// RGB 固定8色。
static const ColorRGB palette_fixed8[] = {
	{ RGBToU32(  0,   0,   0) },
	{ RGBToU32(255,   0,   0) },
	{ RGBToU32(  0, 255,   0) },
	{ RGBToU32(255, 255,   0) },
	{ RGBToU32(  0,   0, 255) },
	{ RGBToU32(255,   0, 255) },
	{ RGBToU32(  0, 255, 255) },
	{ RGBToU32(255, 255, 255) },
};

static uint
finder_fixed8(image_reductor_handle *ir, ColorRGB c)
{
#if defined(__m68k__)
	// $RRGGBB00 の各最上位ビットを下位3ビット %BGR にする。
	uint32 dst = 0;
	__asm(
		"	add.w	%1,%1	;\n"	// B -> X
		"	addx.l	%0,%0	;\n"
		"	swap	%1		;\n"
		"	add.b	%1,%1	;\n"	// G -> X
		"	addx.l	%0,%0	;\n"
		"	add.w	%1,%1	;\n"	// R -> X
		"	addx.l	%0,%0	;\n"
		: /* Output */
			"+d" (dst)
		: /* Input */
			"d" (c.u32)
	);
	return dst;
#else
	uint R = ((uint8)c.r >= 128);
	uint G = ((uint8)c.g >= 128);
	uint B = ((uint8)c.b >= 128);
	return R | (G << 1) | (B << 2);
#endif
}

// VGA 固定 16 色。
// ANSI16 の Standard VGA colors を基準とし、
// ただしパレット4 を Brown ではなく Yellow になるようにしてある。
static const ColorRGB palette_vga16[] = {
	{ RGBToU32(  0,   0,   0) },
	{ RGBToU32(170,   0,   0) },
	{ RGBToU32(  0, 170,   0) },
	{ RGBToU32(170, 170,   0) },
	{ RGBToU32(  0,   0, 170) },
	{ RGBToU32(170,   0, 170) },
	{ RGBToU32(  0, 170, 170) },
	{ RGBToU32(170, 170, 170) },
	{ RGBToU32( 85,  85,  85) },
	{ RGBToU32(255,  85,  85) },
	{ RGBToU32( 85, 255,  85) },
	{ RGBToU32(255, 255,  85) },
	{ RGBToU32( 85,  85, 255) },
	{ RGBToU32(255,  85, 255) },
	{ RGBToU32( 85, 255, 255) },
	{ RGBToU32(255, 255, 255) },
};

// 色 c を VGA 固定 16 色パレットへ変換する。
static uint
finder_vga16(image_reductor_handle *ir, ColorRGB c)
{
	uint R;
	uint G;
	uint B;
	uint I = (uint)c.r + (uint)c.g + (uint)c.b;

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
		return R | (G << 1) | (B << 2);
	}
}

#if defined(SIXELV)

// R3,G3,B2 の固定 256 色パレットを作成して返す。
static ColorRGB *
image_alloc_fixed256_palette(void)
{
	ColorRGB *pal = malloc(sizeof(ColorRGB) * 256);
	if (pal == NULL) {
		return NULL;
	}
	for (uint i = 0; i < 256; i++) {
		ColorRGB c;
		c.r = (((i >> 5) & 0x07) * 255) / 7;
		c.g = (((i >> 2) & 0x07) * 255) / 7;
		c.b = (( i       & 0x03) * 255) / 3;
		pal[i] = c;
	}

	return pal;
}

// 固定 256 色で c に最も近いパレット番号を返す。
static uint
finder_fixed256(image_reductor_handle *ir, ColorRGB c)
{
	uint R = c.r >> 5;
	uint G = c.g >> 5;
	uint B = c.b >> 6;
	return (R << 5) | (G << 2) | B;
}

// xterm 互換の固定 256 色パレットを作成して返す。
static ColorRGB *
image_alloc_xterm256_palette(void)
{
	ColorRGB *pal = malloc(sizeof(ColorRGB) * 256);
	int i;

	if (pal == NULL) {
		return NULL;
	}
	// ANSI16 色。
	memcpy(pal, palette_vga16, sizeof(palette_vga16));

	// 216色 (6x6x6)。
	for (i = 0; i < 216; i++) {
		ColorRGB c;

		// レベルは 00, 5f, 87, af, d7, ff で、00 だけ直線上にない。
		c.r = ((i / (6*6)) % 6);
		c.r = c.r == 0 ? 0 : c.r * 0x28 + 0x37;
		c.g = ((i / (6)  ) % 6);
		c.g = c.g == 0 ? 0 : c.g * 0x28 + 0x37;
		c.b = ((i        ) % 6);
		c.b = c.b == 0 ? 0 : c.b * 0x28 + 0x37;
		pal[i + 16] = c;
	}

	// グレー24色。
	for (i = 0; i < 24; i++) {
		ColorRGB c;
		uint8 I = 8 + i * 10;
		c.r = I;
		c.g = I;
		c.b = I;
		pal[i + 16 + 216] = c;
	}

	return pal;
}

// xterm256色の1チャンネル分、0 .. 5 を返す。
static inline uint8
finder_xterm256_channel(uint8 c)
{
	// レベル: 00, 5f, 87, af, d7, ff
	// しきい:   2f, 73, 9b, bc, eb

	if (c < 0x73) {
		if (c < 0x2f) {
			c = 0;
		} else {
			c = 1;
		}
	} else {
		c = 2 + (c - 0x73) / 0x28;
	}
	return c;
}

// xterm 互換 256 色で c に最も近いパレット番号を返す。
static uint
finder_xterm256(image_reductor_handle *ir, ColorRGB c)
{
	return 16
		+ finder_xterm256_channel(c.r) * 36
		+ finder_xterm256_channel(c.g) * 6
		+ finder_xterm256_channel(c.b) * 1;
}

#endif // SIXELV

//
// 適応 256 色パレット。
//

struct octree {
	uint32 count;	// ピクセル数
	uint32 r;		// R 合計
	uint32 g;		// G 合計
	uint32 b;		// B 合計
	struct octree *children; // [8]
};

static const uint16 tobits[] = {
	0x0000,	// 0b000000000000000
	0x1000,	// 0b001000000000000
	0x0200,	// 0b000001000000000
	0x1200,	// 0b001001000000000
	0x0040,	// 0b000000001000000
	0x1040,	// 0b001000001000000
	0x0240,	// 0b000001001000000
	0x1240,	// 0b001001001000000
	0x0008,	// 0b000000000001000
	0x1008,	// 0b001000000001000
	0x0208,	// 0b000001000001000
	0x1208,	// 0b001001000001000
	0x0048,	// 0b000000001001000
	0x1048,	// 0b001000001001000
	0x0248,	// 0b000001001001000
	0x1248,	// 0b001001001001000
	0x0001,	// 0b000000000000001
	0x1001,	// 0b001000000000001
	0x0201,	// 0b000001000000001
	0x1201,	// 0b001001000000001
	0x0041,	// 0b000000001000001
	0x1041,	// 0b001000001000001
	0x0241,	// 0b000001001000001
	0x1241,	// 0b001001001000001
	0x0009,	// 0b000000000001001
	0x1009,	// 0b001000000001001
	0x0209,	// 0b000001000001001
	0x1209,	// 0b001001000001001
	0x0049,	// 0b000000001001001
	0x1049,	// 0b001000001001001
	0x0249,	// 0b000001001001001
	0x1249,	// 0b001001001001001
};

// octree に色を配置する。
// bits は c の R,G,B (の上の 5 ビット) を左右逆にして R,G,B シャッフルした
// ビット列で、これで下から3ビットずつ取り出すとそのまま octree の各階層の
// インデックスになる。
//  14 13 12 11 10  9  8  7  6  5  4  3  2  1  0
// +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
// |R3|G3|B3|R4|G4|B4|R5|G5|B5|R6|G6|B6|R7|G7|B7|
// +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
static bool
octree_set(struct octree *node, uint32 bits, ColorRGB c, uint32 count)
{
	for (uint lv = 0; lv < 5; lv++) {
		// node->count は自ノード以下のピクセル数なので、途中にもすべて加算。
		node->count += count;

		if (__predict_false(node->children == NULL)) {
			node->children = calloc(8, sizeof(struct octree));
			if (__predict_false(node->children == NULL)) {
				return false;
			}
		}

		node = &node->children[(bits & 7)];
		bits >>= 3;
	}

	// リーフにデータを置く。
	// ここは色ごとに一度ずつしか呼ばないので代入でいい。
	node->count = count;
	node->r = c.r * count;
	node->g = c.g * count;
	node->b = c.b * count;

	return true;
}

// node 以下で count が最小である、リーフ直上のノードを返す。
// *min は in/out パラメータで、初期 min を渡す。
// min より最小のものが見付かれば *min を更新してそのノードを返す。
// 見付からなければ *min を更新せず NULL を返す。
static struct octree *
octree_find_minnode(struct octree *node, uint32 *min)
{
	if (__predict_false(node->children == NULL)) {
		// リーフ。ここには来ないはず。
		return NULL;
	}

	// 子ノードのいずれかが孫を持つか。
	bool has_grandchild =
		(node->children[0].children != NULL) |
		(node->children[1].children != NULL) |
		(node->children[2].children != NULL) |
		(node->children[3].children != NULL) |
		(node->children[4].children != NULL) |
		(node->children[5].children != NULL) |
		(node->children[6].children != NULL) |
		(node->children[7].children != NULL);

	if (has_grandchild) {
		// 自分はまだ中間ノード。
		struct octree *minnode = NULL;
		for (uint i = 0; i < 8; i++) {
			struct octree *n = octree_find_minnode(&node->children[i], min);
			if (n) {
				minnode = n;
			}
		}
		return minnode;
	} else {
		// 自分の子がリーフのノード。
		if (node->count < *min) {
			*min = node->count;
			return node;
		} else {
			return NULL;
		}
	}
}

// このノードのリーフをマージする。リーフ直上のノードで行うこと。
// 戻り値はマージによって増減したリーフ数。子リーフを1〜8個減らすが、
// 自分が新たにリーフになって 1 増えるので、都合 0 以下になる。
static int
octree_merge_leaves(struct octree *node)
{
	uint32 r = 0;
	uint32 g = 0;
	uint32 b = 0;
	int ndiff = 1;

	for (uint i = 0; i < 8; i++) {
		struct octree *child = &node->children[i];
		if (child->count != 0) {
			ndiff--;
			r += child->r;
			g += child->g;
			b += child->b;
		}
	}
	// count は計算済み。
	node->r = r;
	node->g = g;
	node->b = b;
	free(node->children);
	node->children = NULL;

	return ndiff;
}

// Y (輝度)でソートする。といいつつ Y は .a のところを使っている。
static int
cmp_y(const void *a1, const void *a2)
{
	const ColorRGB *c1 = a1;
	const ColorRGB *c2 = a2;

	return c1->a - c2->a;
}

// node 以下のリーフをパレットに登録していく。
// n は次のパレット番号。
// 戻り値も次のパレット番号。
static uint
octree_make_palette(ColorRGB *pal, uint n, const struct octree *node)
{
	if (node->children) {
		for (uint i = 0; i < 8; i++) {
			n = octree_make_palette(pal, n, &node->children[i]);
		}
	} else {
		if (node->count != 0) {
			uint32 r = node->r / node->count;
			uint32 g = node->g / node->count;
			uint32 b = node->b / node->count;
			// 輝度を計算して A のところに入れておく。
			// (パレットの .A はパレットとしては使っていない)
			uint32 y = RGBToY(r, g, b);

			pal[n].r = r;
			pal[n].g = g;
			pal[n].b = b;
			pal[n].a = y;
			n++;
		}
	}
	return n;
}

// パレットから c に最も近い色のパレット番号を返す。
static uint
finder_linear(image_reductor_handle *ir, ColorRGB c)
{
	uint32 mindist = (uint32)-1;
	uint minidx = 0;

	uint32 yh = RGBToY(c.r, c.g, c.b) >> 5;

#if defined(IMAGE_PROFILE)
	ir->y_count[yh]++;
#endif
	for (uint i = ir->y_lo[yh]; i < ir->y_hi[yh]; i++) {
		const ColorRGB *pal = &ir->dstimg->palette[i];
		int32 dr = c.r - pal->r;
		int32 dg = c.g - pal->g;
		int32 db = c.b - pal->b;
		int32 dist = (dr * dr) + (dg * dg) + (db * db);

		if (dist < mindist) {
			minidx = i;
			if (__predict_false(dist < 8)) {
				break;
			}
			mindist = dist;
		}
	}
	return minidx;
}

// node の子を解放する。node 自身はこの親が解放すること。
static void
octree_free(struct octree *node)
{
	if (node->children) {
		for (uint i = 0; i < 8; i++) {
			octree_free(&node->children[i]);
		}
		free(node->children);
		node->children = NULL;
	}
}

// srcimg から適応パレットを作成。
// gain にはゲインを指定する。
// o 1パス構成なら色を取り出すここでゲイン調整も行うため。
// o 2パス構成なら1パス目でゲインは適用されているので負数を指定すること。
// srcimg->palette_count にはソース画像に使われてる色数を返す(表示用)。
static bool
image_calc_adaptive_palette(image_reductor_handle *ir, struct image *srcimg,
	int gain)
{
	struct image *dstimg = ir->dstimg;
	const uint16 *src = (const uint16 *)srcimg->buf;
	uint palette_count;
	struct octree root;
	bool rv = false;
#if defined(IMAGE_PROFILE)
	struct timespec colormap_start, colormap_end;
	struct timespec octree_start, octree_end;
	struct timespec merge_start, merge_end;
	struct timespec make_start, make_end;
#endif

	// この直後で使う colormap は uint16 * 32768。
	// 一方この関数を終えて reduct 中に使う ir->colorhash も uint16 * 32768 で
	// 両者は使用期間がかぶらないので、一度確保したのを使い回す。
	const uint32 capacity = 32768;
	ir->colorhash = calloc(capacity, sizeof(uint16));
	if (__predict_false(ir->colorhash == NULL)) {
		return false;
	}
	uint16 *colormap = ir->colorhash;

	// src 画像に使われている色を全部取り出す。
	// この時点で R,G,B 各色5ビットで足切りされていて、合計15ビットしか
	// ないので、配列の添字にしてダイレクトアクセスする。
	PROF(colormap_start);
	const uint16 *send = src + srcimg->width * srcimg->height;
	for (const uint16 *s = src; s < send; ) {
		uint16 n = *s++;
		n &= 0x7fff;
		if (__predict_false(gain >= 0)) {
			uint32 r5 = ((n >> 10) & 0x1f) * gain / 256;
			uint32 g5 = ((n >>  5) & 0x1f) * gain / 256;
			uint32 b5 = ( n        & 0x1f) * gain / 256;
			if (__predict_false(r5 > 31)) r5 = 31;
			if (__predict_false(g5 > 31)) g5 = 31;
			if (__predict_false(b5 > 31)) b5 = 31;
			n = RGB555_to_ARGB16(r5, g5, b5);
		}
		// ピクセル数の計数は 0xffff で頭打ちにしておく。
		// 65536 ピクセル以上の色が 256 色以上ある時には困るかも知れないが。
		uint16 count = colormap[n];
		uint16 count1 = count + 1;
		if (__predict_true(count1 != 0)) {
			colormap[n] = count1;
		}
	}
	PROF(colormap_end);

	// octree に配置。
	palette_count = 0;
	PROF(octree_start);
	memset(&root, 0, sizeof(root));
	for (uint i = 0; i < capacity; i++) {
		uint32 count = colormap[i];
		if (__predict_true(count == 0)) {
			continue;
		}
		uint32 r5 = (i >> 10) & 0x1f;
		uint32 g5 = (i >>  5) & 0x1f;
		uint32 b5 = (i      ) & 0x1f;
		uint32 bits = (tobits[r5] << 2) | (tobits[g5] << 1) | tobits[b5];
		ColorRGB c;
		c.r = (r5 << 3);
		c.g = (g5 << 3);
		c.b = (b5 << 3);
		if (__predict_false(octree_set(&root, bits, c, count) == false)) {
			goto abort;
		}
		palette_count++;
	}
	PROF(octree_end);
	srcimg->palette_count = palette_count;

	if (0) {
		for (uint i = 0; i < 8; i++) {
			printf("[%u] %u\n", i, root.children[i].count);
			if (root.children) {
				for (uint j = 0; j < 8; j++) {
					struct octree *n1 = &root.children[j];
					printf(" [%u,%u] %u\n", i, j, n1->count);
					if (n1->children) {
						for (uint k = 0; k < 8; k++) {
							struct octree *n2 = &n1->children[k];
							printf("  [%u,%u,%u] %u\n", i, j, k, n2->count);
						}
					}
				}
			}
		}
	}

	// 指定の色数以下になるまで少ない色をマージしていく。
	PROF(merge_start);
	uint dst_count = dstimg->palette_count;
	while (palette_count > dst_count) {
		//printf("palette_count=%u\n", palette_count);
		uint32 min = -1;
		struct octree *minnode = octree_find_minnode(&root, &min);
		palette_count += octree_merge_leaves(minnode);
	}
	PROF(merge_end);
	dstimg->palette_count = palette_count;

	// パレットにセット。
	PROF(make_start);
	ColorRGB *dstpal = dstimg->palette_buf;
	octree_make_palette(dstpal, 0, &root);
	// パレットを Y (輝度) でソート。
	qsort(dstpal, palette_count, sizeof(dstpal[0]), cmp_y);
	// Y の上位 3 ビット(8通り)に対応する検索範囲を事前に調べておく。
	// どう見ても明るい色なら暗い方と比較する必要ないよねというくらい。
	// margin の値は適当。狭いと速いが最も近い色を拾えない可能性がある、
	// 広いと色は近くなる可能性が上がるが遅くなる。
	// ただしそもそも RGB のユークリッド距離なので細かい事は気にしない。
	const int margin = 20;
	for (uint y = 0; y < 8; y++) {
		int lo = y * 0x20 - margin;
		if (lo < 0)
			lo = 0;
		int hi = y * 0x20 + (0x20 - 1) + margin;
		if (hi > 255)
			hi = 255;

		int i = 0;
		int ylo;
		int yhi;
		for (; i < dstimg->palette_count - 1; i++) {
			if (dstpal[i].a >= lo) {
				break;
			}
		}
		ylo = i;
		for (; i < dstimg->palette_count; i++) {
			if (dstpal[i].a > hi) {
				break;
			}
		}
		yhi = i;

		// 0 個になった場合。
		if (ylo == yhi) {
			if (yhi == dstimg->palette_count) {
				ylo--;
			} else {
				yhi++;
			}
		}
		ir->y_lo[y] = ylo;
		ir->y_hi[y] = yhi;

	}
	PROF(make_end);

	PROF_RESULT("colormap",		colormap);
	PROF_RESULT("octree_set",	octree);
	PROF_RESULT("octree_merge",	merge);
	PROF_RESULT("octree_make",	make);

	// colorhash を本当はここで確保するが
	// 使い終わった colormap とサイズが同じなのでありがたく使い回す。
	// まだパレット引いてない印の (uint16)-1 で初期化する。
	memset(ir->colorhash, 0xff, capacity * sizeof(uint16));

	rv = true;
 abort:
	octree_free(&root);
	return rv;
}

// 適応パレットから c に最も近いパレット番号を返す。
static uint
finder_adaptive(image_reductor_handle *ir, ColorRGB c)
{
	uint32 r5 = c.r >> 3;
	uint32 g5 = c.g >> 3;
	uint32 b5 = c.b >> 3;
	uint32 n = r5 * 32 * 32 + g5 * 32 + b5;
	uint16 cc = ir->colorhash[n];
	if (__predict_false((int16)cc < 0)) {
		// まだパレット番号が入ってなければここで引く。
		c.r = r5 * 8 + 4;
		c.g = g5 * 8 + 4;
		c.b = b5 * 8 + 4;
		cc = finder_linear(ir, c);
		ir->colorhash[n] = cc;
	}
	return cc;
}

#if defined(SIXELV)

//
// enum のデバッグ表示用
//

// ResizeAxis を文字列にする。
// (内部バッファを使う可能性があるため同時に2回呼ばないこと)
const char *
resizeaxis_tostr(ResizeAxis axis)
{
	static const struct {
		ResizeAxis value;
		const char *name;
	} table[] = {
		{ RESIZE_AXIS_BOTH,				"Both" },
		{ RESIZE_AXIS_WIDTH,			"Width" },
		{ RESIZE_AXIS_HEIGHT,			"Height" },
		{ RESIZE_AXIS_LONG,				"Long" },
		{ RESIZE_AXIS_SHORT,			"Short" },
		{ RESIZE_AXIS_SCALEDOWN_BOTH,	"ScaleDownBoth" },
		{ RESIZE_AXIS_SCALEDOWN_WIDTH,	"ScaleDownWidth" },
		{ RESIZE_AXIS_SCALEDOWN_HEIGHT,	"ScaleDownHeight" },
		{ RESIZE_AXIS_SCALEDOWN_LONG,	"ScaleDownLong" },
		{ RESIZE_AXIS_SCALEDOWN_SHORT,	"ScaleDownShort" },
	};

	for (int i = 0; i < countof(table); i++) {
		if (axis == table[i].value) {
			return table[i].name;
		}
	}

	static char buf[16];
	snprintf(buf, sizeof(buf), "%u", (uint)axis);
	return buf;
}

// ReductorDiffuse を文字列にする。
// (内部バッファを使う可能性があるため同時に2回呼ばないこと)
const char *
reductordiffuse_tostr(ReductorDiffuse diffuse)
{
	static const struct {
		ReductorDiffuse value;
		const char *name;
	} table[] = {
		{ DIFFUSE_NONE,		"NONE" },
		{ DIFFUSE_SFL,		"SFL" },
		{ DIFFUSE_FS,		"FS" },
		{ DIFFUSE_ATKINSON,	"ATKINSON" },
		{ DIFFUSE_JAJUNI,	"JAJUNI" },
		{ DIFFUSE_STUCKI,	"STUCKI" },
		{ DIFFUSE_BURKES,	"BURKES" },
		{ DIFFUSE_2,		"2" },
		{ DIFFUSE_3,		"3" },
		{ DIFFUSE_RGB,		"RGB" },
	};

	for (int i = 0; i < countof(table); i++) {
		if (diffuse == table[i].value) {
			return table[i].name;
		}
	}

	static char buf[16];
	snprintf(buf, sizeof(buf), "%u", (uint)diffuse);
	return buf;
}

// ColorMode を文字列にする。
// (内部バッファを使う可能性があるため同時に2回呼ばないこと)
const char *
colormode_tostr(ColorMode color)
{
	static const struct {
		ColorMode mode;
		const char *name;
	} table[] = {
		{ COLOR_MODE_GRAY,			"Gray" },
		{ COLOR_MODE_8_RGB,			"8(RGB)" },
		{ COLOR_MODE_16_VGA,		"16(ANSI VGA)" },
		{ COLOR_MODE_256_RGB332,	"256(RGB332)" },
		{ COLOR_MODE_256_XTERM,		"256(xterm)" },
		{ COLOR_MODE_ADAPTIVE,		"Adaptive" },
	};
	static char buf[16];
	uint colormode = GET_COLOR_MODE(color);

	for (int i = 0; i < countof(table); i++) {
		if (table[i].mode == colormode) {
			if (colormode == COLOR_MODE_GRAY ||
			    colormode == COLOR_MODE_ADAPTIVE)
			{
				uint num = GET_COLOR_COUNT(color);
				snprintf(buf, sizeof(buf), "%s%u", table[i].name, num);
				return buf;
			} else {
				return table[i].name;
			}
		}
	}

	snprintf(buf, sizeof(buf), "0x%x", (uint)color);
	return buf;
}

#endif // SIXELV
