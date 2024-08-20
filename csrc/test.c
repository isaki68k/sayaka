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

#include "common.h"
#include <err.h>
#include <errno.h>
#include <stdio.h>

#define fail(fmt...)	do {	\
	printf("%s: ", __func__);	\
	printf(fmt);	\
	printf("\n");	\
} while(0)

static void
test_stou32def(void)
{
	printf("%s\n", __func__);

#define DEF (-2)
	struct {
		const char *src;
		uint32 expval;
		uint32 experr;
		uint32 expoff;	// *end の src 先頭からのオフセット
	} table[] = {
		// input		val			error	endoffset
		{ "0",			0,			0,		1 },
		{ "9",			9,			0,		1 },
		{ "12",			12,			0,		2 },
		{ "429496729",	429496729,	0,		9 },	// MAXの一桁少ないやつ
		{ "429496730",	429496730,	0,		9 },
		{ "4294967289",	4294967289, 0,		10 },	// MAX近く
		{ "4294967295",	4294967295, 0,		10 },	// MAX
		{ "4294967296",	DEF,		ERANGE,	-1 },	// 範囲外
		{ "42949672950",DEF,		ERANGE,	-1 },	// MAX より一桁多い
		{ "4294967295a",4294967295,	0,		10 },	// 正常
		{ "",			DEF,		EINVAL,	-1 },	// 空
		{ "-1",			DEF,		EINVAL,	-1 },	// 負数
		{ "-2147483648",DEF,		EINVAL,	-1 },	// 負数(INT_MIN)
		{ "-2147483649",DEF,		EINVAL,	-1 },	// 負数(INT_MIN外)
		{ "-4294967295",DEF,		EINVAL,	-1 },	// 負数(-UINT_MAX)
		{ "1.9",		1,			0,		1 },	// 整数以外は無視
		{ "00000000009",9,			0,		11 },	// 先頭のゼロを8進数にしない
	};
	for (uint i = 0; i < countof(table); i++) {
		const char *src = table[i].src;
		uint32 expval = table[i].expval;
		int    experr = table[i].experr;
		int    expoff = table[i].expoff;

		char *actend = UNCONST(src - 1);
		uint32 actval = stou32def(src, DEF, &actend);
		int actoff = actend - src;
		if (expval != actval)
			fail("%s: val expects %d but %d", src, expval, actval);
		if (expoff != actoff)
			fail("%s: offset expects %d but %d", src, expoff, actoff);
		if (actval == DEF) {
			// errno は失敗した時だけ更新される
			if (experr != errno)
				fail("%s: errno expects %d but %d", src, experr, errno);
		}
	}
#undef DEF
}

int
main(int ac, char *av[])
{
	test_stou32def();
	return 0;
}
