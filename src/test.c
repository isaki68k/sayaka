/* vi:set ts=4: */
/*
 * Copyright (C) 2021-2024 Tetsuya Isaki
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

#include "sayaka.h"
#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#define fail(fmt...)	do {	\
	printf("%s: ", __func__);	\
	printf(fmt);	\
	printf("\n");	\
} while(0)

// src の C 文字列をエスケープした文字列を返す。
static string *
string_escape_c(const char *src)
{
	string *dst = string_init();

	char c;
	for (int i = 0; (c = src[i]) != '\0'; i++) {
		// 適当。
		if (c == '\r') {
			string_append_cstr(dst, "\\r");
		} else if (c == '\n') {
			string_append_cstr(dst, "\\n");
		} else if (c == '\t') {
			string_append_cstr(dst, "\\t");
		} else if (c < 0x20) {
			string_append_printf(dst, "\\x%02x", (uint8)c);
		} else if (c == '"') {
			string_append_cstr(dst, "\\\"");
		} else if (c == '\\') {
			string_append_cstr(dst, "\\\\");
		} else {
			string_append_char(dst, c);
		}
	}
	return dst;
}

static void
test_chomp()
{
	printf("%s\n", __func__);

	struct {
		const char *src;
		const char *exp;
	} table[] = {
		{ "",			"" },
		{ "abc",		"abc" },
		{ " abc",		" abc" },
		{ "  abc",		"  abc" },
		{ "a ",			"a " },
		{ "a  ",		"a  " },
		{ "  ab  ",		"  ab  " },
		{ "a\r\n",		"a" },
		{ "\r\r\n\n",	"" },
		{ "\n ab\t \n",	"\n ab\t " },
	};
	for (uint i = 0; i < countof(table); i++) {
		char buf[64];
		const char *src = table[i].src;
		const char *exp = table[i].exp;

		strlcpy(buf, src, sizeof(buf));
		chomp(buf);
		if (strcmp(exp, buf) != 0) {
			string *src_esc = string_escape_c(src);
			string *act_esc = string_escape_c(buf);
			string *exp_esc = string_escape_c(exp);
			fail("\"%s\" expects \"%s\" but \"%s\"\n",
				string_get(src_esc), string_get(exp_esc), string_get(act_esc));
			string_free(src_esc);
			string_free(act_esc);
			string_free(exp_esc);
		}
	}
}

static void
test_decode_isotime(void)
{
	printf("%s\n", __func__);

	struct {
		const char *src;
		time_t expected;
	} table[] = {
		{ "2009-11-18T09:54:12Z",		1258538052 },
		{ "2009-11-18T18:54:12+0900",	1258538052 },
		{ "2009-11-18T18:54:12+09:00",	1258538052 },	// コロンもあり
		{ "2009-11-18T08:24:12-0130",	1258538052 },	// TZが負で、分あり
		{ "2009-11-18T09:54:12.01234Z",	1258538052 },	// 小数部何桁でも可

		{ "2009-11-18T00:00:00",		0 },	// timezone がない
		{ "2009-11-18T00:00:00.Z",		0 },	// 小数部がない
	};
	for (uint i = 0; i < countof(table); i++) {
		const char *src = table[i].src;
		time_t expected = table[i].expected;

		time_t actual = decode_isotime(src);
		if (expected != actual) {
			fail("%s: expects %08lx but %08lx",
				src, (long)expected, (long)actual);
		}
	}
}

static void
test_json_unescape(void)
{
	printf("%s\n", __func__);

	struct {
		const char *src;
		const char *exp;
	} table[] = {
		// src		expected
		{ "",		"" },
		{ "abc",	"abc" },
		{ "\\\"\\\\" "\\/\\b\\f\\n\\r\\t",	"\"\\/\b\f\n\r\t" },
		{ "\\a",	"\\a" },	// どうなる?
		{ "\\u004a",	"J" },
		{ "\\u004A",	"J" },
		{ "\\u004aB",	"JB" },
		{ "\\u3042\\u3044",	"あい" },
	};
	for (uint i = 0; i < countof(table); i++) {
		const char *src = table[i].src;
		const char *exp = table[i].exp;

		string *act = json_unescape(src);
		if (act == NULL) {
			fail("%s: expects %s but NULL", src, exp);
		} else if (strcmp(exp, string_get(act)) != 0) {
			fail("%s: expects %s but %s", src, exp, string_get(act));
		}
	}
}

static void
test_stou32def(void)
{
	printf("%s\n", __func__);

#define DEF (-2)
	struct {
		const char *src;
		uint32 expval;
		uint32 experr;
		uint32 expoff;	// *end の src 先頭からのオフセット
	} table[] = {
		// input		val			error	endoffset
		{ "0",			0,			0,		1 },
		{ "9",			9,			0,		1 },
		{ "12",			12,			0,		2 },
		{ "429496729",	429496729,	0,		9 },	// MAXの一桁少ないやつ
		{ "429496730",	429496730,	0,		9 },
		{ "4294967289",	4294967289, 0,		10 },	// MAX近く
		{ "4294967295",	4294967295, 0,		10 },	// MAX
		{ "4294967296",	DEF,		ERANGE,	-1 },	// 範囲外
		{ "42949672950",DEF,		ERANGE,	-1 },	// MAX より一桁多い
		{ "4294967295a",4294967295,	0,		10 },	// 正常
		{ "",			DEF,		EINVAL,	-1 },	// 空
		{ "-1",			DEF,		EINVAL,	-1 },	// 負数
		{ "-2147483648",DEF,		EINVAL,	-1 },	// 負数(INT_MIN)
		{ "-2147483649",DEF,		EINVAL,	-1 },	// 負数(INT_MIN外)
		{ "-4294967295",DEF,		EINVAL,	-1 },	// 負数(-UINT_MAX)
		{ "1.9",		1,			0,		1 },	// 整数以外は無視
		{ "00000000009",9,			0,		11 },	// 先頭のゼロを8進数にしない
	};
	for (uint i = 0; i < countof(table); i++) {
		const char *src = table[i].src;
		uint32 expval = table[i].expval;
		int    experr = table[i].experr;
		int    expoff = table[i].expoff;

		char *actend = UNCONST(src - 1);
		uint32 actval = stou32def(src, DEF, &actend);
		int actoff = actend - src;
		if (expval != actval)
			fail("%s: val expects %d but %d", src, expval, actval);
		if (expoff != actoff)
			fail("%s: offset expects %d but %d", src, expoff, actoff);
		if (actval == DEF) {
			// errno は失敗した時だけ更新される
			if (experr != errno)
				fail("%s: errno expects %d but %d", src, experr, errno);
		}
	}
}

static void
test_stox32def(void)
{
	printf("%s\n", __func__);

	struct {
		const char *src;
		uint32 expval;
		uint32 experr;
		uint32 expoff;	// *end の src 先頭からのオフセット
	} table[] = {
		// input		val			error	endoffset
		{ "0",			0,			0,		1 },
		{ "9",			0x9,		0,		1 },
		{ "F",			0xf,		0,		1 },
		{ "f",			0xf,		0,		1 },
		{ "1f",			0x1f,		0,		2 },
		{ "fffffff",	0x0fffffff,	0,		7 },	// 一桁少ない
		{ "ffffffff",	0xffffffff,	0,		8 },	// UINT32_MAX
		{ "fffffffff",	DEF,		ERANGE,	-1 },	// 一桁多い
		{ "ffffffffg",	0xffffffff,	0,		8 },	// これは正常...
		{ "",			DEF,		EINVAL,	-1 },	// 空
		{ "-1",			DEF,		EINVAL,	-1 },	// 負数
		{ "0xff",		0,			0,		1 },	// 正常な 0 で終わる...
		{ "1.9",		1,			0,		1 },	// 整数以外は無視
		{ "00000000009",9,			0,		11 },	// 先頭のゼロを8進数にしない
	};
	for (uint i = 0; i < countof(table); i++) {
		const char *src = table[i].src;
		uint32 expval = table[i].expval;
		int    experr = table[i].experr;
		int    expoff = table[i].expoff;

		char *actend = UNCONST(src - 1);
		uint32 actval = stox32def(src, DEF, &actend);
		int actoff = actend - src;
		if (expval != actval)
			fail("%s: val expects 0x%x but 0x%x", src, expval, actval);
		if (expoff != actoff)
			fail("%s: offset expects %d but %d", src, expoff, actoff);
		if (actval == DEF) {
			// errno は失敗した時だけ更新される
			if (experr != errno)
				fail("%s: errno expects %d but %d", src, experr, errno);
		}
	}
#undef DEF
}

static void
test_string_rtrim_inplace(void)
{
	printf("%s\n", __func__);

	struct {
		const char *src;
		const char *exp;
	} table[] = {
		// input		expected
		{ "ab c",		"ab c" },
		{ "ab c \n",	"ab c" },
		{ "a\t \r \n",	"a" },
		{ "\r\n",		"" },
		{ "",			"" },
	};
	for (uint i = 0; i < countof(table); i++) {
		const char *src = table[i].src;
		const char *exp = table[i].exp;

		string *s = string_from_cstr(src);
		string_rtrim_inplace(s);
		if (strcmp(exp, string_get(s)) != 0) {
			string *src_esc = string_escape_c(src);
			string *act_esc = string_escape_c(string_get(s));
			string *exp_esc = string_escape_c(exp);
			fail("\"%s\" expects \"%s\" but \"%s\"\n",
				string_get(src_esc), string_get(exp_esc), string_get(act_esc));
			string_free(src_esc);
			string_free(act_esc);
			string_free(exp_esc);
		}
		string_free(s);
	}
}

static void
test_urlinfo_parse(void)
{
	printf("%s\n", __func__);

	struct {
		const char *src;
		const char *exp_scheme;
		const char *exp_host;
		const char *exp_port;
		const char *exp_user;
		const char *exp_pass;
		const char *exp_path;
		const char *exp_query;
		const char *exp_frag;
	} table[] = {
 // input		scheme	host	port	user,	pass,	path	query	frag
 { "a://b",		"a",	"b",	"",		"",		"",		"/",	"",		"" },
 { "a://b/",	"a",	"b",	"",		"",		"",		"/",	"",		"" },
 { "a://b:c",	"a",	"b",	"c",	"",		"",		"/",	"",		"" },
 { "a://b:c/d",	"a",	"b",	"c",	"",		"",		"/d",	"",		"" },
 { "a://[b:b]/","a",	"b:b",	"",		"",		"",		"/",	"",		"" },
 { "a://[b:b]:c/","a",	"b:b",	"c",	"",		"",		"/",	"",		"" },
 { "a://u@b",	"a",	"b",	"",		"u",	"",		"/",	"",		"" },
 { "a://u:p@b",	"a",	"b",	"",		"u",	"p",	"/",	"",		"" },
 { "/d",		"",		"",		"",		"",		"",		"/d",	"",		"" },
 { "b:c",		"",		"b",	"c",	"",		"",		"/",	"",		"" },
 { "b:c/d/e",	"",		"b",	"c",	"",		"",		"/d/e",	"",		"" },
 { "b/d?q",		"",		"b",	"",		"",		"",		"/d",	"q",	"" },
 { "b/d?q#f",	"",		"b",	"",		"",		"",		"/d",	"q",	"f" },
 { "b/d#f",		"",		"b",	"",		"",		"",		"/d",	"",		"f" },
 { "b/#f",		"",		"b",	"",		"",		"",		"/",	"",		"f" },
 { "http://user:pass@host:port/path?query#frag",
			"http",	"host",	"port",	"user",	"pass",	"/path", "query", "frag" },
 { "http://user:pass@[f::1%0]:port/path",
			"http",	"f::1%0", "port", "user", "pass", "/path", "", "" },
	};

#define TEST(x)	do {	\
	const char *exp_ = exp_##x;	\
	const char *act_ = string_get(info->x);	\
	if (strcmp(exp_, act_) != 0)	\
		fail("\"%s\": %s expects \"%s\" but \"%s\"", src, #x, exp_, act_);	\
} while (0)

	for (uint i = 0; i < countof(table); i++) {
		const char *src = table[i].src;
		const char *exp_scheme	= table[i].exp_scheme;
		const char *exp_host	= table[i].exp_host;
		const char *exp_port	= table[i].exp_port;
		const char *exp_user	= table[i].exp_user;
		const char *exp_password= table[i].exp_pass;
		const char *exp_path	= table[i].exp_path;
		const char *exp_query	= table[i].exp_query;
		const char *exp_fragment= table[i].exp_frag;

		struct urlinfo *info = urlinfo_parse(src);
		if (info == NULL)
			fail("info returned NULL\n");
		TEST(scheme);
		TEST(host);
		TEST(port);
		TEST(user);
		TEST(password);

		// path, query, fragment から pqf の期待値を作成。
		string *s_pqf = string_init();
		string_append_cstr(s_pqf, exp_path);
		if (exp_query[0]) {
			string_append_char(s_pqf, '?');
			string_append_cstr(s_pqf, exp_query);
		}
		if (exp_fragment[0]) {
			string_append_char(s_pqf, '#');
			string_append_cstr(s_pqf, exp_fragment);
		}
		const char *exp_pqf = string_get(s_pqf);
		TEST(pqf);
		string_free(s_pqf);

		urlinfo_free(info);
	}
}

int
main(int ac, char *av[])
{
	test_chomp();
	test_decode_isotime();
	test_json_unescape();
	test_stou32def();
	test_stox32def();
	test_string_rtrim_inplace();
	test_urlinfo_parse();
	return 0;
}