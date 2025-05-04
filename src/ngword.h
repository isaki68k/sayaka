/* vi:set ts=4: */
/*
 * Copyright (C) 2014-2025 Tetsuya Isaki
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
// NG ワード
//

#ifndef sayaka_ngword_h
#define sayaka_ngword_h

#include <regex.h>

// NG ワードの 1 エントリ。
struct ngword {
	enum {
		NG_NONE,
		NG_TEXT,
		NG_REGEX,
	} ng_type;

	// NG_TEXT  ならそのまま判定文字列。
	// それ以外でも表示用とかのため入力文字列。
	string *ng_text;

	// NG_REGEX ならコンパイルした正規表現。それ以外なら NULL。
	regex_t ng_regex;

	// ユーザ指定があればユーザ名('@' も含む)。
	// ユーザ指定がなければ NULL。
	string *ng_user;
};

// NG ワード一覧。
struct ngwords {
	uint count;
	struct ngword item[0];	// 実際には count 個の配列。
};

extern struct ngwords *ngword_read_file(const char *, const struct diag *);
extern void ngword_destroy(struct ngwords *);

#endif // !sayaka_ngword_h
