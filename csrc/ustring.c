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

//
// Unicode 文字列
//

#include "sayaka.h"
#include <errno.h>
#include <string.h>

typedef uint32 unichar;

#define xstring ustring
#define xchar   unichar
#include "xstring.h"
#undef xstring
#undef xchar

static unichar uchar_from_utf8(const char **);

// UTF-8 文字列 str を ustring に変換する。
ustring *
ustring_from_utf8(const char *cstr)
{
	// 変換後の長さは分からないけど元のバイト数より増えることはないはず。
	uint ulen = strlen(cstr);

	ustring *u = ustring_alloc(ulen + 1);
	if (u == NULL) {
		return NULL;
	}

	uint i;
	for (i = 0; i < ulen && *cstr; i++) {
		u->buf[i] = uchar_from_utf8(&cstr);
	}
	u->buf[i] = 0;
	u->len = i;

	return u;
}

// u を newlen 文字分が追加できるようブロック単位で拡大する。
#define ustring_expand(u, newlen)	do {	\
	uint newcap_ = roundup((u)->len + (newlen) + 1, 64);	\
	if (__predict_false(ustring_realloc(u, newcap_) == false)) {	\
		return;	\
	}	\
} while (0)

// u の末尾に1文字追加する。
void
ustring_append_char(ustring *u, char ch)
{
	assert(u);

	ustring_expand(u, 1);
	u->buf[u->len++] = ch;
	u->buf[u->len] = '\0';
}

// u の末尾に cstr を追加する。
void
ustring_append_cstr(ustring *u, const char *cstr)
{
	assert(u);

	uint clen = strlen(cstr);
	ustring_expand(u, clen);

	const uint8 *s = (const uint8 *)cstr;
	uint i;
	for (i = 0; i < clen; i++) {
		u->buf[u->len + i] = (unichar)s[i];
	}
	u->len += i;
	u->buf[u->len] = '\0';
}

// UTF-8 文字列の *srcp から始まる1文字を Unicode コードポイントにして返す。
// *srcp には次の位置を書き戻す。
static unichar
uchar_from_utf8(const char **srcp)
{
	const char *src = *srcp;
	unichar code;
	uint bytelen;
	uint8 c;

	// UTF-8 は1バイト目でこの文字のバイト数が分かる。

	c = (uint8)*src;
	if (__predict_true(c < 0x80)) {
		bytelen = 1;
		code = c;
	} else if (__predict_true(0xc2 <= c && c <= 0xdf)) {
		bytelen = 2;
		code = c & 0x1f;
	} else if (__predict_true(0xe0 <= c && c <= 0xef)) {
		bytelen = 3;
		code = c & 0x0f;
	} else if (__predict_true(0xf0 <= c && c <= 0xf4)) {
		bytelen = 4;
		code = c & 0x07;
	} else {
		// 来ないはずだけど、とりあえず
		bytelen = 1;
		code = c;
	}

	// 2バイト目以降。
	uint pos;
	for (pos = 1; pos < bytelen && src[pos]; pos++) {
		code = (code << 6) | (src[pos] & 0x3f);
	}

	// 文字の途中で '\0' になってしまったらエラーだが、
	// 正常系と同じ値でそのまま帰るくらいしか、出来ることはない。

	*srcp = &src[pos];
	return code;
}
