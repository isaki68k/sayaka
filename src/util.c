/* vi:set ts=4: */
/*
 * Copyright (C) 2016-2024 Tetsuya Isaki
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
// sayaka/sixelv 共通の雑多なルーチン
//

#include "common.h"
#include <errno.h>
#include <stdint.h>
#include <string.h>

static uint putd_subr(char *, uint);

// strerror(errno) は Debug() 等のマクロ内から呼ぶことが多いのに
// clang だと errno が再帰展開になるとかで怒られるので、回避のため。
const char *
strerrno(void)
{
	return strerror(errno);
}

// 文字列 s 末尾の連続する改行を取り除く。
void
chomp(char *s)
{
	char *p = s + strlen(s);
	while (--p >= s) {
		if (*p == '\r' || *p == '\n') {
			*p = '\0';
		} else {
			break;
		}
	}
}

// map から arg に対応する値を返す。
// 見付からなければ -1 を返す。
int
parse_optmap(const struct optmap *map, const char *arg)
{
	for (int i = 0; map[i].name; i++) {
		if (strcmp(map[i].name, arg) == 0) {
			return map[i].value;
		}
	}
	return -1;
}

// 文字列 s を radix 進数符号なし整数とみなして数値に変換する、簡易版。
// 数値以外の文字が来たところで変換を終了する。
// 変換できればその値を返す。出来なければ errno をセットし defval を返す。
// errno は EINVAL なら s が NULL か数字で始まっていない(負号を含む)。
// errno が ERANGE なら uint32 で表現出来ない。
// strto*(3) と異なり前置されている空白文字をスキップしない。
// endp が NULL でなければ、*endp に数値の次の文字の位置を返す。
// 変換できなければ endp は変更しない。
static uint32
sto32def(const char *s, uint32 defval, char **endp, uint radix)
{
	uint32 val;
	int error;

	val = 0;
	error = EINVAL;	// 1桁もなければエラー。

	if (__predict_false(s == NULL)) {
		errno = error;
		return defval;
	}

	for (; *s; s++) {
		int c = *s;

		// 10 か 16 のみ。
		if (radix == 10) {
			if (c < '0' || c > '9') {
				break;
			}
			c -= '0';
		} else {
			if ('0' <= c && c <= '9') {
				c -= '0';
			} else if ('A' <= c && c <= 'F') {
				c = c - 'A' + 10;
			} else if ('a' <= c && c <= 'f') {
				c = c - 'a' + 10;
			} else {
				break;
			}
		}

		// オーバーフローチェックは演算ごとに必要。
		uint32 n1 = val * radix;
		uint32 n2 = n1 + c;
		if (__predict_false(n1 < val || n2 < val)) {
			errno = ERANGE;
			return defval;
		}
		val = n2;
		error = 0;
	}

	if (__predict_false(error != 0)) {
		errno = error;
		return defval;
	}

	if (endp) {
		*endp = UNCONST(s);
	}
	return val;
}

uint32
stou32def(const char *s, uint32 defval, char **endp)
{
	return sto32def(s, defval, endp, 10);
}

uint32
stox32def(const char *s, uint32 defval, char **endp)
{
	return sto32def(s, defval, endp, 16);
}

// 10進数(0-99) を BCD(0x00-0x99) に変換するテーブル。
static const uint8 decimal_to_bcd[] = {
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09,
	0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19,
	0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29,
	0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39,
	0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49,
	0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59,
	0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69,
	0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79,
	0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89,
	0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99,
};

// 符号なし整数 n を文字列にして dst に出力する。小さい数を高速に出力したい。
// 戻り値は 出力した文字数。dst はゼロ終端しない。
// つまり int len = putd(buf, n); buf[len] = '\0'; が
// sprintf(dst, "%u", n) と等価になる。
// n の頻度は 1桁:2桁:3桁以上 で、ざっくり 4:4:2 とかそのくらい。
// 3桁の内訳は 100-199:200-299:300-999 でざっくり 6:3:1 くらい。
uint
#if defined(__OpenBSD__)
putd(char *dst, uint n, uint dstsize)
#else
putd(char *dst, uint n)
#endif
{
	// 小さい数優先で、255 までを高速に出力できればそれでいい。

	if (__predict_true(n < 10)) {
		dst[0] = n + '0';
		return 1;
	} else if (__predict_true(n < 100)) {
		uint8 bcd = decimal_to_bcd[n];
		dst[0] = (bcd >> 4)  + '0';
		dst[1] = (bcd & 0xf) + '0';
		return 2;
	} else {
		if (__predict_true(n < 200)) {
			dst[0] = '1';
			n -= 100;
		} else if (__predict_true(n < 300)) {
			dst[0] = '2';
			n -= 200;
		} else {
			return putd_subr(dst, n);
		}
		uint8 bcd = decimal_to_bcd[n];
		dst[1] = (bcd >> 4)  + '0';
		dst[2] = (bcd & 0xf) + '0';
		return 3;
	}
}

static uint
putd_subr(char *dst, uint n)
{
	static const uint32 t[] = {
		1,
		10,
		100,
		1000,
		10000,
		100000,
		1000000,
		10000000,
		100000000,
		1000000000,
	};

	uint len;
	uint i = countof(t) - 1;

	for (; i > 0; --i) {
		if (n >= t[i]) {
			break;
		}
	}
	len = i + 1;

	for (; i > 0; --i) {
		char d = '0';
		uint32 m = t[i];
		d += n / m;
		n = n % m;
		*dst++ = d;
	}
	*dst++ = n + '0';

	return len;
}
