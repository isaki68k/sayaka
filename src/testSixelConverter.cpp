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
#include "SixelConverter.h"

static void
test_enum()
{
	std::vector<std::pair<SixelOutputMode, const std::string>> table_SOM = {
		{ SixelOutputMode::Normal,			"Normal" },
		{ SixelOutputMode::Or,				"Or" },
	};
	for (const auto& a : table_SOM) {
		const auto n = a.first;
		const auto& exp = a.second;
		std::string act(SixelConverter::SOM2str(n));
		xp_eq(exp, act, exp);
	}

	std::vector<std::pair<SixelResizeMode, const std::string>> table_SRM = {
		{ SixelResizeMode::ByLoad,			"ByLoad" },
		{ SixelResizeMode::ByImageReductor,	"ByImageReductor" },
	};
	for (const auto& a : table_SRM) {
		const auto n = a.first;
		const auto& exp = a.second;
		std::string act(SixelConverter::SRM2str(n));
		xp_eq(exp, act, exp);
	}
}

void
test_SixelConverter()
{
	test_enum();
}
