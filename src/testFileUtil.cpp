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
#include "FileUtil.h"
#include <fcntl.h>
#include <unistd.h>

void
test_FileReadWriteAllText()
{
	// File{Read,Write}AllText() を両方一度にテストする
	printf("%s\n", __func__);

	autotemp filename("a.txt");
	{
		std::string exp = "hoge";

		auto r = FileWriteAllText(filename, exp);
		xp_eq(true, r);

		auto actual = FileReadAllText(filename);
		xp_eq(exp, actual);
	}
	{
		// 空文字列
		std::string exp;

		auto r = FileWriteAllText(filename, exp);
		xp_eq(true, r);

		auto actual = FileReadAllText(filename);
		xp_eq(exp, actual);
	}
}

void
test_FileUtil_Exists()
{
	autotemp filename("a");
	bool r;

	printf("%s\n", __func__);

	// 適当なチェックしかしてない

	// ファイルがない
	r = FileUtil::Exists(filename);
	xp_eq(false, r);

	// ファイルがある
	int fd = open(filename.c_str(), O_WRONLY | O_CREAT, 0644);
	if (fd >= 0)
		close(fd);
	r = FileUtil::Exists(filename);
}

void
test_FileUtil()
{
	test_FileReadWriteAllText();
	test_FileUtil_Exists();
}
