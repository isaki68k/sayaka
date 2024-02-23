/*
 * Copyright (C) 2023 Tetsuya Isaki
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

#include "header.h"
#include "Blurhash.h"
#include <cmath>

struct Blurhash::ColorF
{
	float r;
	float g;
	float b;
};

// コンストラクタ
Blurhash::Blurhash(const std::string& hash_)
	: hash(hash_)
{
}

// この hash が正しそうなら true を返す。
// 長さでしか調べる方法がない。文字列の後ろに改行とかないか気をつけること。
bool
Blurhash::IsValid() const
{
	int comp = Decode83(0, 1);
	if (comp < 0) {
		return false;
	}

	int compx = (comp % 9) + 1;
	int compy = (comp / 9) + 1;
	if (hash.size() != compx * compy * 2 + 4) {
		return false;
	}
	return true;
}

// dst は width * height * 3 バイト確保してあること。
bool
Blurhash::Decode(uint8 *dst, int width, int height)
{
	if (dst == NULL) {
		return false;
	}

	int comp = Decode83(0, 1);
	int compx = (comp % 9) + 1;
	int compy = (comp / 9) + 1;

	// Decode quantized max value.
	maxvalue = DecodeMaxAC(Decode83(1, 1));

	std::vector<ColorF> values;

	// 1つ目
	ColorF col;
	DecodeDC(&col, Decode83(2, 4));
	values.emplace_back(col);

	// 残り
	for (int pos = 6, end = hash.size(); pos < end; pos += 2) {
		int val = Decode83(pos, 2);
		int qr =  val / (19 * 19);
		int qg = (val / 19) % 19;
		int qb =  val % 19;
		col.r = DecodeACq(qr) * maxvalue;
		col.g = DecodeACq(qg) * maxvalue;
		col.b = DecodeACq(qb) * maxvalue;
		values.emplace_back(col);
	}

	std::vector<float> bases_x;
	std::vector<float> bases_y;
	BasesFor(bases_x, width, compx);
	BasesFor(bases_y, height, compy);

	// RGB に展開。
	for (int y = 0; y < height; y++) {
		for (int x = 0; x < width; x++) {
			ColorF c {};

			for (int ny = 0; ny < compy; ny++) {
				for (int nx = 0; nx < compx; nx++) {
					auto base = bases_x[x * compx + nx] *
					            bases_y[y * compy + ny];
					const auto& v = values[ny * compx + nx];
					c.r += v.r * base;
					c.g += v.g * base;
					c.b += v.b * base;
				}
			}
			*dst++ = LinearToSRGB(c.r);
			*dst++ = LinearToSRGB(c.g);
			*dst++ = LinearToSRGB(c.b);
		}
	}

	return true;
}

// hash の pos から len バイトをデコードする。
// len は 1, 2, 4 なので戻り値は (unsigned) int に収まる。
int
Blurhash::Decode83(int pos, int len) const
{
	static const std::string chars =
		"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ"
		"abcdefghijklmnopqrstuvwxyz#$%*+,-.:;=?@[]^_{|}~";

	int val = 0;
	for (int i = 0; i < len; i++) {
		auto d = chars.find(hash[pos + i]);
		if (d == std::string::npos) {
			return -1;
		}
		val = val * 83 + (int)d;
	}
	return val;
}

void
Blurhash::DecodeDC(ColorF *col, int val) const
{
	int r =  val >> 16;
	int g = (val >> 8) & 0xff;
	int b =  val & 0xff;

	col->r = SRGBToLinear(r);
	col->g = SRGBToLinear(g);
	col->b = SRGBToLinear(b);
}

/*static*/ float
Blurhash::DecodeACq(int ival)
{
	ival -= 9;
	int signsq = ival * std::abs(ival);
	return (float)signsq / 81;
}

/*static*/ float
Blurhash::DecodeMaxAC(int ival)
{
	return (float)(ival + 1) / 166;
}

/*static*/ float
Blurhash::SRGBToLinear(int ival)
{
	if (ival <= 0) {
		return 0;
	}
	if (ival >= 255) {
		return 255;
	}

	float v = (float)ival / 255;
	if (v < 0.04045) {
		return v / 12.92;
	} else {
		return std::pow((v + 0.055) / 1.055, 2.4);
	}
}

/*static*/ int
Blurhash::LinearToSRGB(float val)
{
	if (val <= 0) {
		return 0;
	}
	if (val >= 1) {
		return 255;
	}

	int idx = (int)(val * table_L2SRGB.size());
	return table_L2SRGB[idx];
}

void
Blurhash::BasesFor(std::vector<float>& bases, int pixels, int comp)
{
	bases.resize(pixels * comp);

	float scale = M_PI / pixels;
	for (int x = 0; x < pixels; x++) {
		for (int c = 0; c < comp; c++) {
			bases[x * comp + c] = std::cos(scale * c * x);
		}
	}
}

