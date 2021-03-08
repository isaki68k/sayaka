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
#include "ParsedUri.h"

void
test_ParsedUri()
{
	printf("%s\n", __func__);

	std::vector<std::array<std::string, 5>> table = {
		// input		scheme	host	port	pqf
		{ "a://b",		"a",	"b",	"",		"/" },
		{ "a://b/",		"a",	"b",	"",		"/" },
		{ "a://b:c",	"a",	"b",	"c",	"/" },
		{ "a://b:c/d",	"a",	"b",	"c",	"/d" },
		{ "/d",			"",		"",		"",		"/d" },
		{ "b:c",		"",		"b",	"c",	"/" },
		{ "b:c/d/e",	"",		"b",	"c",	"/d/e" },
	};

	for (const auto& a : table) {
		auto input      = a[0];
		auto exp_scheme = a[1];
		auto exp_host   = a[2];
		auto exp_port   = a[3];
		auto exp_pqf    = a[4];

		auto uri = ParsedUri::Parse(input);
		xp_eq(exp_scheme, uri.Scheme, input);
		xp_eq(exp_host, uri.Host, input);
		xp_eq(exp_port, uri.Port, input);
		xp_eq(exp_pqf, uri.PQF(), input);
	}
}
