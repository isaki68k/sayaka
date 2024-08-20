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
	// 入力文字列 (の先頭)。
	// 入力時は C の文字列だが、jsmn_parser() で語句に分解した後
	// ここをそのままバッファとして使って、各単語の末尾に '\0' を書き込み
	// ゼロ終端させているため、json_parser() 実行後は単純な C の文字列では
	// なくなっている (strlen(cstr) が出来ないという意味)。
	char *cstr;

	jsmntok_t *token;		// トークンの配列
	uint tokencap;			// 確保してある要素数
	uint tokenlen;			// 使用中の要素数

	jsmn_parser parser;

	const diag *diag;
} json;

static int  json_dump_r(const json *, int, uint, const char *);
static const char *json_get_cstr_prim(const json *, int);
static bool json_equal_cstr(const json *, int, const char *);

static inline bool
tok_is_obj(const jsmntok_t *t)
{
	return (t->type == JSMN_OBJECT);
}

static inline bool
tok_is_array(const jsmntok_t *t)
{
	return (t->type == JSMN_ARRAY);
}

static inline bool
tok_is_str(const jsmntok_t *t)
{
	return (t->type == JSMN_STRING);
}

static inline bool
tok_is_prim(const jsmntok_t *t)
{
	return (t->type == JSMN_PRIMITIVE);
}


// json を生成する。
json *
json_create(const diag *diag)
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

// JSON 文字列 str を語句に分解する。その際 str を書き換える。
// 成功すれば要素数を返す。
// -1 .. -3 は jsmn のエラー。
int
json_parse(json *js, string *str)
{
	int n;

	assert(js);

	js->cstr = string_get_buf(str);
	jsmn_init(&js->parser);

	for (;;) {
		n = jsmn_parse(&js->parser,
				js->cstr, string_len(str),
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

	// STRING と NUMBER をゼロ終端させる。
	// BOOL と NULL をゼロ終端させる必要はそんなにないが、プリミティブのうち
	// NUMBER に限定するほうが手間なのでプリミティブは全部やってしまう。
	for (int i = 0; i < n; i++) {
		jsmntok_t *t = &js->token[i];
		if (tok_is_str(t) || tok_is_prim(t)) {
			js->cstr[t->end] = '\0';
		}
	}

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
		if (tok_is_obj(t)) {
			printf(" OBJECT child=%u", t->size);
		} else if (tok_is_array(t)) {
			printf(" ARRAY child=%u", t->size);
		} else if (tok_is_str(t)) {
			printf(" STRING \"%s\"", &cstr[t->start]);
		} else if (tok_is_prim(t)) {
			char ch = cstr[t->start];
			if (ch == 'n') {
				printf(" NULL");
			} else if (ch == 't') {
				printf(" BOOL true");
			} else if (ch == 'f') {
				printf(" BOOL false");
			} else {
				printf(" NUMBER %s", &cstr[t->start]);
			}
		} else {
			printf(" Undefined??");
		}
		printf("\n");
	}
}

// JSON のダンプを表示する。
void
json_dump(const json *js, int root)
{
	for (int id = root; id < js->tokenlen; ) {
		id = json_dump_r(js, id, 0, "\n");
	}
}

#define INDENT(d)	printf("%*s", (d * 2), "")

// TODO: もうちょっとちゃんとする
static int
json_dump_r(const json *js, int id, uint depth, const char *term)
{
	const char *cstr = js->cstr;
	jsmntok_t *t = &js->token[id];

	if (tok_is_prim(t)) {
		char ch = js->cstr[t->start];
		if (ch == 'n') {
			printf("null");
		} else if (ch == 't') {
			printf("true");
		} else if (ch == 'f') {
			printf("false");
		} else if (ch == '-' || ('0' <= ch && ch <= '9')) {
			printf("%s", &cstr[t->start]);
		}
		printf("%s", term);
		return ++id;
	}
	if (tok_is_str(t)) {
		printf("\"%s\"", &cstr[t->start]);	// XXX TODO エスケープ
		printf("%s", term);
		return ++id;
	}

	if (tok_is_array(t)) {
		uint num = t->size;
		printf("[\n");
		depth++;
		uint n = 0;
		for (id++; id < js->tokenlen && n < num; ) {
			INDENT(depth);
			id = json_dump_r(js, id, depth, (n < num - 1) ? ",\n" : "\n");
			n++;
		}
		depth--;
		INDENT(depth);
		printf("]%s", term);
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
			id = json_dump_r(js, id, depth, ":");
			// val
			id = json_dump_r(js, id, depth, (n < num - 1) ? ",\n" : "\n");
			n++;
		}
		depth--;
		INDENT(depth);
		printf("}%s", term);
		return id;
	}

	printf("?\n");
	return -1;
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

	if (tok_is_prim(t)) {
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

	if (tok_is_prim(t)) {
		char ch = js->cstr[t->start];
		if (ch == 't' || ch == 'f') {
			return true;
		}
	}
	return false;
}

// js[idx] が null 型なら true を返す。
bool
json_is_null(const json *js, int idx)
{
	jsmntok_t *t = &js->token[idx];

	if (tok_is_prim(t) && js->cstr[t->start] == 'n') {
		return true;
	}
	return false;
}

// js[idx] の値の長さを返す。
// STRING 型、NUMBER 型で使う。
// プリミティブ型でも動作はするがそれぞれ固定値が得られるだけで意味はない。
// オブジェクト型、配列型では要素数を返したりはしないので使わないこと。
uint
json_get_len(const json *js, int idx)
{
	jsmntok_t *t = &js->token[idx];

	return t->end - t->start;
}

// js[idx] の値 (無加工の文字列) を返す。
// STRING、NUMBER の他、プリミティブ型でもそのまま文字列を返す。
// そのため STRING の "null" もプリミティブの null もどちらも "null" になる。
static const char *
json_get_cstr_prim(const json *js, int idx)
{
	jsmntok_t *t = &js->token[idx];

	return &js->cstr[t->start];
}

// js[idx] の値 (ただし null 加工済みの文字列) を返す。
// プリミティブの null なら "" を返す。
// STRING と NUMBER 型は文字列を返す。
// プリミティブの true, false に対しては "true", "false" を返すが、
// これは仕様として意図したものではないので使わないこと。
// オブジェクト型、配列型に対しての動作は不定。
const char *
json_get_cstr(const json *js, int idx)
{
	if (json_is_null(js, idx) == false) {
		return json_get_cstr_prim(js, idx);
	} else {
		return "";
	}
}

// js[idx] の値が s2 と一致すれば true を返す。
// 本来 STRING 用だが NUMBER などでも使える。
// プリミティブの null は "" とみなして比較する。
// プリミティブの true, false は "true", "false" との比較になるが
// これは仕様として意図したものではないので使わないこと。
// オブジェクト型、配列型に対しての動作は不定。
static bool
json_equal_cstr(const json *js, int idx, const char *s2)
{
	const char *s1;

	if (json_is_null(js, idx) == false) {
		s1 = json_get_cstr_prim(js, idx);
	} else {
		s1 = "";
	}

	return (strcmp(s1, s2) == 0);
}

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
			if (json_is_str(js, i) && json_equal_cstr(js, i, key)) {
				return i + 1;
			}
			n++;
		}
	}
	return -1;
}
