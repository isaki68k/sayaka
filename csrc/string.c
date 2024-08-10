/* vi:set ts=4: */
/*
 * Copyright (C) 2024 Tetsuya Isaki
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
// 独自の文字列型
//

#include "common.h"
#include <string.h>

typedef struct string_ {
	char *buf;		// len == 0 の時 buf を触らないこと。
	uint len;		// 文字列の長さ ('\0' の位置)
	uint capacity;	// 確保してあるバイト数
} string;

// 空の文字列を確保して返す。
string *
string_init(void)
{
	string *s = calloc(1, sizeof(string));
	if (s == NULL) {
		return NULL;
	}
	return s;
}

// 初期確保量を指定する。(文字列は空)
string *
string_alloc(uint capacity)
{
	string *s = string_init();
	if (s == NULL) {
		return NULL;
	}

	string_realloc(s, capacity);
	return s;
}

// 確保量を拡大する。文字列は変わらない。
// new_capacity が現行以下なら何もせず true を返す。
// 拡大出来なければ変更せずに false を返す。
bool
string_realloc(string *s, uint new_capacity)
{
	assert(s);

	if (new_capacity <= s->capacity) {
		return true;
	}

	char *tmp = realloc(s->buf, new_capacity);
	if (tmp == NULL) {
		return false;
	}
	s->buf = tmp;
	s->capacity = new_capacity;
	return true;
}

// s を解放する。
void
string_free(string *s)
{
	if (s) {
		free(s->buf);
		free(s);
	}
}

// s の文字列を返す。
const char *
string_get(const string *s)
{
	assert(s);

	if (__predict_true(s->len != 0)) {
		return s->buf;
	} else {
		return "";
	}
}

// s の文字列の長さを返す。
uint
string_len(const string *s)
{
	assert(s);
	return s->len;
}

// s1 と s2 が同じなら true を返す。
bool
string_equal(const string *s1, const string *s2)
{
	assert(s1);
	assert(s2);

	if (s1->len != s2->len) {
		return false;
	}
	return (strcmp(string_get(s1), string_get(s2)) == 0);
}

// s1 と cstr が同じなら true を返す。
bool
string_equal_cstr(const string *s1, const char *cstr)
{
	assert(s1);
	assert(cstr);

	return (strcmp(string_get(s1), cstr) == 0);
}

// 文字列を空にする。容量は変わらない。
void
string_clear(string *s)
{
	assert(s);

	s->len = 0;
}

// s の末尾に1文字追加する。
void
string_append_char(string *s, char ch)
{
	assert(s);

	if (s->len + 1 >= s->capacity) {
		string_realloc(s, s->capacity + 256);
	}

	s->buf[s->len++] = ch;
	s->buf[s->len] = '\0';
}

// s の末尾に文字列を追加する。
void
string_append_cstr(string *s, const char *cstr)
{
	assert(s);

	uint newlen = s->len + strlen(cstr) + 1;
	uint newcap = roundup(newlen, 256);
	string_realloc(s, newcap);

	strlcpy(s->buf + s->len, cstr, s->capacity - s->len);
	s->len += strlen(cstr);
}