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

// デバッグ用診断ツール

#include "Diag.h"
#include <cstdio>
#include <cstdarg>

// コンストラクタ
Diag::Diag()
{
}

// コンストラクタ
Diag::Diag(const std::string& name_)
{
	SetClassname(name_);
}

// クラス名を後から設定する
void
Diag::SetClassname(const std::string& name_)
{
	classname = name_;
	if (!classname.empty()) {
		classname += " ";
	}
}

// デバッグレベルを lv に設定する
void
Diag::SetLevel(int lv)
{
	debuglevel = lv;
}

#define VPRINTF(fmt) do { \
	fputs(classname.c_str(), stderr); \
	va_list ap; \
	va_start(ap, (fmt)); \
	vfprintf(stderr, (fmt), ap); \
	fputs("\n", stderr); \
	va_end(ap); \
} while (0)

// レベル不問のメッセージ (改行はこちらで付加する)
void
Diag::Print(const char *fmt, ...)
{
	VPRINTF(fmt);
}

// レベル可変のメッセージ (改行はこちらで付加する)
void
Diag::Print(int lv, const char *fmt, ...)
{
	if (debuglevel >= lv) {
		VPRINTF(fmt);
	}
}

// デバッグログ表示 (改行はこちらで付加する)
void
Diag::Debug(const char *fmt, ...)
{
	if (debuglevel >= 1) {
		VPRINTF(fmt);
	}
}

// トレースログ表示 (改行はこちらで付加する)
void
Diag::Trace(const char *fmt, ...)
{
	if (debuglevel >= 2) {
		VPRINTF(fmt);
	}
}
