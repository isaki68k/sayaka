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
// JSON
//

#include "sayaka.h"
#define JSMN_PARENT_LINKS
#define JSMN_STRICT
#include "jsmn/jsmn.h"
#include <errno.h>
#include <string.h>

#define TOKEN_SIZE_INIT	(500)	// 初期トークン数
#define TOKEN_SIZE_INC	(100)	// 1回の増分

typedef struct json_
{
	const string *str;		// 元文字列 (所有しない)
	const char *cstr;		// 生ポインタ

	jsmntok_t *token;		// トークンの配列
	uint tokencap;			// 確保してある要素数
	uint tokenlen;			// 使用中の要素数

	jsmn_parser parser;

	const struct diag *diag;
} json;

static int  json_dump_r(const json *, int, uint);
static bool tok_is_obj(const jsmntok_t *);
static bool tok_is_array(const jsmntok_t *);
static bool tok_is_str(const jsmntok_t *);
static bool json_str_eq(const json *, int, const char *);
int  json_get_int_def(json *, int, int);


// json を生成する。
json *
json_create(const struct diag *diag)
{
	json *js = calloc(1, sizeof(*js));
	if (js == NULL) {
		return NULL;
	}

	js->diag = diag;

	// 適当なサイズ確保しておく。
	js->tokencap = TOKEN_SIZE_INIT;
	js->token = malloc(js->tokencap * sizeof(jsmntok_t));
	if (js->token == NULL) {
		goto abort;
	}

	return js;

 abort:
	json_destroy(js);
	return NULL;
}

// json を解放する。
void
json_destroy(json *js)
{
	if (js) {
		free(js->token);
		free(js);
	}
}

// JSON 文字列 str を語句に分解する。
// 成功すれば要素数を返す。
// -1 .. -3 は jsmn のエラー。
int
json_parse(json *js, const string *str)
{
	int n;

	assert(js);

	js->str = str;
	js->cstr = string_get(str);
	jsmn_init(&js->parser);

	for (;;) {
		n = jsmn_parse(&js->parser,
				js->cstr, string_len(js->str),
				js->token, js->tokencap);
		if (n != JSMN_ERROR_NOMEM) {
			break;
		}

		// 足りなければトークンを増やす。
		// jsmn_parse() は jsmn_init() しなければ前回の続きから処理する。
		uint newcap = js->tokencap + TOKEN_SIZE_INC;
		void *newbuf = realloc(js->token, newcap * sizeof(jsmntok_t));
		if (newbuf == NULL) {
			Debug(js->diag, "%s: realloc(%zu): %s", __func__,
				newcap * sizeof(jsmntok_t), strerrno());
			return JSMN_ERROR_NOMEM;
		}
		js->token = newbuf;
		js->tokencap = newcap;
	}

	if (n < 0) {
		Debug(js->diag, "%s: jsmn_parse failed: %d", __func__, n);
		return n;
	}

	js->tokenlen = n;
	return n;
}

// jsmn トークンのダンプを表示する。
void
json_jsmndump(const json *js)
{
	const char *cstr = js->cstr;

	for (int i = 0; i < js->tokenlen; i++) {
		jsmntok_t *t = &js->token[i];
		printf("[%4u] s=%-4u e=%-4u p=%-4d", i, t->start, t->end, t->parent);
		if (t->type == JSMN_OBJECT) {
			printf(" OBJECT child=%u", t->size);
		} else if (t->type == JSMN_ARRAY) {
			printf(" ARRAY child=%u", t->size);
		} else if (t->type == JSMN_STRING) {
			printf(" STRING \"%.*s\"", t->end - t->start, &cstr[t->start]);
		} else if (t->type == JSMN_UNDEFINED) {
			printf(" Undefined??");
		} else {
			char ch = cstr[t->start];
			if (ch == 'n') {
				printf(" NULL");
			} else if (ch == 't') {
				printf(" BOOL true");
			} else if (ch == 'f') {
				printf(" BOOL false");
			} else {
				printf(" NUMBER %.*s", t->end - t->start, &cstr[t->start]);
			}
		}
		printf("\n");
	}
}

// JSON のダンプを表示する。
void
json_dump(const json *js, int root)
{
	for (int id = root; id < js->tokenlen; ) {
		id = json_dump_r(js, id, 0);
	}
	printf("\n");
}

#define INDENT(d)	printf("%*s", (d * 2), "")

// TODO: もうちょっとちゃんとする
static int
json_dump_r(const json *js, int id, uint depth)
{
	jsmntok_t *t = &js->token[id];

	if (t->type == JSMN_PRIMITIVE) {
		char ch = js->cstr[t->start];
		if (ch == 'n') {
			printf("null");
		} else if (ch == 't') {
			printf("true");
		} else if (ch == 'f') {
			printf("false");
		} else if (ch == '-' || ('0' <= ch && ch <= '9')) {
			char buf[t->end - t->start + 1];
			json_get_str_buf(js, id, buf, sizeof(buf));
			printf("%s", buf);
		}
		return ++id;
	}
	if (tok_is_str(t)) {
		char buf[t->end - t->start + 1];
		json_get_str_buf(js, id, buf, sizeof(buf));
		printf("\"%s\"", buf);	// XXX TODO エスケープ
		return ++id;
	}

	if (tok_is_array(t)) {
		uint num = t->size;
		printf("[\n");
		depth++;
		uint n = 0;
		for (id++; id < js->tokenlen && n < num; ) {
			INDENT(depth);
			id = json_dump_r(js, id, depth);
			n++;
			printf("\n");
		}
		depth--;
		INDENT(depth);
		printf("]\n");
		return id;
	}

	if (tok_is_obj(t)) {
		uint num = t->size;
		printf("{\n");
		depth++;
		uint n = 0;
		for (id++; id < js->tokenlen && n < num; ) {
			INDENT(depth);

			// key
			id = json_dump_r(js, id, depth);
			printf(": ");
			// val
			id = json_dump_r(js, id, depth);
			n++;
			printf("\n");
		}
		depth--;
		INDENT(depth);
		printf("}\n");
		return id;
	}

	printf("?\n");
	return -1;
}

