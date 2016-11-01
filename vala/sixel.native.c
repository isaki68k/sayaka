/*
 * Copyright (C) 2016 Y.Sugahara (moveccr)
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

/* Sixel 変換 for X680x0 */

#include <stdint.h>
#include <stdio.h>

#define SIXEL_PLANE_COUNT	(4)

// SIXEL 変換中間バッファ
// 必要に応じてアロケートする。
static uint8_t *sixelbuf = NULL;
static size_t sixelbuf_length = 0;

// 10 進変換テーブル
static const uint8_t decimal_table[] = {
0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,
0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,0x18,0x19,
0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27,0x28,0x29,
0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37,0x38,0x39,
0x40,0x41,0x42,0x43,0x44,0x45,0x46,0x47,0x48,0x49,
0x50,0x51,0x52,0x53,0x54,0x55,0x56,0x57,0x58,0x59,
0x60,0x61,0x62,0x63,0x64,0x65,0x66,0x67,0x68,0x69,
0x70,0x71,0x72,0x73,0x74,0x75,0x76,0x77,0x78,0x79,
0x80,0x81,0x82,0x83,0x84,0x85,0x86,0x87,0x88,0x89,
0x90,0x91,0x92,0x93,0x94,0x95,0x96,0x97,0x98,0x99,
};

inline int
sixel_putc(uint8_t *dst, char c)
{
	*dst = c;
	return 1;
}

/* 小さい正の整数をとにかく高速に出力したい */
inline int
sixel_putd(uint8_t *dst, int n)
{
	// 小さい数優先で、255 までを高速に出力できればそれでいい
	int m;
	int rv = 0;
	if (n < 10) {
		return sixel_putc(dst, n + 0x30);
	} else if (n < 100) {
		sixel_putc(dst, (decimal_table[n] >> 4) + 0x30);
		sixel_putc(dst + 1, (decimal_table[n] & 0xf) + 0x30);
		return 2;
	} else {
		if (n < 200) {
			sixel_putc(dst, 1 + 0x30);
			m = n - 100;
		} else if (n < 300) {
			sixel_putc(dst, 2 + 0x30);
			m = n - 200;
		} else {
			return sprintf(dst, "%d", n);
		}
		sixel_putc(dst + 1, (decimal_table[m] >> 4) + 0x30);
		sixel_putc(dst + 2, (decimal_table[m] & 0xf) + 0x30);
		return 3;
	}
}

inline int
sixel_put_repunit(uint8_t *dst, int rep, uint8_t ptn)
{
	if (rep == 1) {
		return sixel_putc(dst, ptn + 0x3f);
	} else if (rep == 2) {
		sixel_putc(dst, ptn + 0x3f);
		sixel_putc(dst + 1, ptn + 0x3f);
		return 2;
	} else if (rep == 3) {
		sixel_putc(dst, ptn + 0x3f);
		sixel_putc(dst + 1, ptn + 0x3f);
		sixel_putc(dst + 2, ptn + 0x3f);
		return 3;
	} else {
		sixel_putc(dst, '!');
		dst++;
		int len = sixel_putd(dst, rep);
		dst += len;
		sixel_putc(dst, ptn + 0x3f);
		return len + 2;
	}
}


/*
 * イメージを SIXEL に 6 ラスタ変換します。
 * dst: 変換結果。
 *      w * 4 + 12 バイト以上確保してください。
 *      (4 プレーン分のデータと、パレットコードと改行指示)
 * src: 画像 1 byte per pixel, max 16 color
 *      LSB 4bit がパレットコードとして読み込まれます。上位ニブルは無視。
 * w: 幅ピクセル
 *      (0 < w && w <= 768) を保証してください。
 * h: 高さピクセル
 *      (1 <= h && h <= 6) を保証してください。
 * return: dst に書き込んだバイト長
 *
 * m68k での速度を優先するため、範囲チェックなどは行いません。
 */
int
sixel_image_to_sixel_h6_ormode(
	uint8_t* dst,
	uint8_t* src,
	int w,
	int h)
{
	// 6 ラスタ、4プレーンデータを作る
	// sixelbuf :=
	//  [X=0,Y=0..6,Plane0]
	//  [X=0,Y=0..6,Plane1]
	//  [X=0,Y=0..6,Plane2]
	//  [X=0,Y=0..6,Plane3]
	//  [X=1,Y=0..6,Plane0]
	//  [X=1,Y=0..6,Plane1]
	//  [X=1,Y=0..6,Plane2]
	//  [X=1,Y=0..6,Plane3]

	size_t required_length = w * SIXEL_PLANE_COUNT;
	if (sixelbuf == NULL) {
		sixelbuf_length = required_length;
		sixelbuf = malloc(sixelbuf_length);
	} else if (sixelbuf_length < required_length) {
		sixelbuf_length = required_length;
		sixelbuf = realloc(sixelbuf, sixelbuf_length);
	}

	uint8_t *buf = sixelbuf;

	// y=0 のケースで初期化も同時に実行する
	for (int x = 0; x < w; x++) {
		uint8_t b = *src++;
		for (int i = 0; i < SIXEL_PLANE_COUNT; i++) {
			*buf++ = (b & 1);
			b >>= 1;
		}
	}

	// y >= 1
	for (int y = 1; y < h; y++) {
		buf = sixelbuf;
		for (int x = 0; x < w; x++) {
			uint8_t b = *src++;
			for (int i = 0; i < SIXEL_PLANE_COUNT; i++) {
				*buf++ |= (b & 1) << y;
				b >>= 1;
			}
		}
	}

	// 各プレーンデータを SIXEL に変換

	uint8_t *dst0 = dst;

	for (int i = 0; i < SIXEL_PLANE_COUNT; i++) {
		buf = sixelbuf + i;
		int rep = 1;
		uint8_t ptn = *buf;
		buf += SIXEL_PLANE_COUNT;

		dst += sixel_putc(dst, '#');
		dst += sixel_putd(dst, 1 << i);

		// 1 から
		for (int x = 1; x < w; x++, buf += SIXEL_PLANE_COUNT) {
			if (ptn == *buf) {
				rep++;
			} else {
				dst += sixel_put_repunit(dst, rep, ptn);
				rep = 1;
				ptn = *buf;
			}
		}
		// 末尾の 0 パターンは出力しなくていい
		if (ptn != 0) {
			dst += sixel_put_repunit(dst, rep, ptn);
		}
		dst += sixel_putc(dst, '$');
	}
	// 復帰を改行に書き換える
	sixel_putc(dst - 1, '-');

	return dst - dst0;
}

