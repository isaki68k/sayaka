/* vi:set ts=4: */
/*
 * Copyright (C) 2015 Y.Sugahara (moveccr)
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
// SIXEL 書き出し
//

#include "common.h"
#include "image_priv.h"
#include <string.h>
#include <sys/time.h>

static bool sixel_preamble(FILE *, const image *, const image_opt *);
static bool sixel_postamble(FILE *);
static bool sixel_convert_normal(FILE *, const image *, const diag *);
static bool sixel_convert_ormode(FILE *, const image *, const diag *);
static void sixel_ormode_h6(string *, uint8 *, const uint8 *, uint, uint, uint);
static void sixel_repunit(string *, uint, uint8);

// SIXEL 中断シーケンスを出力する。
void
image_sixel_abort(FILE *fp)
{
	fputs(CAN ESC "\\", fp);
	fflush(fp);
}

// img を SIXEL に変換して fp に出力する。
// (呼び出し後にフラッシュすること)
bool
image_sixel_write(FILE *fp, const image *img,
	const image_opt *opt, const diag *diag)
{
	struct timeval start, end, result;

	Debug(diag, "%s: source image (%u, %u) %u colors", __func__,
		img->width, img->height, img->palette_count);

	gettimeofday(&start, NULL);

	if (sixel_preamble(fp, img, opt) == false) {
		return false;
	}

	if (opt->output_ormode) {
		if (sixel_convert_ormode(fp, img, diag) == false) {
			return false;
		}
	} else {
		if (sixel_convert_normal(fp, img, diag) == false) {
			return false;
		}
	}

	if (sixel_postamble(fp) == false) {
		return false;
	}

	if (diag_get_level(diag) >= 1) {
		gettimeofday(&end, NULL);
		fflush(fp);
		timersub(&end, &start, &result);
		uint ms = (uint)(result.tv_sec * 1000) + (uint)(result.tv_usec / 1000);
		uint us = (uint)(result.tv_usec % 1000);
		diag_print(diag, "%s: total %u.%03u msec", __func__, ms, us);
	}

	return true;
}

static bool
sixel_preamble(FILE *fp, const image *img, const image_opt *opt)
{
	char buf[40];

	snprintf(buf, sizeof(buf), ESC "P7;%u;q\"1;1;%u;%u",
		opt->output_ormode ? 5 : 1,
		img->width,
		img->height);

	if (fwrite(buf, strlen(buf), 1, fp) < 1) {
		return false;
	}

	// パレットを出力する。
	// "#255;2;255;255;255"
	if (opt->suppress_palette == false) {
		const ColorRGB *col = &img->palette[0];
		for (uint i = 0, end = img->palette_count; i < end; i++, col++) {
			snprintf(buf, sizeof(buf), "#%u;2;%u;%u;%u", i,
				col->r * 100 / 255,
				col->g * 100 / 255,
				col->b * 100 / 255);
			if (fwrite(buf, strlen(buf), 1, fp) < 1) {
				return false;
			}
		}
	}

	return true;
}

bool
sixel_postamble(FILE *fp)
{
#define Postamble ESC "\\"
	if (fwrite(Postamble, strlen(Postamble), 1, fp) < 1) {
		return false;
	}
	return true;
}

#define ADDCHAR(s, ch)	string_append_char(s, ch)

#define REPUNIT(s, n, ptn)	sixel_repunit(s, n, ptn)

// SIXEL 従来モードで出力。
static bool
sixel_convert_normal(FILE *fp, const image *img, const diag *diag)
{
	uint w = img->width;
	uint h = img->height;
	uint palcnt = img->palette_count;
	int16 *min_x = NULL;
	int16 *max_x = NULL;
	string *linebuf = NULL;
	char cbuf[16];
	bool rv = false;

	// カラー番号ごとの、X 座標の min, max を計算する。
	// 16bit なので画像サイズの上限は 65535 x 65535。
	uint mlen = sizeof(uint16) * palcnt;
	min_x = malloc(mlen);
	max_x = malloc(mlen);
	if (min_x == NULL || max_x == NULL) {
		goto abort;
	}

	linebuf = string_alloc(256);
	if (linebuf == NULL) {
		goto abort;
	}

	for (uint y = 0; y < h; y += 6) {
		string_clear(linebuf);

		const uint8 *src = &img->buf[y * w];

		memset(min_x, 0xff, mlen);	// fill as -1
		memset(max_x, 0x00, mlen);	// fill as 0

		// h が 6 の倍数でない時には溢れてしまうので、上界を計算する。
		uint max_dy = 6;
		if (__predict_false(y + max_dy > h)) {
			max_dy = h - y;
		}

		// 各カラーの X 座標範囲を計算する。
		for (uint dy = 0; dy < max_dy; dy++) {
			for (uint x = 0; x < w; x++) {
				uint8 idx = *src++;
				if (min_x[idx] < 0 || min_x[idx] > x) {
					min_x[idx] = x;
				}
				if (max_x[idx] < x) {
					max_x[idx] = x;
				}
			}
		}

		for (;;) {
			// 出力するべきカラーがなくなるまでのループ。
			int16 mx = -1;

			for (;;) {
				// 1行の出力で出力できるカラーのループ。
				uint8 min_color = 0;
				int16 min = INT16_MAX;

				// min_x から、mx より大きいもののうち最小のカラーを探して、
				// 塗っていく。
				for (uint c = 0; c < palcnt; c++) {
					if (mx < min_x[c] && min_x[c] < min) {
						min_color = c;
						min = min_x[c];
					}
				}
				// なければ抜ける。
				if (min_x[min_color] <= mx) {
					break;
				}

				// SIXEL に色コードを出力。
				snprintf(cbuf, sizeof(cbuf), "#%u", min_color);
				string_append_cstr(linebuf, cbuf);

				// 相対 X シーク処理。
				int space = min_x[min_color] - (mx + 1);
				if (space > 0) {
					REPUNIT(linebuf, space, 0);
				}

				// パターンが変わったら、それまでのパターンを出していく
				// アルゴリズム。
				uint8 prev_t = 0;
				uint n = 0;
				for (uint x = min_x[min_color]; x <= max_x[min_color]; x++) {
					uint8 t = 0;
					for (uint dy = 0; dy < max_dy; dy++) {
						uint8 idx = img->buf[(y + dy) * w + x];
						if (idx == min_color) {
							t |= 1U << dy;
						}
					}

					if (prev_t != t) {
						if (n > 0) {
							REPUNIT(linebuf, n, prev_t);
						}
						prev_t = t;
						n = 1;
					} else {
						n++;
					}
				}
				// 最後のパターン。
				if (prev_t != 0 && n > 0) {
					REPUNIT(linebuf, n, prev_t);
				}

				// X 位置を更新
				mx = max_x[min_color];
				// 済んだ印
				min_x[min_color] = -1;
			}

			ADDCHAR(linebuf, '$');

			// 最後までやったら抜ける。
			if (mx == -1) {
				break;
			}
		}

		ADDCHAR(linebuf, '-');

		if (fwrite(string_get(linebuf), string_len(linebuf), 1, fp) < 1) {
			goto abort;
		}
	}

	rv = true;
 abort:
	string_free(linebuf);
	free(min_x);
	free(max_x);
	return rv;
}

static uint
mylog2(uint n)
{
#if defined(HAVE___BUILTIN_POPCOUNT)
	return 32 - __builtin_popcount(n);
#else
	for (uint i = 0; i < 8; i++) {
		if (n <= (1U << i)) {
			return i;
		}
	}
	return 8;
#endif
}

// SIXEL OR モードで出力。
static bool
sixel_convert_ormode(FILE *fp, const image *img, const diag *diag)
{
	const uint8 *src = img->buf;
	uint w = img->width;
	uint h = img->height;
	uint palcnt = img->palette_count;
	string *linebuf = NULL;
	uint8 *sixelbuf = NULL;
	uint y;
	bool rv = true;

	// パレットのビット数 (0 は来ないはず)
	uint nplane = mylog2(palcnt);
	linebuf = string_alloc((w + 5) * nplane);
	if (linebuf == NULL) {
		goto done;
	}

	sixelbuf = malloc(w * nplane);
	if (sixelbuf == NULL) {
		goto done;
	}

	// 6ラスターずつ。
	for (y = 0; y < h - 6; y += 6) {
		sixel_ormode_h6(linebuf, sixelbuf, src, w, 6, nplane);
		if (fwrite(string_get(linebuf), string_len(linebuf), 1, fp) < 1) {
			goto done;
		}
		string_clear(linebuf);
		src += w * 6;
	}

	// 最終 SIXEL 行。
	sixel_ormode_h6(linebuf, sixelbuf, src, w, h - y, nplane);
	if (fwrite(string_get(linebuf), string_len(linebuf), 1, fp) < 1) {
		goto done;
	}

	rv = true;
 done:
	free(sixelbuf);
	string_free(linebuf);
	return rv;
}

// sixelbuf は毎回同じサイズなので呼び出し元で一度だけ確保しておく。
static void
sixel_ormode_h6(string *dst, uint8 *sixelbuf, const uint8 *src,
	uint width, uint height, uint nplane)
{
	uint8 *buf;

	// y = 0 のケースで初期化も同時に実行する。
	buf = sixelbuf;
	for (uint x = 0; x < width; x++) {
		uint8 cc = *src++;
		for (uint i = 0; i < nplane; i++) {
			*buf++ = (cc & 1) << 0;
			cc >>= 1;
		}
	}

	// y >= 1 は重ねていく。
	for (uint y = 1; y < height; y++) {
		buf = sixelbuf;
		for (uint x = 0; x < width; x++) {
			uint8 cc = *src++;
			for (uint i = 0; i < nplane; i++) {
				*buf |= (cc & 1) << y;
				buf++;
				cc >>= 1;
			}
		}
	}

	// 各プレーンデータを SIXEL に変換。
	for (uint i = 0; i < nplane; i++) {
		buf = &sixelbuf[i];

		string_append_char(dst, '#');
		char numbuf[12];
		snprintf(numbuf, sizeof(numbuf), "%u", 1U << i);
		string_append_cstr(dst, numbuf);

		// [0]
		uint rept = 1;
		uint8 ptn = *buf;
		buf += nplane;

		// 1 から
		for (uint x = 1; x < width; x++, buf += nplane) {
			if (ptn == *buf) {
				rept++;
			} else {
				sixel_repunit(dst, rept, ptn);
				rept = 1;
				ptn = *buf;
			}
		}
		// 末尾の 0 パターンは出力しなくていい。
		if (ptn != 0) {
			sixel_repunit(dst, rept, ptn);
		}
		string_append_char(dst, '$');
	}

	// 復帰を改行に書き換える。
	char *p = string_get_buf(dst);
	p[string_len(dst) - 1] = '-';
}

static void
sixel_repunit(string *s, uint n, uint8 ptn)
{
	ptn += 0x3f;

	if (n >= 4) {
		char buf[16];
		snprintf(buf, sizeof(buf), "!%u%c", n, ptn);
		string_append_cstr(s, buf);
	} else {
		for (uint i = 0; i < n; i++) {
			string_append_char(s, ptn);
		}
	}
}
