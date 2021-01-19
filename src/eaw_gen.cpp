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
	auto eaw = (UEastAsianWidth)u_getIntPropertyValue(c,
		UCHAR_EAST_ASIAN_WIDTH);
	switch (eaw) {
	 case U_EA_NARROW:
	 case U_EA_HALFWIDTH:
		return 'H';
	 case U_EA_WIDE:
	 case U_EA_FULLWIDTH:
		return 'F';

	 case U_EA_NEUTRAL:
		return 'N';
	 case U_EA_AMBIGUOUS:
		return 'A';
	 default:
		errx(1, "0x%x has unknown width %d\n", c, (int)eaw);
	}
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
