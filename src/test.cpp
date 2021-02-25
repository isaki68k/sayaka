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
#include <inttypes.h>
#include "ChunkedInputStream.h"
#include "Diag.h"
#include "Dictionary.h"
#include "FileUtil.h"
#include "ImageReductor.h"
#include "MemoryStream.h"
#include "NGWord.h"
#include "OAuth.h"
#include "ParsedUri.h"
#include "RichString.h"
#include "SixelConverter.h"
#include "StringUtil.h"
#include "Twitter.h"
#include "UString.h"
#include "acl.h"
#include "eaw_code.h"
#include "subr.h"
#include "term.h"

int test_count;
int test_fail;

void
xp_eq_(const char *file, int line, const char *func,
	int exp, int act, const std::string& msg)
{
	test_count++;

	if (exp != act) {
		test_fail++;
		printf("%s:%d: %s(%s) expects %d but %d\n",
			file, line, func, msg.c_str(), exp, act);
	}
}

void
xp_eq_(const char *file, int line, const char *func,
	uint64 exp, uint64 act, const std::string& msg)
{
	test_count++;

	if (exp != act) {
		test_fail++;
		printf("%s:%d: %s(%s) expects %" PRIu64 "d but %" PRIu64 "d\n",
			file, line, func, msg.c_str(), exp, act);
	}
}

void
xp_eq_(const char *file, int line, const char *func,
	const std::string& exp, const std::string& act, const std::string& msg)
{
	test_count++;

	if (exp != act) {
		test_fail++;
		printf("%s:%d: %s(%s) expects \"%s\" but \"%s\"\n",
			file, line, func, msg.c_str(), exp.c_str(), act.c_str());
	}
}

void
xp_fail_(const char *file, int line, const char *func,
	const std::string& msg)
{
	test_count++;
	test_fail++;
	printf("%s:%d: %s(%s) failed\n", file, line, func, msg.c_str());
}

int
main(int ac, char *av[])
{
	test_count = 0;
	test_fail = 0;

	test_ChunkedInputStream();
	test_Diag();
	test_Dictionary();
	test_FileUtil();
	test_ImageReductor();
	test_MemoryInputStream();
	test_NGWord();
	test_OAuth();
	test_ParsedUri();
	test_RichString();
	test_SixelConverter();
	test_StringUtil();
	test_Twitter();
	test_UString();
	test_eaw_code();
	test_subr();
	test_term();

	printf("%d tests", test_count);
	if (test_fail == 0) {
		printf(", all passed.\n");
	} else {
		printf(", %d faild!!\n", test_fail);
	}

	// acl はこの中で独自にカウントしている
	test_acl();
	return 0;
}
