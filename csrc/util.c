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
// 雑多なルーチン
//

#include "common.h"
#include <errno.h>
#include <string.h>
#include <sys/time.h>

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

// 32ビットの乱数を返す。
uint32
rnd_get32(void)
{
	static bool initialized = false;

	if (__predict_false(initialized == false)) {
		struct timeval tv;
		gettimeofday(&tv, NULL);
		srandom(tv.tv_sec ^ tv.tv_usec);
		initialized = true;
	}

	return random();
}

// 乱数で埋める。
void
rnd_fill(void *dst, uint dstsize)
{
	for (int i = 0; i < dstsize; ) {
		uint32 r = rnd_get32();
		uint copylen = MIN(dstsize - i, sizeof(r));
		memcpy((char *)dst + i, &r, copylen);
		i += copylen;
	}
}

// BASE64 エンコードした文字列を返す。
string *
base64_encode(const void *vsrc, uint srclen)
{
	static const char enc[] =
		"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
		"abcdefghijklmnopqrstuvwxyz"
		"0123456789+/";

	const uint8 *src = (const uint8 *)vsrc;
	uint dstlen = ((srclen + 2) / 3) * 4;
	string *dst = string_alloc(dstlen + 1);
	uint i;

	uint packed = (srclen / 3) * 3;
	for (i = 0; i < packed; ) {
		// 0000'0011  1111'2222  2233'3333
		uint8 a0 = src[i++];
		uint8 a1 = src[i++];
		uint8 a2 = src[i++];

		string_append_char(dst, enc[a0 >> 2]);
		string_append_char(dst, enc[((a0 & 0x03) << 4) | (a1 >> 4)]);
		string_append_char(dst, enc[((a1 & 0x0f) << 2) | (a2 >> 6)]);
		string_append_char(dst, enc[a2 & 0x3f]);
	}

	// 残りは 0, 1, 2 バイト。
	uint remain = srclen - packed;
	if (remain == 1) {
		uint8 a0 = src[i];

		string_append_char(dst, enc[a0 >> 2]);
		string_append_char(dst, enc[((a0 & 0x03) << 4)]);
	} else if (remain == 2) {
		uint8 a0 = src[i++];
		uint8 a1 = src[i];

		string_append_char(dst, enc[a0 >> 2]);
		string_append_char(dst, enc[((a0 & 0x03) << 4) | (a1 >> 4)]);
		string_append_char(dst, enc[((a1 & 0x0f) << 2)]);
	}

	// パディング。
	while ((string_len(dst) % 4) != 0) {
		string_append_char(dst, '=');
	}

	return dst;
}
