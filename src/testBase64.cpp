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

#include "test.h"
#include "Base64.h"
#include "StringUtil.h"
#include <array>

// テストの表記を簡単にするため
static inline std::string operator"" _hex2str(const char *str, std::size_t len)
{
	std::string v;

	// XXX とりあえず
	if (len % 2 != 0)
		return v;

	for (; *str; str += 2) {
		char buf[3];
		buf[0] = str[0];
		buf[1] = str[1];
		buf[2] = '\0';
		v += stox32def(buf, 0);
	}
	return v;
}

// XXX 今は Base64.cpp に移動しているがとりあえず。
static void
test_Base64Encode()
{
	printf("%s\n", __func__);

	std::vector<std::array<std::string, 2>> table = {
		{ "ABCDEFG",				"QUJDREVGRw==" },
		// From RFC3548
		{ "14fb9c03d97e"_hex2str,	"FPucA9l+" },
		{ "14fb9c03d9"_hex2str,		"FPucA9k=" },
		{ "14fb9c03"_hex2str,		"FPucAw==" },
	};
	for (auto& a : table) {
		const std::string& src = a[0];
		const std::string& exp = a[1];

		std::vector<uint8> input(src.begin(), src.end());
		auto actual = Base64Encode(input);
		xp_eq(exp, actual, src);
	}
}

void
test_Base64()
{
	test_Base64Encode();
}
