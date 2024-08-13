/* vi:set ts=4: */
/*
 * Copyright (C) 2015 Y.Sugahara (moveccr)
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

//
// デバッグ用診断ツール
//

#include "common.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

// 新しい diag を確保して返す。
struct diag *
diag_alloc(void)
{
	struct diag *diag = calloc(1, sizeof(struct diag));
	return diag;
}

// diag を解放する。NULL なら何もしない。
void
diag_free(struct diag *diag)
{
	if (diag != NULL) {
		free(diag);
	}
}

// クラス名を設定する。
void
diag_set_name(struct diag *diag, const char *name)
{
	assert(diag);
	assert(name);

	strlcpy(diag->name, name, sizeof(diag->name));
}

// デバッグレベルを lv に設定する。
void
diag_set_level(struct diag *diag, int level_)
{
	assert(diag);

	diag->level = level_;
}

// タイムスタンプを有効にする。
void
diag_set_timestamp(struct diag *diag, bool enable)
{
	assert(diag);

	diag->timestamp = enable;
}

// メッセージ出力 (改行はこちらで付加する)。
void
diag_print(const struct diag *diag, const char *fmt, ...)
{
	va_list ap;

	assert(diag);

	if (__predict_false(diag->timestamp)) {
		struct timeval now;
		gettimeofday(&now, NULL);
		struct tm *tm = localtime(&now.tv_sec);
		uint ms = now.tv_usec / 1000;
		fprintf(stderr, "%02u:%02u:%02u.%03u ",
			tm->tm_hour, tm->tm_min, tm->tm_sec, ms);
	}

	fputs(diag->name, stderr);

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);

	fputs("\n", stderr);
}
