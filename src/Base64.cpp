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

#include "Base64.h"

std::string
Base64Encode(const std::vector<uint8>& src)
{
	static const char enc[] =
		"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
		"abcdefghijklmnopqrstuvwxyz"
		"0123456789+/";
	std::vector<uint8> tmp;
	std::string base64;
	int i;

	for (i = 0; src.size() - i >= 3; ) {
		// 0000'0011  1111'2222  2233'3333
		uint8 a0 = src[i++];
		uint8 a1 = src[i++];
		uint8 a2 = src[i++];

		tmp.push_back(a0 >> 2);
		tmp.push_back(((a0 & 0x03) << 4) | (a1 >> 4));
		tmp.push_back(((a1 & 0x0f) << 2) | (a2 >> 6));
		tmp.push_back(a2 & 0x3f);
	}

	// 残りは 0,1,2バイト
	if (src.size() - i == 1) {
		uint8 a0 = src[i++];

		tmp.push_back(a0 >> 2);
		tmp.push_back((a0 & 0x03) << 4);
	} else if (src.size() - i == 2) {
		uint8 a0 = src[i++];
		uint8 a1 = src[i++];

		tmp.push_back(a0 >> 2);
		tmp.push_back(((a0 & 0x03) << 4) | (a1 >> 4));
		tmp.push_back(((a1 & 0x0f) << 2));
	}

	for (const auto& c : tmp) {
		base64 += enc[c];
	}
	// 4文字になるようパディング
	while (base64.size() % 4 != 0) {
		base64 += '=';
	}
	return base64;
}
