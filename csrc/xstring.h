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
// string と ustring の共通部分
//

#define XCAT_HELPER(x,y) x##y
#define XCAT(x,y) XCAT_HELPER(x,y)

typedef struct XCAT(xstring,_) {
	xchar *buf;		// len == 0 の時 buf を触らないこと。
	uint len;		// 文字列の長さ ('\0' の位置)
	uint capacity;	// 確保してあるバイト数
} xstring;

// 空の文字列を確保して返す。
xstring *
XCAT(xstring,_init)(void)
{
	xstring *x = calloc(1, sizeof(*x));
	if (x == NULL) {
		return NULL;
	}
	return x;
}

// 初期確保量を指定する。(文字列は空)
xstring *
XCAT(xstring,_alloc)(uint capacity)
{
	xstring *x = XCAT(xstring,_init)();
	if (x == NULL) {
		return NULL;
	}

	XCAT(xstring,_realloc)(x, capacity);
	if (capacity > 0) {
		x->buf[0] = '\0';
	}
	return x;
}

// 確保量を拡大する。文字列は変わらない。
// new_capacity が現行以下なら何もせず true を返す。
// 拡大出来なければ変更せずに false を返す。
bool
XCAT(xstring,_realloc)(xstring *x, uint new_capacity)
{
	assert(x);

	if (new_capacity <= x->capacity) {
		return true;
	}

	xchar *tmp = realloc(x->buf, new_capacity * sizeof(xchar));
	if (tmp == NULL) {
		return false;
	}
	x->buf = tmp;
	x->capacity = new_capacity;
	return true;
}

// x を解放する。
void
XCAT(xstring,_free)(xstring *x)
{
	if (x) {
		free(x->buf);
		free(x);
	}
}

// 文字列を空にする。容量は変わらない。
void
XCAT(xstring,_clear)(xstring *x)
{
	assert(x);
	x->len = 0;
}

// x の文字列の長さを返す。
uint
XCAT(xstring,_len)(const xstring *x)
{
	assert(x);
	return x->len;
}
