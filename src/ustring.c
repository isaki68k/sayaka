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
#if defined(HAVE_ICONV)
#include <iconv.h>
#endif

#define xstring ustring
#define xchar   unichar
#include "xstring.h"
#undef xstring
#undef xchar

#if defined(HAVE_ICONV)
static string *utf8_to_outcode(const string *);
#endif
static unichar uchar_from_utf8(const char **);

// 出力文字コードが UTF-8 以外 (iconv による変換が必要) なら true。
static bool use_iconv;

#if defined(HAVE_ICONV)
// UTF-8 以外の文字コードへの変換用。
static iconv_t cd;

// 代替文字
static string *altchar;
#endif

// 文字コードの初期化。
// codeset は出力文字コード名。NULL なら文字コードを (UTF-8 から) 変換しない。
// 失敗すれば errno をセットし false を返す。
bool
init_codeset(const char *codeset)
{
	use_iconv = false;

	if (codeset == NULL) {
		return true;
	} else {
#if defined(HAVE_ICONV)
		// UTF-8 以外なら iconv を使う。
		cd = iconv_open(codeset, "utf-8");
		if (cd == (iconv_t)-1) {
			return false;
		}
		use_iconv = true;

		// UTF-8 を codeset に変換出来なかった時に出力する代替文字(ゲタ)を
		// ここで用意しておく。
		string *altsrc = string_from_cstr("〓");
		if (altsrc) {
			altchar = utf8_to_outcode(altsrc);
		}
		string_free(altsrc);

		return true;
#else
		// UTF-8 以外が指定されたのに iconv がなければエラー。
		// (iconv_open() のエラーと区別つけるため errno = 0 にする)
		errno = 0;
		return false;
#endif
	}
}

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

// u の生配列を返す。
const unichar *
ustring_get(const ustring *u)
{
	assert(u);

	if (__predict_true(u->len != 0)) {
		return u->buf;
	} else {
		static const unichar empty[1] = {
			0,
		};
		return empty;
	}
}

// u の i 番目の文字を返す。
// i が範囲外なら安全に 0 を返す。
unichar
ustring_at(const ustring *u, int i)
{
	if (i < 0 || i >= u->len) {
		return 0;
	}
	return u->buf[i];
}

// u を newlen 文字分が追加できるようブロック単位で拡大する。
#define ustring_expand(u, newlen)	do {	\
	uint newcap_ = roundup((u)->len + (newlen) + 1, 64);	\
	if (__predict_false(ustring_realloc(u, newcap_) == false)) {	\
		return;	\
	}	\
} while (0)

// u の末尾に ustring t を追加する。
void
ustring_append(ustring *u, const ustring *t)
{
	assert(u);
	assert(t);

	ustring_expand(u, t->len);
	uint i;
	for (i = 0; i < t->len; i++) {
		u->buf[u->len + i] = t->buf[i];
	}
	u->len += t->len;
	u->buf[u->len] = '\0';
}

// u の末尾に unichar 1文字追加する。
void
ustring_append_unichar(ustring *u, unichar ch)
{
	assert(u);

	ustring_expand(u, 1);
	u->buf[u->len++] = ch;
	u->buf[u->len] = '\0';
}

// u の末尾に cstr を追加する。
// ただし文字コードは変換しないので置けるのは ASCII のみ。
void
ustring_append_ascii(ustring *u, const char *cstr)
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

// u の末尾に UTF-8 文字列 cstr を追加する。
void
ustring_append_utf8(ustring *u, const char *cstr)
{
	assert(u);

	ustring *t = ustring_from_utf8(cstr);
	ustring_append(u, t);
	ustring_free(t);
}

// u の ASCII 大文字を小文字に変換する。u を書き換える。
void
ustring_tolower_inplace(ustring *u)
{
	for (uint i = 0; i < u->len; i++) {
		if ('A' <= u->buf[i] && u->buf[i] <= 'Z') {
			u->buf[i] += 0x20;
		}
	}
}

// src を UTF-8 文字列に変換して返す。
string *
ustring_to_utf8(const ustring *src)
{
	assert(src);

	string *dst = string_alloc(src->len * 4 + 1);
	if (dst == NULL) {
		return NULL;
	}

	uint i;
	for (i = 0; i < src->len; i++) {
		char utf8buf[8];
		uint outlen = uchar_to_utf8(utf8buf, src->buf[i]);
		string_append_mem(dst, utf8buf, outlen);
	}

	return dst;
}

