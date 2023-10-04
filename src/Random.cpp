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

#include "Random.h"
#include <random>

// コンストラクタ
Random::Random()
{
	// 乱数のシードを用意。
	std::random_device gen;
	std::mt19937 engine(gen());
	std::uniform_int_distribution<> rand(0);
	seed = rand(engine);
}

// 32 ビットの乱数を返す。
uint32
Random::Get()
{
	// xorshift

	uint32 y = seed;

	y ^= y << 13;
	y ^= y >> 17;
	y ^= y << 15;
	seed = y;

	return y;
}
// dst から dstlen バイトを乱数で埋める。
void
Random::Fill(uint8 *dst, size_t dstlen)
{
	uint8 *d = dst;
	uint i = 0;

	if (__predict_true(((uintmax_t)d & 3) == 0)) {
		uint len4 = (dstlen / 4) * 4;
		for (; i < len4; i += 4) {
			*(uint32 *)d = Get();
			d += 4;
		}
	}

	for (uint32 r = 0; i < dstlen; i++) {
		if (__predict_false((i % 4) == 0)) {
			r = Get();
		}
		*d++ = r;
		r >>= 8;
	}
}
