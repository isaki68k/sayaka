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

static bool sixel_preamble(FILE *, const image *, const image_opt *);
static bool sixel_postamble(FILE *);
static bool sixel_convert_normal(FILE *, const image *, const diag *);
static bool sixel_convert_ormode(FILE *, const image *, const diag *);
static uint sixel_ormode_h6(char *, uint8 *, const uint16 *, uint, uint,
	uint);
static uint sixel_repunit(char *, uint, uint8);

static const uint32 deptable[16] = {
	0x00000000,
	0x00000001,
	0x00000100,
	0x00000101,
	0x00010000,
	0x00010001,
	0x00010100,
	0x00010101,
	0x01000000,
	0x01000001,
	0x01000100,
	0x01000101,
	0x01010000,
	0x01010001,
	0x01010100,
	0x01010101,
};

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
	Debug(diag, "%s: source image (%u, %u) %u colors", __func__,
		img->width, img->height, img->palette_count);

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

	return true;
}

static bool
sixel_preamble(FILE *fp, const image *img, const image_opt *opt)
{
	// ヘッダは
	//
	//  +0 +1 +2 +3 +4    +5 +6 +7 +8  +9 +10 +11 +12
	//  ESC P  7  ; <mode> ;  q  " <Ph> ; <Pv>  ; <Width> ; <Height>
	//
	// で、<mode> は通常 1、OR モードなら 5。
	// Ph,Pv は 1。
	static const char head[] = ESC "P7;1;q\"1;1;";
	char buf[40];
	char *p;

	memcpy(buf, head, strlen(head));
	if (opt->output_ormode) {
		buf[4] = '5';
	}
	p = buf + strlen(head);
	p += PUTD(p, img->width, sizeof(buf) - (p - buf));
	*p++ = ';';
	p += PUTD(p, img->height, sizeof(buf) - (p - buf));
	*p = '\0';

	if (fwrite(buf, p - buf, 1, fp) < 1) {
		return false;
	}

	// パレットを出力する。
	// "#255;2;255;255;255"
	if (opt->suppress_palette == false) {
		const ColorRGB *col = &img->palette[0];
		for (uint i = 0, end = img->palette_count; i < end; i++, col++) {
			uint r = col->r * 100 / 255;
			uint g = col->g * 100 / 255;
			uint b = col->b * 100 / 255;
			p = buf;

			*p++ = '#';
			p += PUTD(p, i, sizeof(buf) - (p - buf));	// palette num
			*p++ = ';';
			*p++ = '2';		// RGB
			*p++ = ';';
			p += PUTD(p, r, sizeof(buf) - (p - buf));
			*p++ = ';';
			p += PUTD(p, g, sizeof(buf) - (p - buf));
			*p++ = ';';
			p += PUTD(p, b, sizeof(buf) - (p - buf));
			*p = '\0';

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

// SIXEL 従来モードで出力。
static bool
sixel_convert_normal(FILE *fp, const image *img, const diag *diag)
{
	const uint16 *imgbuf16 = (const uint16 *)img->buf;
	uint w = img->width;
	uint h = img->height;
	uint palcnt = img->palette_count;
	int16 *min_x = NULL;
	int16 *max_x = NULL;
	char *linebuf = NULL;
	bool rv = false;

	assert(img->format == IMAGE_FMT_AIDX16);

	// カラー番号ごとの、X 座標の最小最大(最も左と右の位置)を覚えておくため。
	// 16bit なので画像サイズの上限は 65535 x 65535。
	uint mlen = sizeof(uint16) * palcnt;
	min_x = malloc(mlen);
	max_x = malloc(mlen);
	if (min_x == NULL || max_x == NULL) {
		goto abort;
	}

	// 1行(縦6ピクセル x 全色ではなく、一回の '$'(LF) まで) の最長を求める。
	// 1行で最大 cs = MIN(palcnt, width) 回色を変えることが出来るので
	// 色セレクタ "#nnn" が cs 回、
	// パターンは一切連続しなかったとして width ピクセル分、
	// がもっとも分が悪いケースのはず。ゼロ終端は不要。
	uint cs = MIN(palcnt, w);
	uint bufsize = cs * 4 + w + 1/*$*/;
	linebuf = malloc(bufsize);
	if (linebuf == NULL) {
		goto abort;
	}

	for (uint y = 0; y < h; y += 6) {
		const uint16 *src = &imgbuf16[y * w];

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
				uint16 cc = *src++;
				if ((int16)cc < 0) {
					continue;
				}
				if (min_x[cc] < 0 || min_x[cc] > x) {
					min_x[cc] = x;
				}
				if (max_x[cc] < x) {
					max_x[cc] = x;
				}
			}
		}

		for (;;) {
			// 出力するべきカラーがなくなるまでのループ。
			int16 mx = -1;
			char *d = linebuf;

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
				*d++ = '#';
				d += PUTD(d, min_color, bufsize - (d - linebuf));

				// 相対 X シーク処理。
				int space = min_x[min_color] - (mx + 1);
				if (space > 0) {
					d += sixel_repunit(d, space, 0);
				}

				// パターンが変わったら、それまでのパターンを出していく
				// アルゴリズム。
				uint8 prev_t = 0;
				uint n = 0;
				for (uint x = min_x[min_color]; x <= max_x[min_color]; x++) {
					uint8 t = 0;
					for (uint dy = 0; dy < max_dy; dy++) {
						uint16 idx = imgbuf16[(y + dy) * w + x];
						if (idx == min_color) {
							t |= 1U << dy;
						}
					}

					if (prev_t != t) {
						if (n > 0) {
							d += sixel_repunit(d, n, prev_t);
						}
						prev_t = t;
						n = 1;
					} else {
						n++;
					}
				}
				// 最後のパターン。
				if (prev_t != 0 && n > 0) {
					d += sixel_repunit(d, n, prev_t);
				}

				// X 位置を更新。
				mx = max_x[min_color];
				// 済んだ印。
				min_x[min_color] = -1;
			}

			*d++ = '$';
			if (fwrite(linebuf, d - linebuf, 1, fp) < 1) {
				goto abort;
			}

			// 最後までやったら抜ける。
			if (mx == -1) {
				break;
			}
		}

		if (fputc('-', fp) < 0) {
			goto abort;
		}
	}

	rv = true;
 abort:
	free(linebuf);
	free(min_x);
	free(max_x);
	return rv;
}

