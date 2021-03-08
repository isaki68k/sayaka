/*
 * Copyright (C) 2015 Y.Sugahara (moveccr)
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

#include "FileUtil.h"
#include "autofd.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>

// .Net の File.ReadAllText() のようなもの。
// ただしエンコーディングはサポートしていない。
std::string
FileReadAllText(const std::string& filename)
{
	struct stat st;
	autofd fd;
	void *m;

	fd = open(filename.c_str(), O_RDONLY);
	if (fd < 0) {
		return "";
	}

	if (fstat(fd, &st) < 0) {
		return "";
	}

	m = mmap(NULL, st.st_size, PROT_READ, MAP_FILE | MAP_SHARED, fd, 0);
	if (m == MAP_FAILED) {
		return "";
	}

	// ファイル全域を文字列とする
	std::string rv((const char *)m, st.st_size);

	munmap(m, st.st_size);
	return rv;
}

// .Net の File.WriteAllText() のようなもの。
bool
FileWriteAllText(const std::string& filename, const std::string& text)
{
	autofd fd;

	fd = open(filename.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (fd < 0) {
		return false;
	}

	auto r = write(fd, text.c_str(), text.size());
	if (r < text.size()) {
		return false;
	}
	return true;
}

// ファイルが存在すれば true を返す。
bool
FileUtil::Exists(const std::string& filename)
{
	if (access(filename.c_str(), F_OK) < 0) {
		return false;
	}
	return true;
}