/*static*/ std::array<uint8, Blurhash::L2SRGBSize> Blurhash::table_L2SRGB = {
	  0,   6,  13,  18,  22,  25,  28,  31,
	 34,  36,  38,  40,  42,  44,  46,  48,
	 49,  51,  53,  54,  56,  57,  58,  60,
	 61,  62,  64,  65,  66,  67,  68,  70,
	 71,  72,  73,  74,  75,  76,  77,  78,
	 79,  80,  81,  82,  83,  84,  85,  85,
	 86,  87,  88,  89,  90,  91,  91,  92,
	 93,  94,  95,  95,  96,  97,  98,  98,
	 99, 100, 101, 101, 102, 103, 103, 104,
	105, 105, 106, 107, 107, 108, 109, 109,
	110, 111, 111, 112, 113, 113, 114, 115,
	115, 116, 116, 117, 118, 118, 119, 119,
	120, 120, 121, 122, 122, 123, 123, 124,
	124, 125, 126, 126, 127, 127, 128, 128,
	129, 129, 130, 130, 131, 131, 132, 132,
	133, 133, 134, 134, 135, 135, 136, 136,
	137, 137, 138, 138, 139, 139, 140, 140,
	141, 141, 142, 142, 143, 143, 144, 144,
	145, 145, 145, 146, 146, 147, 147, 148,
	148, 149, 149, 149, 150, 150, 151, 151,
	152, 152, 153, 153, 153, 154, 154, 155,
	155, 155, 156, 156, 157, 157, 158, 158,
	158, 159, 159, 160, 160, 160, 161, 161,
	162, 162, 162, 163, 163, 164, 164, 164,
	165, 165, 166, 166, 166, 167, 167, 167,
	168, 168, 169, 169, 169, 170, 170, 170,
	171, 171, 172, 172, 172, 173, 173, 173,
	174, 174, 174, 175, 175, 176, 176, 176,
	177, 177, 177, 178, 178, 178, 179, 179,
	179, 180, 180, 180, 181, 181, 181, 182,
	182, 183, 183, 183, 184, 184, 184, 185,
	185, 185, 186, 186, 186, 187, 187, 187,
	188, 188, 188, 188, 189, 189, 189, 190,
	190, 190, 191, 191, 191, 192, 192, 192,
	193, 193, 193, 194, 194, 194, 195, 195,
	195, 195, 196, 196, 196, 197, 197, 197,
	198, 198, 198, 199, 199, 199, 199, 200,
	200, 200, 201, 201, 201, 202, 202, 202,
	202, 203, 203, 203, 204, 204, 204, 205,
	205, 205, 205, 206, 206, 206, 207, 207,
	207, 207, 208, 208, 208, 209, 209, 209,
	209, 210, 210, 210, 211, 211, 211, 211,
	212, 212, 212, 213, 213, 213, 213, 214,
	214, 214, 214, 215, 215, 215, 216, 216,
	216, 216, 217, 217, 217, 217, 218, 218,
	218, 219, 219, 219, 219, 220, 220, 220,
	220, 221, 221, 221, 221, 222, 222, 222,
	223, 223, 223, 223, 224, 224, 224, 224,
	225, 225, 225, 225, 226, 226, 226, 226,
	227, 227, 227, 227, 228, 228, 228, 228,
	229, 229, 229, 229, 230, 230, 230, 230,
	231, 231, 231, 231, 232, 232, 232, 232,
	233, 233, 233, 233, 234, 234, 234, 234,
	235, 235, 235, 235, 236, 236, 236, 236,
	237, 237, 237, 237, 238, 238, 238, 238,
	239, 239, 239, 239, 239, 240, 240, 240,
	240, 241, 241, 241, 241, 242, 242, 242,
	242, 243, 243, 243, 243, 243, 244, 244,
	244, 244, 245, 245, 245, 245, 246, 246,
	246, 246, 246, 247, 247, 247, 247, 248,
	248, 248, 248, 249, 249, 249, 249, 249,
	250, 250, 250, 250, 251, 251, 251, 251,
	251, 252, 252, 252, 252, 253, 253, 253,
	253, 253, 254, 254, 254, 254, 255, 255,
};

#if defined(GEN_L2SRGB)
// % c++ -I.. -DGEN_L2SRGB Blurhash.cpp
// % ./a.out 512
#include <cstdio>
#include <cstdlib>
#include <cmath>
int
main(int ac, char *av[])
{
	if (ac < 2) {
		printf("usage: %s <tablesize>\n", av[0]);
		return 1;
	}

	int n = atoi(av[1]);
	for (int i = 0; i < n; i++) {
		float val = (float)i / n;

		if (val < 0.0031308) {
			val = val * 12.92;
		} else {
			val = std::pow(val, (float)1 / 2.4) * 1.055 - 0.055;
		}
		int d = (int)(val * 255 + 0.5);

		printf("%c%3u,", ((i % 8) ? ' ' : '\t'), d);
		if ((i % 8) == 7) {
			printf("\n");
		}
	}
	return 0;
}
#endif
