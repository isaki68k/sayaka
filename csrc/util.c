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

// 乱数で埋める。
void
rnd_fill(void *dst, uint dstsize)
{
	static bool initialized = false;

	if (initialized == false) {
		struct timeval tv;
		gettimeofday(&tv, NULL);
		srandom(tv.tv_sec ^ tv.tv_usec);
		initialized = true;
	}

	for (int i = 0; i < dstsize; ) {
		uint32 r = random();
		uint copylen = MIN(dstsize - i, sizeof(r));
		memcpy((char *)dst + i, &r, copylen);
		i += copylen;
	}
}