static uint
mylog2(uint n)
{
#if defined(HAVE___BUILTIN_CLZ)
	return 31 - __builtin_clz(n);
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
	const uint16 *src = (const uint16 *)img->buf;
	uint w = img->width;
	uint h = img->height;
	uint palcnt = img->palette_count;
	char *linebuf = NULL;
	uint8 *sixelbuf = NULL;
	int y;
	int len;
	bool rv = true;

	// パレットのビット数。(0 は来ないはず)
	uint nplane = mylog2(palcnt);

	// 1行は "#n" <pattern*width> "$" (n は1桁、ゼロ終端不要) なので、
	// パターンが一切連続しなくても絶対溢れないはず。
	linebuf = malloc((w + 3) * nplane);
	if (linebuf == NULL) {
		goto done;
	}

	sixelbuf = malloc(w * nplane);
	if (sixelbuf == NULL) {
		goto done;
	}

	// 6ラスターずつ。
	for (y = 0; y < (int)h - 6; y += 6) {
		len = sixel_ormode_h6(linebuf, sixelbuf, src, w, 6, nplane);
		if (fwrite(linebuf, len, 1, fp) < 1) {
			goto done;
		}
		src += w * 6;
	}

	// 最終 SIXEL 行。
	len = sixel_ormode_h6(linebuf, sixelbuf, src, w, h - y, nplane);
	if (fwrite(linebuf, len, 1, fp) < 1) {
		goto done;
	}

	rv = true;
 done:
	free(sixelbuf);
	free(linebuf);
	return rv;
}

