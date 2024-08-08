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
#include "image.h"
#include <string.h>

#define ESC "\x1b"

static bool sixel_preamble(FILE *, const struct image *,
	const struct sixel_opt *);
static bool sixel_postamble(FILE *);
static bool sixel_core(FILE *, const struct image *, const struct diag *);
static uint sixel_repunit(char *, uint, uint, uint8);

// img を SIXEL に変換して fp に出力する。
// (呼び出し後にフラッシュすること)
bool
image_sixel_write(FILE *fp, const struct image *img,
	const struct sixel_opt *opt, const struct diag *diag)
{
	if (sixel_preamble(fp, img, opt) == false) {
		return false;
	}

	if (sixel_core(fp, img, diag) == false) {
		return false;
	}

	if (sixel_postamble(fp) == false) {
		return false;
	}

	return true;
}

static bool
sixel_preamble(FILE *fp, const struct image *img,
	const struct sixel_opt *opt)
{
	char buf[40];

	snprintf(buf, sizeof(buf), ESC "P7;%u;q\"1;1;%u;%u",
		opt->output_ormode ? 5 : 1,
		img->width,
		img->height);

	if (fwrite(buf, 1, strlen(buf), fp) < 0) {
		return false;
	}

	// パレットを出力する。
	// "#255;2;255;255;255"
	if (opt->disable_palette == false) {
		const ColorRGB *col = &img->palette[0];
		for (uint i = 0, end = img->palette_count; i < end; i++, col++) {
			snprintf(buf, sizeof(buf), "#%u;2;%u;%u;%u", i,
				col->r * 100 / 255,
				col->g * 100 / 255,
				col->b * 100 / 255);
			if (fwrite(buf, 1, strlen(buf), fp) < 0) {
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
	if (fwrite(Postamble, 1, strlen(Postamble), fp) < 0) {
		return false;
	}
	return true;
}

#define ADDCHAR(buf, pos, ch)	do {	\
	if (__predict_true(pos < sizeof(buf))) {	\
		buf[pos++] = ch;	\
		buf[pos] = '\0';	\
	}	\
} while (0)

#define REPUNIT(linebuf, pos, n, ptn)	do {	\
	pos += sixel_repunit(linebuf + pos, sizeof(linebuf) - pos, n, ptn);	\
} while (0)

static bool
sixel_core(FILE *fp, const struct image *img, const struct diag *diag)
{
	uint w = img->width;
	uint h = img->height;
	uint palcnt = img->palette_count;
	int16 *min_x = NULL;
	int16 *max_x = NULL;
	char linebuf[1024];	// XXX どうする
	bool rv = false;

	// カラー番号ごとの、X 座標の min, max を計算する。
	// 16bit なので画像サイズの上限は 65535 x 65535。
	uint mlen = sizeof(uint16) * palcnt;
	min_x = malloc(mlen);
	max_x = malloc(mlen);
	if (min_x == NULL || max_x == NULL) {
		goto abort;
	}

	for (uint y = 0; y < h; y += 6) {
		uint pos = 0;
		linebuf[pos] = '\0';

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
				pos += snprintf(linebuf + pos, sizeof(linebuf) - pos,
					"#%u", min_color);

				// 相対 X シーク処理。
				int space = min_x[min_color] - (mx + 1);
				if (space > 0) {
					REPUNIT(linebuf, pos, space, 0);
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
							REPUNIT(linebuf, pos, n, prev_t);
						}
						prev_t = t;
						n = 1;
					} else {
						n++;
					}
				}
				// 最後のパターン。
				if (prev_t != 0 && n > 0) {
					REPUNIT(linebuf, pos, n, prev_t);
				}

				// X 位置を更新
				mx = max_x[min_color];
				// 済んだ印
				min_x[min_color] = -1;
			}

			ADDCHAR(linebuf, pos, '$');

			// 最後までやったら抜ける。
			if (mx == -1) {
				break;
			}
		}

		ADDCHAR(linebuf, pos, '-');

		if (fwrite(linebuf, 1, strlen(linebuf), fp) < 0) {
			goto abort;
		}
	}

	rv = true;
 abort:
	free(min_x);
	free(max_x);
	return rv;
}

static uint
sixel_repunit(char *buf, uint bufsize, uint n, uint8 ptn)
{
	uint r = 0;

	ptn += 0x3f;
	if (n >= 4) {
		r = snprintf(buf, bufsize, "!%u%c", n, ptn);
	} else {
		if (bufsize > n) {
			for (r = 0; r < n; r++) {
				buf[r] = ptn;
			}
			buf[r] = '\0';
		}
	}
	return r;
}
