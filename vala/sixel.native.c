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
	int m;
	int rv = 0;
	if (n < 10) {
		goto d1;
	} else if (n < 100) {
		goto d2;
	} else if (n < 1000) {
		goto d3;
	} else {
		return snprintf(dst, "%d", n);
	}

 d3:
	m = n / 100;
	rv += sixel_putc(dst, m + 0x30);
	n = n - m * 100;
 d2:
	m = n / 10;
	rv += sixel_putc(dst, m + 0x30);
	n = n - m * 10;
 d1:
	rv += sixel_putc(dst, n + 0x30);
	return rv;
}

inline int
sixel_put_repunit(uint8_t *dst, int rep, uint8_t ptn)
{
	int rv = 0;
	if (rep == 1) {
		goto d1;
	} else if (rep == 2) {
		goto d2;
	} else if (rep == 3) {
		goto d3;
	} else {
		rv = sixel_putc('!');
		rv += sixel_putd(rep);
		rv += sixel_putc(ptn + 0x3f);
		return rv;
	}

 d3:
	rv += sixel_putc(ptn + 0x3f);
 d2:
	rv += sixel_putc(ptn + 0x3f);
 d1:
	rv += sixel_putc(ptn + 0x3f);

	return rv;
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
sixel_image_to_sixel_ormode_6(
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

	uint8_t *buf = sixelbuf;

	// y=0 のケースで初期化も同時に実行する
	for (int x = 0; x < w; x++) {
		uint8_t b = *src++;
		for (int i = 0; i < 4; i++) {
			*buf++ = (b & 1);
			b >>= 1;
		}
	}

	// y >= 1
	for (int y = 1; y < h; y++) {
		for (int x = 0; x < w; x++) {
			uint8_t b = *src++;
			for (int i = 0; i < 4; i++) {
				*buf++ |= (b & 1) << y;
				b >>= 1;
			}
		}
	}

	// 各プレーンデータを SIXEL に変換

	uint8_t *dst0 = dst;

	for (int i = 0; i < 4; i++) {
		buf = sixelbuf + i;
		int rep = 1;
		uint8_t ptn = *buf;
		dst += sixel_putc(dst, '#');
		dst += sixel_putd(dst, 1 << i);
		// 1 から
		for (int x = 1; x < w; x++, buf += 4) {
			if (ptn == *buf) {
				rep++;
			} else {
				dst += sixel_put_repunit(dst, rep, ptn);
				rep = 1;
				ptn = *buf;
			}
		}
		dst += sixel_put_repunit(dst, rep, ptn);
		dst += sixel_putc(dst, '$');
	}

	return dst - dst0;
}