// 戻り値は dst に書き込んだバイト数。
// sixelbuf は毎回同じサイズなので呼び出し元で一度だけ確保しておく。
static uint
sixel_ormode_h6(char *dst, uint8 *sixelbuf, const uint16 *src,
	uint width, uint height, uint nplane)
{
	char *d = dst;
	uint8 *buf;

	// sixelbuf は画素を以下の順に並び替えたもの。(nplane=4 の場合)
	// [0] Y=0..5, X=0, Plane=0
	// [1] Y=0..5, X=0, Plane=1
	// [2] Y=0..5, X=0, Plane=2
	// [3] Y=0..5, X=0, Plane=3
	// [4] Y=0..5, X=1, Plane=0
	// :

#if 0
	// y = 0 のケースで初期化も同時に実行する。
	buf = sixelbuf;
	for (uint x = 0; x < width; x++) {
		uint16 cc = *src++;
		if ((int16)cc < 0) {
			cc = 0;
		}
		for (uint i = 0; i < nplane; i++) {
			*buf++ = (cc & 1) << 0;
			cc >>= 1;
		}
	}

	// y >= 1 は重ねていく。
	for (uint y = 1; y < height; y++) {
		buf = sixelbuf;
		for (uint x = 0; x < width; x++) {
			uint16 cc = *src++;
			if ((int16)cc <= 0) {
				buf += nplane;
				continue;
			}
			for (uint i = 0; i < nplane; i++) {
				*buf |= (cc & 1) << y;
				buf++;
				cc >>= 1;
			}
		}
	}
#else
	// 縦 6 ピクセルとプレーン(最大8)の水平垂直変換。
	//       bn      b2   b1   b0            b5   b4   b3   b2   b1   b0
	// [0] Y0Pn … Y0P2 Y0P1 Y0P0      [0] Y5P0 Y4P0 Y3P0 Y2P0 Y1P0 Y0P0
	// [1] Y1Pn … Y1P2 Y1P1 Y1P0  ==> [1] Y5P1 Y4P1 Y3P1 Y2P1 Y1P1 Y0P1
	//  :                               :
	// [5] Y5Pn … Y5P2 Y5P1 Y5P0      [n] Y5Pn Y4Pn Y3Pn Y2Pn Y1Pn Y0Pn

	buf = sixelbuf;
	if (nplane <= 4) {
		for (uint x = 0; x < width; x++) {
			uint32 data0 = 0;
			for (uint y = 0; y < height; y++) {
				uint16 cc = src[width * y];
				if ((int16)cc > 0) {
					data0 |= deptable[cc] << y;
				}
			}
			src++;

			for (uint i = 0; i < nplane; i++) {
				*buf++ = data0 & 0xff;
				data0 >>= 8;
			}
		}
	} else {
		for (uint x = 0; x < width; x++) {
			uint32 data0 = 0;
			uint32 data1 = 0;
			for (uint y = 0; y < height; y++) {
				uint16 cc = src[width * y];
				if ((int16)cc > 0) {
					data0 |= deptable[cc & 0xf] << y;
					data1 |= deptable[cc >> 4] << y;
				}
			}
			src++;

			uint i = 0;
			for (; i < 4; i++) {
				*buf++ = data0 & 0xff;
				data0 >>= 8;
			}
			for (; i < nplane; i++) {
				*buf++ = data1 & 0xff;
				data1 >>= 8;
			}
		}
	}
#endif

	// 各プレーンデータを SIXEL に変換。
	for (uint i = 0; i < nplane; i++) {
		buf = &sixelbuf[i];

		*d++ = '#';
		d += PUTD(d, (1U << i), 10/*適当*/);

		// [0]
		uint rept = 1;
		uint8 ptn = *buf;
		buf += nplane;

		// 1 から。
		for (uint x = 1; x < width; x++, buf += nplane) {
			if (ptn == *buf) {
				rept++;
			} else {
				d += sixel_repunit(d, rept, ptn);
				rept = 1;
				ptn = *buf;
			}
		}
		// 末尾の 0 パターンは出力しなくていい。
		if (ptn != 0) {
			d += sixel_repunit(d, rept, ptn);
		}
		*d++ = '$';
	}

	// 復帰を改行に書き換える。
	d--;
	*d++ = '-';

	return d - dst;
}

// ptn が n 個連続する場合のデータを出力する。
// 出力したバイト数を返す。
static uint
sixel_repunit(char *dst, uint n, uint8 ptn)
{
	char *d = dst;
	ptn += 0x3f;

	switch (n) {
	 default:
		*d++ = '!';
		d += PUTD(d, n, 10/*適当*/);
		*d++ = ptn;
		break;
	 case 3:
		*d++ = ptn;
		// FALLTHROUGH
	 case 2:
		*d++ = ptn;
		// FALLTHROUGH
	 case 1:
		*d++ = ptn;
		// FALLTHROUGH
	 case 0:
		break;
	}
	return d - dst;
}
