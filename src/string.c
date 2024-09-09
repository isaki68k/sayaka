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
#include <stdarg.h>

#define xstring string
#define xchar   char
#include "xstring.h"
#undef xstring
#undef xchar

// cstr を複製した文字列を返す。
string *
string_from_cstr(const char *cstr)
{
	uint len = strlen(cstr);
	string *s = string_alloc(len + 1);
	if (s == NULL) {
		return NULL;
	}
	strlcpy(s->buf, cstr, len + 1);
	s->len = len;
	return s;
}

// mem から memlen バイトのゼロ終端していない領域から
// ゼロ終端文字列を作って返す。
string *
string_from_mem(const void *mem, uint memlen)
{
	string *s = string_alloc(memlen + 1);
	if (s == NULL) {
		return NULL;
	}
	memcpy(s->buf, mem, memlen);
	s->buf[memlen] = '\0';
	s->len = memlen;
	return s;
}

// old を複製した文字列を返す。
string *
string_dup(const string *old)
{
	uint len = old->len;
	string *s = string_alloc(len + 1);
	if (s == NULL) {
		return NULL;
	}
	memcpy(s->buf, old->buf, len + 1);
	s->len = len;
	return s;
}

// fp から1行読み込む。
// EOF なら NULL を返す。
string *
string_fgets(FILE *fp)
{
	char buf[4096];
	string *s = string_init();

	while (fgets(buf, sizeof(buf), fp)) {
		string_append_cstr(s, buf);

		uint n = strlen(buf);
		if (n > 0 && buf[n - 1] == '\n') {
			break;
		}
	}

	if (s->len == 0) {
		string_free(s);
		s = NULL;
	}
	return s;
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

// s の文字列を char * として返す。
// s が空の時には呼び出さないこと(?)
char *
string_get_buf(const string *s)
{
	assert(s);

	return s->buf;
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

// s を newlen 文字分が追加できるようブロック単位で拡大する。
#define string_expand(s, newlen)	do {	\
	uint newcap_ = roundup((s)->len + (newlen) + 1, 256);	\
	if (__predict_false(string_realloc(s, newcap_) == false)) {	\
		return;	\
	}	\
} while (0)

// s の末尾に1文字追加する。
void
string_append_char(string *s, char ch)
{
	assert(s);

	string_expand(s, 1);
	s->buf[s->len++] = ch;
	s->buf[s->len] = '\0';
}

// s の末尾に文字列を追加する。
void
string_append_cstr(string *s, const char *cstr)
{
	assert(s);

	string_expand(s, strlen(cstr));
	strlcpy(s->buf + s->len, cstr, s->capacity - s->len);
	s->len += strlen(cstr);
}

// mem から memlen バイトのゼロ終端していない領域を s の末尾に追加して
// ゼロ終端する。
void
string_append_mem(string *s, const void *mem, uint memlen)
{
	assert(s);

	string_expand(s, memlen);
	memcpy(s->buf + s->len, mem, memlen);
	s->len += memlen;
	s->buf[s->len] = '\0';
}

// s の末尾に fmt... を追加する。
void
string_append_printf(string *s, const char *fmt, ...)
{
	va_list ap;
	uint availbytes;	// 空きバイト数
	uint reqlen;		// 必要な文字数

	assert(s);

	// 直接連結を試みる。後ろが空いてれば最速。
	// s->buf が NULL でも vsnprintf() は動作する。
	availbytes = s->capacity - s->len;
	va_start(ap, fmt);
	reqlen = vsnprintf(s->buf + s->len, availbytes, fmt, ap);
	va_end(ap);
	if (reqlen < availbytes) {
		s->len += reqlen;
		return;
	}

	// 今書いたのを取り消して..
	if (s->buf) {
		s->buf[s->len] = '\0';
	}

	// 長さは分かったので広げる。
	string_expand(s, reqlen);

	// 書き込めるはず。
	va_start(ap, fmt);
	reqlen = vsnprintf(s->buf + s->len, s->capacity - s->len, fmt, ap);
	va_end(ap);

	s->len += reqlen;
}

// s の末尾の連続する空白・改行文字を削除する (s を書き換える)
void
string_rtrim_inplace(string *s)
{
	for (; s->len != 0; ) {
		char c = s->buf[s->len - 1];
		if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
			s->len--;
			s->buf[s->len] = '\0';
		} else {
			break;
		}
	}
}
