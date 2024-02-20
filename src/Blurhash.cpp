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
#include <array>
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
	for (int pos = 6; pos < hash.size(); pos += 2) {
		DecodeAC(&col, Decode83(pos, 2));
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

void
Blurhash::DecodeAC(ColorF *col, int val) const
{
	int qr =  val / (19 * 19);
	int qg = (val / 19) % 19;
	int qb =  val % 19;

	col->r = SignPow((float)(qr - 9) / 9, 2) * maxvalue;
	col->g = SignPow((float)(qg - 9) / 9, 2) * maxvalue;
	col->b = SignPow((float)(qb - 9) / 9, 2) * maxvalue;
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

	const int N = 256;
	static std::array<uint8, N> cache {};
	static std::array<bool, N> valid {};
	int t = (int)(val * N);
	if (valid[t]) {
		return cache[t];
	}

	if (val < 0.0031308) {
		val = val * 12.92;
	} else {
		val = std::pow(val, (float)1 / 2.4) * 1.055 - 0.055;
	}
	int rv = (int)(val * 255 + 0.5);

	cache[t] = rv;
	valid[t] = true;

	return rv;
}

/*static*/ float
Blurhash::SignPow(float val, float exp)
{
	float r = std::pow(std::abs(val), exp);
	return std::copysign(r, val);
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
