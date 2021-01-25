/*
 * Copyright (C) 2021 Tetsuya Isaki
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

#include <cstdio>
#include <cstring>
#include <string>
#include <err.h>
#include <unicode/uchar.h>

// U+0000-FFFF:   Plane0 BMP
// U+10000-1FFFF: Plane1 SMP
// U+20000-2FFFF: Plane2 SIP
// U+30000-3FFFF: Plane3 TIP
//                (Plane4-13 unassigned)
// U+E0000-EFFFF: Plane14 SSP
// U+F0000-FFFFF: Plane15 SPUA-A
// U+100000-10FFFF: Plane16 SPUA-B
//
// これを生成する時点で使ってるライブラリが最新仕様をどのくらい正確に
// さばけるかも分からないし、そもそも範囲外はすべて安全のため 2桁に振って
// あるので、上限についてはあまり気にしないことにする。
// とりあえず BMP、SMP をカバーしておけばいいだろう。
static const int MAXCHARS = 0x20000;

// Unicode 文字 c の文字幅を示す文字を返す。
char
conv(int c)
{
	char r;

	auto eaw = (UEastAsianWidth)u_getIntPropertyValue(c,
		UCHAR_EAST_ASIAN_WIDTH);
	switch (eaw) {
	 case U_EA_NARROW:
	 case U_EA_HALFWIDTH:
		r ='H';
		break;
	 case U_EA_WIDE:
	 case U_EA_FULLWIDTH:
		r ='F';
		break;

	 case U_EA_NEUTRAL:
		r ='N';
		break;
	 case U_EA_AMBIGUOUS:
		r ='A';
		break;
	 default:
		errx(1, "0x%x has unknown width %d\n", c, (int)eaw);
	}

	// どう見ても絵文字が並んでるのに
	// 文字によって Full (全角幅) ではなく Neutral (半角幅) の文字が
	// ちらほらあって、どう考えてもおかしいので、勝手に変更する。
	// クソすぎる…。orz
	auto block = (UBlockCode)ublock_getCode(c);
	switch (block) {
	 case UBLOCK_CONTROL_PICTURES:
	 case UBLOCK_MISCELLANEOUS_SYMBOLS_AND_PICTOGRAPHS:
	 case UBLOCK_SUPPLEMENTAL_SYMBOLS_AND_PICTOGRAPHS:
	 case UBLOCK_SYMBOLS_AND_PICTOGRAPHS_EXTENDED_A:
		if (r == 'N') {
			r = 'F';
		}
		break;
	 default:
		break;
	}

	return r;
}

void
preamble()
{
	// For header
	printf("extern const std::array<uint8,0x%x> eaw2width_packed;\n",
		MAXCHARS / 2);

	printf("#include \"eaw_data.h\"\n");
	printf("\n");
	const char *name = "HFNA";
	for (int i = 0; i < strlen(name); i++) {
		for (int j = 0; j < strlen(name); j++) {
			printf("#define %c%c (0x%x%x)\n", name[i], name[j], i, j);
		}
	}
	printf("\n");
	printf("const std::array<uint8,0x%x> eaw2width_packed = {\n",
		MAXCHARS / 2);
}

void
postamble()
{
	printf("};\n");
}

int
main(int ac, char *av[])
{
	std::string line;
	const int LCHARS = 32;

	preamble();
	for (int i = 0; i < MAXCHARS; ) {
		line += conv(i++);
		line += conv(i++);
		line += ',';
		if (i % LCHARS != 0) {
			line += ' ';
		} else {
			printf(" %s // %04x\n", line.c_str(), (i - LCHARS));
			line.clear();
		}
	}
	postamble();
	return 0;
}