// src を初期化時に指定した出力文字コードに変換して返す。
string *
ustring_to_string(const ustring *src)
{
	string *utf8 = ustring_to_utf8(src);
	if (utf8 == NULL) {
		return NULL;
	}
#if defined(HAVE_ICONV)
	if (use_iconv) {
		string *dst = utf8_to_outcode(utf8);
		string_free(utf8);
		return dst;
	} else
#endif
	{
		return utf8;
	}
}

#if defined(HAVE_ICONV)
// UTF-8 文字列を、初期化時に設定した出力文字コードに変換して返す。
static string *
utf8_to_outcode(const string *utf8)
{
	size_t tmpsize = 2048;
	char *tmpbuf = malloc(tmpsize);
	if (tmpbuf == NULL) {
		return NULL;
	}

	string *dst = string_init();
	if (dst == NULL) {
		goto done;
	}

	// 文字列全体を変換してみる。
	const char *src = string_get(utf8);
	size_t srcleft = string_len(utf8);
	char *tmp = tmpbuf;
	size_t tmpleft = tmpsize;
	while (srcleft) {
		size_t r = ICONV(cd, &src, &srcleft, &tmp, &tmpleft);
		if (r == (size_t)-1) {
			if (errno == EILSEQ) {
				// 変換できない文字の場合、
				// まず代わりにゲタを出力する。
				uint altlen = string_len(altchar);
				if (tmpleft > altlen) {
					strlcpy(tmp, string_get(altchar), tmpleft);
					tmp += altlen;
					tmpleft -= altlen;
				} else {
					// 足りなければ増やして、再度やり直し。
					goto nomem;
				}

				// で、変換できなかった入力の1文字を飛ばす。
				const char *next = src;
				uchar_from_utf8(&next);
				srcleft += (next - src);
				src = next;

			} else if (errno == E2BIG) {
		nomem:
				string_append_mem(dst, tmpbuf, tmpsize - tmpleft);
				tmp = tmpbuf;
				tmpleft = tmpsize;

			} else {
				// それ以外のエラーならどうする?
				break;
			}
		}
	}
	string_append_mem(dst, tmpbuf, tmpsize - tmpleft);

 done:
	free(tmpbuf);
	return dst;
}
#endif

// u のデバッグ用ダンプを表示する。
void
ustring_dump(const ustring *u, const char *head)
{
	for (uint i = 0, len = u->len; i < len; i++) {
		unichar uni = u->buf[i];
		printf("%s[%u] %02x", head, i, uni);

		if (uni == '\t') {
			printf(" \\t");
		} else if (uni == '\r') {
			printf(" \\r");
		} else if (uni == '\n') {
			printf(" \\n");
		} else if (uni < ' ') {
			printf(" \\x%02x", uni);
		} else if (uni < 0x7f) {
			printf(" %c", uni);
		}
		printf("\n");
	}
}

// UTF-8 文字列の *srcp から始まる1文字を Unicode コードポイントにして返す。
// *srcp には次の位置を書き戻す。
static unichar
uchar_from_utf8(const char **srcp)
{
	const uint8 *src = (const uint8 *)*srcp;
	unichar code;
	uint bytelen;
	uint8 c;

	// UTF-8 は1バイト目でこの文字のバイト数が分かる。

	c = *src;
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
		// 来ないはずだけど、とりあえず。
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

	*srcp = (const char *)&src[pos];
	return code;
}

// Unicode コードポイント code を UTF-8 に変換して dst に書き出す。
// dst は '\0' 終端しない。
// 戻り値は書き出したバイト数。
uint
uchar_to_utf8(char *dst, unichar code)
{
	if (code < 0x80) {
		// 1バイト。
		*dst = (char)code;
		return 1;

	} else if (code < 0x7ff) {
		// 2バイト。
		*dst++ = 0xc0 | (code >> 6);
		*dst++ = 0x80 | (code & 0x3f);
		return 2;

	} else if (code < 0x10000) {
		// 3バイト。
		*dst++ = 0xe0 |  (code >> 12);
		*dst++ = 0x80 | ((code >> 6) & 0x3f);
		*dst++ = 0x80 |  (code & 0x3f);
		return 3;

	} else {
		// 4バイト。
		*dst++ = 0xf0 |  (code >> 18);
		*dst++ = 0x80 | ((code >> 12) & 0x3f);
		*dst++ = 0x80 | ((code >>  6) & 0x3f);
		*dst++ = 0x80 |  (code & 0x3f);
		return 4;
	}
}
