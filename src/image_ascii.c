/* vi:set ts=4: */
/*
 * Copyright (C) 2025 Tetsuya Isaki
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
// ASCII アート書き出し
//

#include "image_priv.h"
#include "sixelv.h"
#include <err.h>

// image を ASCII モザイクで fp に出力する。
// srcimg は桁数x行数 [ピクセル] の画像になっている。
bool
image_ascii_write(FILE *fp, const struct image *img,
	const struct image_opt *opt, const struct diag *diag)
{
	assert(img->format == IMAGE_FMT_AIDX16);

	// 背景色を指定しながら空白を表示。
	const int16 *buf = (const int16 *)img->buf;
	for (uint y = 0; y < img->height; y++) {
		int16 prev = -1;
		for (uint x = 0; x < img->width; x++) {
			int16 cc = *buf++;
			if (cc < 0) {
				cc = -1;
			}
			if (cc != prev) {
				if (cc < 0) {
					// 透過なら一旦色を解除して空白だけを出力。
					fprintf(fp, "\x1b[m");
				} else if (opt->color == COLOR_FMT_256_XTERM) {
					// xterm256 ならカラーコードそのままのはず。
					if (__predict_false(cc < 8)) {
						fprintf(fp, "\x1b[4%um", cc);
					} else {
						fprintf(fp, "\x1b[48;5;%um", cc);
					}
				} else {
					// 他はすべて R/G/B 指定。
					ColorRGB c = img->palette[cc];
					fprintf(fp, "\x1b[48;2;%u;%u;%um", c.r, c.g, c.b);
				}
				prev = cc;
			}
			fputc(' ', fp);
		}
		fprintf(fp, "\x1b[m\n");
	}

	return true;
}
