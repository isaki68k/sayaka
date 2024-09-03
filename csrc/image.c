/* vi:set ts=4: */
/*
 * Copyright (C) 2021-2024 Tetsuya Isaki
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
#include <errno.h>
#include <string.h>

typedef struct image_reductor_handle_ image_reductor_handle;

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
	bool is_gray;

	uint gain;

	// 色からパレット番号を検索する関数。
	finder_t finder;

	// パレット (palette_buf か固定パレットを指す)
	const ColorRGB *palette;
	uint palette_count;

	// 動的に作成する場合はここがメモリを所有している。
	ColorRGB *palette_buf;
} image_reductor_handle;

static uint finder_gray(image_reductor_handle *, ColorRGB);
static uint finder_fixed8(image_reductor_handle *, ColorRGB);
static uint finder_ansi16(image_reductor_handle *, ColorRGB);
static uint finder_fixed256(image_reductor_handle *, ColorRGB);
static void colorcvt_gray(image_reductor_handle *, ColorRGBint32 *);
static ColorRGB *image_alloc_gray_palette(uint);
static ColorRGB *image_alloc_fixed256_palette(void);

#if defined(SIXELV)
static void image_reduct_simple(image_reductor_handle *,
	image *, const image *, const diag *diag);
#endif
static bool image_reduct_highquality(image_reductor_handle *,
	image *, const image *, const image_opt *, const diag *diag);
static void set_err(ColorRGBint16 *, int, const ColorRGBint32 *col, int);
static uint8 saturate_uint8(int);
static int16 saturate_adderr(int16, int);

static const ColorRGB palette_fixed8[];
static const ColorRGB palette_ansi16[];

// opt を初期化する。
void
image_opt_init(image_opt *opt)
{
	opt->method  = REDUCT_HIGH_QUALITY;
	opt->diffuse = DIFFUSE_FS;
	opt->color   = ReductorColor_Fixed256;
	opt->gain    = 256;
	opt->output_ormode = false;
	opt->suppress_palette = false;
}

// width_ x height_ x channels_ の image を作成する。
// (バッファは未初期化)
image *
image_create(uint width_, uint height_, uint channels_)
{
	image *img = calloc(1, sizeof(*img));
	if (img == NULL) {
		return NULL;
	}

	img->width = width_;
	img->height = height_;
	img->channels = channels_;
	img->buf = malloc(image_get_stride(img) * img->height);
	if (img->buf == NULL) {
		free(img);
		return NULL;
	}

	return img;
}

// image を解放する。NULL なら何もしない。
void
image_free(image *img)
{
	if (img != NULL) {
		free(img->buf);
		free(img->palette_buf);
		free(img);
	}
}

// ストライドを返す。
uint
image_get_stride(const image *img)
{
	return img->width * img->channels;
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

	if (request_width < 1) {
		request_width = current_width;
	}
	if (request_height < 1) {
		request_height = current_height;
	}

	// 条件を丸めていく
#if defined(SIXELV)
	switch (axis) {
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

	 case RESIZE_AXIS_LONG:
	 case RESIZE_AXIS_SCALEDOWN_LONG:
#endif
		if (current_width >= current_height) {
			axis = RESIZE_AXIS_WIDTH;
		} else {
			axis = RESIZE_AXIS_HEIGHT;
		}
#if defined(SIXELV)
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

	 default:
		__unreachable();
	}

	// 縮小のみ指示。
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
#if defined(SIXELV)
	 case RESIZE_AXIS_BOTH:
		width  = request_width;
		height = request_height;
		break;
#endif

	 case RESIZE_AXIS_WIDTH:
		width  = request_width;
		height = current_height * width / current_width;
		break;

	 case RESIZE_AXIS_HEIGHT:
		height = request_height;
		width  = current_width * height / current_height;
		break;

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

// サポートしているローダを string で返す。表示用。
string *
image_get_loaderinfo(void)
{
	string *s = string_init();

#define ADD(name)	do {	\
	if (string_len(s) != 0)	\
		string_append_cstr(s, ", ");	\
	string_append_cstr(s, name);	\
} while (0)

	// ここはアルファベット順。
	ADD("blurhash");
#if defined(USE_LIBPNG)
	ADD("libpng");
#endif
#if defined(USE_LIBWEBP)
	ADD("libwebp");
#endif
#if defined(USE_STB_IMAGE)
	ADD("stb_image");
#endif

	return s;
}

// pstream から画像を読み込んで image を作成して返す。
// 読み込めなければ errno をセットして NULL を返す。
// 戻り値 NULL で errno = 0 なら画像形式を認識できなかったことを示す。
// ここでは Blurhash は扱わない。
image *
image_read_pstream(pstream *ps, const diag *diag)
{
	int ok = -1;
	FILE *pfp;
	FILE *fp;

	pfp = pstream_open_for_peek(ps);
	if (pfp == NULL) {
		Debug(diag, "pstream_open_for_peek() failed");
		return NULL;
	}

	static const struct {
		image_match_t match;
		image_read_t  read;
		const char *name;
	} loader[] = {
#define ENTRY(name)	{ image_##name##_match, image_##name##_read, #name }
#if defined(USE_LIBWEBP)
		ENTRY(webp),
#endif
#if defined(USE_LIBPNG)
		ENTRY(png),
#endif
#if defined(USE_STB_IMAGE)
		ENTRY(stb),
#endif
#undef ENTRY
	};
	for (uint i = 0; i < countof(loader); i++) {
		ok = loader[i].match(pfp, diag);
		Trace(diag, "Checking %-4s.. %s",
			loader[i].name, (ok ? "matched" : "no"));
		fseek(pfp, 0, SEEK_SET);
		if (ok) {
			fclose(pfp);
			fp = pstream_open_for_read(ps);
			if (fp == NULL) {
				Debug(diag, "%s: pstream_open_for_read() failed", __func__);
				return NULL;
			}
			image *img = loader[i].read(fp, diag);
			fclose(fp);
			return img;
		}
	}

	if (ok < 0) {
		Debug(diag, "%s: no decoders available", __func__);
	} else {
		Debug(diag, "%s: unsupported image format", __func__);
	}

	errno = 0;
	return NULL;
}

// src 画像を (dst_width, dst_height) にリサイズしながら同時に
// colormode (& graycount) に減色した新しい image を作成して返す。
image *
image_reduct(
	const image *src,			// 元画像
	uint dst_width,				// リサイズ後の幅
	uint dst_height,			// リサイズ後の高さ
	const image_opt *opt,		// パラメータ
	const diag *diag)
{
	image *dst;
	image_reductor_handle irbuf, *ir;

	ir = &irbuf;
	memset(ir, 0, sizeof(*ir));
	ir->gain = opt->gain;

	dst = image_create(dst_width, dst_height, 1);
	if (dst == NULL) {
		return NULL;
	}

	// 減色モードからパレットオペレーションを用意。
	switch (opt->color & ReductorColor_MASK) {
	 case ReductorColor_Gray:
	 {
		uint graycount = ((uint)opt->color >> 8) + 1;
		ir->palette_buf = image_alloc_gray_palette(graycount);
		if (ir->palette_buf == NULL) {
			goto abort;
		}
		ir->palette = ir->palette_buf;
		ir->palette_count = graycount;
		ir->finder = finder_gray;
		ir->is_gray = true;
		break;
	 }

	 case ReductorColor_Fixed8:
		ir->finder  = finder_fixed8;
		ir->palette = palette_fixed8;
		ir->palette_count = 8;
		break;

	 case ReductorColor_ANSI16:
		ir->finder  = finder_ansi16;
		ir->palette = palette_ansi16;
		ir->palette_count = 16;
		break;

	 case ReductorColor_Fixed256:
		ir->palette_buf = image_alloc_fixed256_palette();
		if (ir->palette_buf == NULL) {
			goto abort;
		}
		ir->palette = ir->palette_buf;
		ir->palette_count = 256;
		ir->finder = finder_fixed256;
		break;

	 default:
		Debug(diag, "%s: Unsupported color 0x%x", __func__, opt->color);
		goto abort;
	}

#if defined(SIXELV)
	switch (opt->method) {
	 case REDUCT_SIMPLE:
		image_reduct_simple(ir, dst, src, diag);
		break;

	 case REDUCT_HIGH_QUALITY:
#endif
		if (image_reduct_highquality(ir, dst, src, opt, diag) == false) {
			goto abort;
		}
#if defined(SIXELV)
		break;

	 default:
		Debug(diag, "%s: Unsupported method %s", __func__,
			reductormethod_tostr(opt->method));
		goto abort;
	}
#endif

	// 成功したので、使ったパレットを image にコピー。
	// 動的に確保したやつはそのまま所有権を移す感じ。
	dst->palette       = ir->palette;
	dst->palette_count = ir->palette_count;
	dst->palette_buf   = ir->palette_buf;

	return dst;

 abort:
	free(ir->palette_buf);
	image_free(dst);
	return NULL;
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


//
// 減色 & リサイズ
//

#if defined(SIXELV)
// 単純間引き
static void
image_reduct_simple(image_reductor_handle *ir,
	image *dstimg, const image *srcimg, const diag *diag)
{
	uint8 *d = dstimg->buf;
	const uint8 *src = srcimg->buf;
	uint dstwidth  = dstimg->width;
	uint dstheight = dstimg->height;
	uint srcstride = image_get_stride(srcimg);
	Rational ry;
	Rational rx;
	Rational ystep;
	Rational xstep;

	// 水平、垂直方向ともスキップサンプリング。

	rational_init(&ry, 0, 0, dstheight);
	rational_init(&ystep, 0, srcimg->height, dstheight);
	rational_init(&rx, 0, 0, dstwidth);
	rational_init(&xstep, 0, srcimg->width, dstwidth);

	for (uint y = 0; y < dstheight; y++) {
		rational_add(&ry, &ystep);

		rx.I = 0;
		rx.N = 0;
		const uint8 *s0 = &src[ry.I * srcstride];
		for (uint x = 0; x < dstwidth; x++) {
			const uint8 *s = s0 + rx.I * 3;
			rational_add(&rx, &xstep);

			ColorRGBint32 c;
			c.r = *s++;
			c.g = *s++;
			c.b = *s++;
			if (ir->gain != 256) {
				c.r = (uint32)c.r * ir->gain / 256;
				c.g = (uint32)c.g * ir->gain / 256;
				c.b = (uint32)c.b * ir->gain / 256;
			}
			if (ir->is_gray) {
				colorcvt_gray(ir, &c);
			}
			ColorRGB c8;
			c8.r = saturate_uint8(c.r);
			c8.g = saturate_uint8(c.g);
			c8.b = saturate_uint8(c.b);
			uint colorcode = ir->finder(ir, c8);
			*d++ = colorcode;
		}
	}
}
#endif

// 二次元誤差分散法を使用して、出来る限り高品質に変換する。
static bool
image_reduct_highquality(image_reductor_handle *ir,
	image *dstimg, const image *srcimg, const image_opt *opt, const diag *diag)
{
	uint8 *d = dstimg->buf;
	const uint8 *src = srcimg->buf;
	uint dstwidth  = dstimg->width;
	uint dstheight = dstimg->height;
	uint srcstride = image_get_stride(srcimg);
	Rational ry;
	Rational rx;
	Rational ystep;
	Rational xstep;

	// 水平、垂直ともピクセルを平均。
	// 真に高品質にするには補間法を適用するべきだがそこまではしない。

	rational_init(&ry, 0, 0, dstheight);
	rational_init(&ystep, 0, srcimg->height, dstheight);
	rational_init(&rx, 0, 0, dstwidth);
	rational_init(&xstep, 0, srcimg->width, dstwidth);

	// 誤差バッファ
	const int errbuf_count = 3;
	const int errbuf_left  = 2;
	const int errbuf_right = 2;
	int errbuf_width = dstwidth + errbuf_left + errbuf_right;
	int errbuf_len = errbuf_width * sizeof(ColorRGBint16);
	ColorRGBint16 *errbuf[errbuf_count];
	ColorRGBint16 *errbuf_mem = calloc(errbuf_count, errbuf_len);
	if (errbuf_mem == NULL) {
		return false;
	}
	for (int i = 0; i < errbuf_count; i++) {
		errbuf[i] = errbuf_mem + errbuf_left + errbuf_width * i;
	}

	// alpha チャンネルは今はサポートしていない。

	for (uint y = 0; y < dstheight; y++) {
		uint sy0 = ry.I;
		rational_add(&ry, &ystep);
		uint sy1 = ry.I;
		if (sy0 == sy1) {
			sy1 += 1;
		}

		rx.I = 0;
		rx.N = 0;
		for (uint x = 0; x < dstwidth; x++) {
			uint sx0 = rx.I;
			rational_add(&rx, &xstep);
			uint sx1 = rx.I;
			if (sx0 == sx1) {
				sx1 += 1;
			}

			// 画素の平均を求める。
			ColorRGBint32 col = { };
			for (uint sy = sy0; sy < sy1; sy++) {
				const uint8 *s = &src[sy * srcstride + sx0 * 3];
				for (uint sx = sx0; sx < sx1; sx++) {
					col.r += *s++;
					col.g += *s++;
					col.b += *s++;
				}
			}
			uint area = (sy1 - sy0) * (sx1 - sx0);
			col.r /= area;
			col.g /= area;
			col.b /= area;

			if (ir->gain != 256) {
				col.r = (uint32)col.r * ir->gain / 256;
				col.g = (uint32)col.g * ir->gain / 256;
				col.b = (uint32)col.b * ir->gain / 256;
			}

			col.r += errbuf[0][x].r;
			col.g += errbuf[0][x].g;
			col.b += errbuf[0][x].b;

			if (ir->is_gray) {
				colorcvt_gray(ir, &col);
			}

			ColorRGB c8;
			c8.r = saturate_uint8(col.r);
			c8.g = saturate_uint8(col.g);
			c8.b = saturate_uint8(col.b);

			uint colorcode = ir->finder(ir, c8);
			*d++ = colorcode;

			col.r -= ir->palette[colorcode].r;
			col.g -= ir->palette[colorcode].g;
			col.b -= ir->palette[colorcode].b;

			// ランダムノイズを加える。
			if (0) {
			}

#if defined(SIXELV)
			switch (opt->diffuse) {
			 case DIFFUSE_FS:
#endif
				// Floyd Steinberg Method
				set_err(errbuf[0], x + 1, &col, 112);
				set_err(errbuf[1], x - 1, &col, 48);
				set_err(errbuf[1], x    , &col, 80);
				set_err(errbuf[1], x + 1, &col, 16);
#if defined(SIXELV)
				break;
			 case DIFFUSE_ATKINSON:
				// Atkinson
				set_err(errbuf[0], x + 1, &col, 32);
				set_err(errbuf[0], x + 2, &col, 32);
				set_err(errbuf[1], x - 1, &col, 32);
				set_err(errbuf[1], x,     &col, 32);
				set_err(errbuf[1], x + 1, &col, 32);
				set_err(errbuf[2], x,     &col, 32);
				break;
			 case DIFFUSE_JAJUNI:
				// Jarvis, Judice, Ninke
				set_err(errbuf[0], x + 1, &col, 37);
				set_err(errbuf[0], x + 2, &col, 27);
				set_err(errbuf[1], x - 2, &col, 16);
				set_err(errbuf[1], x - 1, &col, 27);
				set_err(errbuf[1], x,     &col, 37);
				set_err(errbuf[1], x + 1, &col, 27);
				set_err(errbuf[1], x + 2, &col, 16);
				set_err(errbuf[2], x - 2, &col,  5);
				set_err(errbuf[2], x - 1, &col, 16);
				set_err(errbuf[2], x,     &col, 27);
				set_err(errbuf[2], x + 1, &col, 16);
				set_err(errbuf[2], x + 2, &col,  5);
				break;
			 case DIFFUSE_STUCKI:
				// Stucki
				set_err(errbuf[0], x + 1, &col, 43);
				set_err(errbuf[0], x + 2, &col, 21);
				set_err(errbuf[1], x - 2, &col, 11);
				set_err(errbuf[1], x - 1, &col, 21);
				set_err(errbuf[1], x,     &col, 43);
				set_err(errbuf[1], x + 1, &col, 21);
				set_err(errbuf[1], x + 2, &col, 11);
				set_err(errbuf[2], x - 2, &col,  5);
				set_err(errbuf[2], x - 1, &col, 11);
				set_err(errbuf[2], x,     &col, 21);
				set_err(errbuf[2], x + 1, &col, 11);
				set_err(errbuf[2], x + 2, &col,  5);
				break;
			 case DIFFUSE_BURKES:
				// Burkes
				set_err(errbuf[0], x + 1, &col, 64);
				set_err(errbuf[0], x + 2, &col, 32);
				set_err(errbuf[1], x - 2, &col, 16);
				set_err(errbuf[1], x - 1, &col, 32);
				set_err(errbuf[1], x,     &col, 64);
				set_err(errbuf[1], x + 1, &col, 32);
				set_err(errbuf[1], x + 2, &col, 16);
				break;
			 case DIFFUSE_2:
				// (x+1,y), (x,y+1)
				set_err(errbuf[0], x + 1, &col, 128);
				set_err(errbuf[1], x,     &col, 128);
				break;
			 case DIFFUSE_3:
				// (x+1,y), (x,y+1), (x+1,y+1)
				set_err(errbuf[0], x + 1, &col, 102);
				set_err(errbuf[1], x,     &col, 102);
				set_err(errbuf[1], x + 1, &col,  51);
				break;
			 case DIFFUSE_RGB:
				errbuf[0][x].r   = saturate_adderr(errbuf[0][x].r,   col.r);
				errbuf[1][x].b   = saturate_adderr(errbuf[1][x].b,   col.b);
				errbuf[1][x+1].g = saturate_adderr(errbuf[1][x+1].g, col.g);
				break;
			}
#endif
		}

		// 誤差バッファをローテート。
		ColorRGBint16 *tmp = errbuf[0];
		for (int i = 0; i < errbuf_count - 1; i++) {
			errbuf[i] = errbuf[i + 1];
		}
		errbuf[errbuf_count - 1] = tmp;
		// errbuf[y] には左マージンがあるのを考慮する。
		memset(errbuf[errbuf_count - 1] - errbuf_left, 0, errbuf_len);
	}

	free(errbuf_mem);
	return true;
}

// eb[x] += col * ratio / 256;
static void
set_err(ColorRGBint16 *eb, int x, const ColorRGBint32 *col, int ratio)
{
	eb[x].r = saturate_adderr(eb[x].r, col->r * ratio / 256);
	eb[x].g = saturate_adderr(eb[x].g, col->g * ratio / 256);
	eb[x].b = saturate_adderr(eb[x].b, col->b * ratio / 256);
}

static uint8
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

static int16
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

// 256 段階グレースケールになっている c からパレット番号を返す。
static uint
finder_gray(image_reductor_handle *ir, ColorRGB c)
{
	uint count = ir->palette_count;

	int I = (((uint)c.r) * (count - 1) + (255 / count)) / 255;
	if (I >= count) {
		return count - 1;
	}
	return I;
}

// c をグレー (NTSC 輝度) に変換する。
static void
colorcvt_gray(image_reductor_handle *ir, ColorRGBint32 *c)
{
	int I = (c->r * 76 + c->g * 153 + c->b * 26) / 255;
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
	uint R = ((uint8)c.r >= 128);
	uint G = ((uint8)c.g >= 128);
	uint B = ((uint8)c.b >= 128);
	return R | (G << 1) | (B << 2);
}

// ANSI 固定 16 色。
// Standard VGA colors を基準とし、
// ただしパレット4 を Brown ではなく Yellow になるようにしてある。
static const ColorRGB palette_ansi16[] = {
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

// 色 c を ANSI 固定 16 色パレットへ変換する。
static uint
finder_ansi16(image_reductor_handle *ir, ColorRGB c)
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

// ReductorMethod を文字列にする。
// (内部バッファを使う可能性があるため同時に2回呼ばないこと)
const char *
reductormethod_tostr(ReductorMethod method)
{
	static const struct {
		ReductorMethod value;
		const char *name;
	} table[] = {
		{ REDUCT_SIMPLE,		"Simple" },
		{ REDUCT_FAST,			"Fast" },
		{ REDUCT_HIGH_QUALITY,	"High" },
	};

	for (int i = 0; i < countof(table); i++) {
		if (method == table[i].value) {
			return table[i].name;
		}
	}

	static char buf[16];
	snprintf(buf, sizeof(buf), "%u", (uint)method);
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

// ReductorColor を文字列にする。
// (内部バッファを使う可能性があるため同時に2回呼ばないこと)
const char *
reductorcolor_tostr(ReductorColor color)
{
	static const struct {
		ReductorColor value;
		const char *name;
	} table[] = {
		{ ReductorColor_Gray,		"Gray" },
		{ ReductorColor_GrayMean,	"GrayMean" },
		{ ReductorColor_Fixed8,		"Fixed8" },
		{ ReductorColor_X68k,		"X68k" },
		{ ReductorColor_ANSI16,		"ANSI16" },
		{ ReductorColor_Fixed256,	"Fixed256" },
		{ ReductorColor_Fixed256I,	"Fixed256I" },
	};
	static char buf[16];
	uint type = (uint)color & ReductorColor_MASK;
	uint num = (uint)color >> 8;

	for (int i = 0; i < countof(table); i++) {
		if (type == table[i].value) {
			// 今のところ Gray は必ず num > 0 なのでこれだけで区別できる
			if (num == 0) {
				return table[i].name;
			} else {
				snprintf(buf, sizeof(buf), "%s%u", table[i].name, num);
				return buf;
			}
		}
	}

	snprintf(buf, sizeof(buf), "0x%x", (uint)color);
	return buf;
}

#endif // SIXELV