static bool
tok_is_obj(const jsmntok_t *t)
{
	return (t->type == JSMN_OBJECT);
}

static bool
tok_is_array(const jsmntok_t *t)
{
	return (t->type == JSMN_ARRAY);
}

static bool
tok_is_str(const jsmntok_t *t)
{
	return (t->type == JSMN_STRING);
}

// js[idx] がオブジェクト { .. } なら true を返す。
bool
json_is_obj(const json *js, int idx)
{
	return tok_is_obj(&js->token[idx]);
}

// js[idx] が配列 [ .. ] なら true を返す。
bool
json_is_array(const json *js, int idx)
{
	return tok_is_array(&js->token[idx]);
}

// js[idx] が文字列なら true を返す。
bool
json_is_str(const json *js, int idx)
{
	return tok_is_str(&js->token[idx]);
}

// js[idx] が数値型なら true を返す。
bool
json_is_num(const json *js, int idx)
{
	jsmntok_t *t = &js->token[idx];

	if (t->type == JSMN_PRIMITIVE) {
		char ch = js->cstr[t->start];
		if (('0' <= ch && ch <= '9') || ch == '-') {
			return true;
		}
	}
	return false;
}

// js[idx] がブール型なら true を返す。
bool
json_is_bool(const json *js, int idx)
{
	jsmntok_t *t = &js->token[idx];

	if (t->type == JSMN_PRIMITIVE) {
		char ch = js->cstr[t->start];
		if (ch == 't' || ch == 'f') {
			return true;
		}
	}
	return false;
}

// js[idx] (STRING 型) が s2 と一致すれば true を返す。
static bool
json_str_eq(const json *js, int idx, const char *s2)
{
	jsmntok_t *t = &js->token[idx];

	if (__predict_true(tok_is_str(t))) {
		const char *s1 = &js->cstr[t->start];
		uint s1len = t->end - t->start;
		uint s2len = strlen(s2);
		if (s1len == s2len && strncmp(s1, s2, s1len) == 0) {
			return true;
		}
	}
	return false;
}

// js[idx] の値 [start..end) を dst にコピーする。
// js[idx] の型が適切なことは呼び出し前に確認しておくこと。
// 本来文字列をバッファに取り出すためだが NUMBER や BOOL などでも使える。
// dst に格納しきれなければ、dstsize を超える前に '\0' で終端し false を返す。
bool
json_get_str_buf(const json *js, int idx, char *dst, size_t dstsize)
{
	assert(idx < js->tokenlen);
	jsmntok_t *t = &js->token[idx];

	const char *src = &js->cstr[t->start];
	uint len = t->end - t->start;
	if (__predict_true(len < dstsize)) {
		memcpy(dst, src, len);
		dst[len] = '\0';
		return true;
	} else {
		strlcpy(dst, src, dstsize);
		return false;
	}
}

#if 0
// js[idx] の要素を string を作成して返す。
// 文字列でなければ defval を返す。
string *
json_get_str_def(json *js, int idx, const char *defval)
{
	assert(idx < js->tokenlen);

	string *retval;
	jsmntok_t *t = &js->token[idx];
	if (tok_is_str(t)) {
		uint len = t->end - t->start;
		retval = string_alloc(len + 1);
		char *d = string_get_buf(retval);
		memcpy(d, string_get(js->str) + t->start, len);
		d[len] = '\0';
	} else {
		if (defval == NULL) {
			retval = NULL;
		} else {
			retval = string_dup_cstr(defval);
		}
	}

	return retval;
}

// js[idx] の要素を int32 として値を返す。
// 小数点以下は無視する。
// int に収まらなければ errno に ERANGE をセットして defval を返す。
int
json_get_int_def(json *js, int idx, int defval)
{
	assert(idx < js->tokenlen);

	jsmntok_t *t = &js->token[idx];
	if (t->type == JSMN_PRIMITIVE) {
		const char *s = string_get(js->str);
		char ch = s[t->start];
		if (('0' <= ch && ch <= '9') || ch == '-') {
			char *end;
			long val;

			errno = 0;
			val = strtol(s, &end, 10);
			if (errno == ERANGE) {
				return defval;
			}
			if (val < INT32_MIN || val > INT32_MAX) {
				errno = ERANGE;
				return defval;
			}
			return (int)val;
		}
	}
	return defval;
}
#endif

// オブジェクト型である idx からキーが key である要素を探す。
// 返されるのは key に対応する値のインデックス。
// 見付からなければ -1 を返す。
int
json_obj_find(const json *js, int idx, const char *key)
{
	jsmntok_t *t = &js->token[idx];

	// 自分がオブジェクトでなければ -1 が返ってきてもいい気がする。
	if (__predict_false(tok_is_obj(t) == false)) {
		return -1;
	}

	// 自分の次の要素から、size 個カウントするまで、自分を親に持つ要素を探す。
	int i = idx + 1;
	uint childnum = t->size;
	uint n = 0;
	for (; i < js->tokenlen && n < childnum; i++) {
		if (js->token[i].parent == idx) {
			if (json_str_eq(js, i, key)) {
				return i + 1;
			}
			n++;
		}
	}
	return -1;
}
