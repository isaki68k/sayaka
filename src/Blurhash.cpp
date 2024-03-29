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
	int val = 0;
	for (int i = 0; i < len; i++) {
		uint32 c = hash[pos + i] - 0x20;
		if (__predict_false((uint8)c >= 0x60)) {
			return -1;
		}
		uint32 d = table_base83[c];
		if (__predict_false((int8)d < 0)) {
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

	// cos() を c == 1 のときだけ計算する。
	// c >= 2 のときは間引きに相当するので再計算する必要はない。
	// bases はここでフルリサイズ(初期化)される前提。

	if (comp < 1) {
		return;
	}
	for (int x = 0; x < pixels; x++) {
		bases[x * comp + 0] = 1;
	}
	if (comp < 2) {
		return;
	}

	float scale = M_PI / pixels;
	for (int x = 0; x < pixels; x++) {
		bases[x * comp + 1] = std::cos(scale * x);
	}
	for (int x = 0; x < pixels; x++) {
		for (int c = 2; c < comp; c++) {
			int t;
			t = (c * x) % (2 * pixels);
			if (t < pixels) {
				bases[x * comp + c] = bases[t * comp + 1];
			} else {
				t -= pixels;
				bases[x * comp + c] = -bases[t * comp + 1];
			}
		}
	}
}

/*static*/ std::array<uint8, Blurhash::L2SRGBSize> Blurhash::table_L2SRGB = {
	  0,  34,  49,  61,  71,  79,  86,  93,
	 99, 105, 110, 115, 120, 124, 129, 133,
	137, 141, 145, 148, 152, 155, 158, 162,
	165, 168, 171, 174, 177, 179, 182, 185,
	188, 190, 193, 195, 198, 200, 202, 205,
	207, 209, 212, 214, 216, 218, 220, 223,
	225, 227, 229, 231, 233, 235, 237, 239,
	240, 242, 244, 246, 248, 250, 251, 253,
};

// Base83(?) のデコード表。'\x20'-'\x7f'
/*static*/ std::array<uint8, 0x60> Blurhash::table_base83 = {
	0xff, 0xff, 0xff, 0x3e, 0x3f, 0x40, 0xff, 0xff,
	0xff, 0xff, 0x41, 0x42, 0x43, 0x44, 0x45, 0xff,
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
	0x08, 0x09, 0x46, 0x47, 0xff, 0x48, 0xff, 0x49,
	0x4a, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10,
	0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
	0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20,
	0x21, 0x22, 0x23, 0x4b, 0xff, 0x4c, 0x4d, 0x4e,
	0xff, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a,
	0x2b, 0x2c, 0x2d, 0x2e, 0x2f, 0x30, 0x31, 0x32,
	0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a,
	0x3b, 0x3c, 0x3d, 0x4f, 0x50, 0x51, 0x52, 0xff,
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

#if defined(GEN_BASE83)
// % c++ -I.. -DGEN_BASE83 Blurhash.cpp
// % ./a.out
#include <cstdio>
#include <cstdlib>
#include <string>
int
main(int ac, char *av[])
{
	static const std::string chars =
		"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ"
		"abcdefghijklmnopqrstuvwxyz#$%*+,-.:;=?@[]^_{|}~";

	for (int c = 0x20; c < 0x80; c++) {
		int n;
		auto d = chars.find(c);
		if (d == std::string::npos) {
			n = -1;
		} else {
			n = (int)d;
		}

		printf("%c0x%02x,", ((c % 8) ? ' ' : '\t'), (uint8)n);
		if ((c % 8) == 7) {
			printf("\n");
		}
	}
	return 0;
}
#endif
