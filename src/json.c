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

	const struct diag *diag;
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
		printf("[%4d] s=%-4d e=%-4d p=%-4d", i, t->start, t->end, t->parent);
		if (tok_is_obj(t)) {
			printf(" OBJECT child=%d", t->size);
		} else if (tok_is_array(t)) {
			printf(" ARRAY child=%d", t->size);
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

// js[idx] がブール型で true なら true を返す。
bool
json_is_true(const json *js, int idx)
{
	jsmntok_t *t = &js->token[idx];

	if (tok_is_prim(t)) {
		char ch = js->cstr[t->start];
		if (ch == 't') {
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

// js[idx] の子要素数 (オブジェクトならペア数、配列なら要素数) を返す。
// オブジェクト型、配列型で使う。他での動作は不定。
uint
json_get_size(const json *js, int idx)
{
	jsmntok_t *t = &js->token[idx];

	return t->size;
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

// JSON の文字列のエスケープを取り除く。
string *
json_unescape(const char *src)
{
	// 最長で元文字列と同じ長さのはず?
	string *dst = string_alloc(strlen(src) + 1);
	if (dst == NULL) {
		return NULL;
	}

	char c;
	bool escape = false;
	for (int i = 0; (c = src[i]) != '\0'; i++) {
		if (escape == false) {
			if (c == '\\') {
				escape = true;
			} else {
				string_append_char(dst, c);
			}
		} else {
			switch (c) {
			 case '\"':
				string_append_char(dst, '"');
				break;
			 case '\\':
				string_append_char(dst, '\\');
				break;
			 case '/':
				string_append_char(dst, '/');
				break;
			 case 'b':
				string_append_char(dst, '\b');
				break;
			 case 'f':
				string_append_char(dst, '\f');
				break;
			 case 'n':
				string_append_char(dst, '\n');
				break;
			 case 'r':
				string_append_char(dst, '\r');
				break;
			 case 't':
				string_append_char(dst, '\t');
				break;
			 case 'u':	// \uXXXX
			 {
				char hex[5];
				uint j;
				// 4文字コピーして一旦ゼロ終端文字列を作る。
				// "\u01234" は '\u+0123' と '4' という2文字なので
				// 元文字列中で任意長の16進数文字列を変換する方法は使えない。
				memset(hex, 0, sizeof(hex));
				for (j = 0; j < 4; j++) {
					hex[j] = src[i + 1 + j];
					if (hex[j] == '\0') {
						goto default_case;
					}
				}
				i += j;

				char *end;
				uint32 code = stox32def(hex, -1, &end);
				if ((int32)code < 0 || (end - hex) != 4) {
					goto default_case;
				}
				char utf8[8];
				uint ulen = uchar_to_utf8(utf8, code);
				string_append_mem(dst, utf8, ulen);
				break;
			 }
			 default:
			 default_case:
				string_append_char(dst, '\\');
				string_append_char(dst, c);
				break;
			}
			escape = false;
		}
	}

	return dst;
}

// js[idx] の値を int で返す。
// NUMBER 型でないか、int で表現できない場合は 0 を返す。
// 小数点以下は切り捨てて整数にする。
int
json_get_int(const json *js, int idx)
{
	int val = 0;
	if (json_is_num(js, idx)) {
		const char *s = json_get_cstr(js, idx);
		val = stou32def(s, 0, NULL);
	}
	return val;
}

// オブジェクトまたは配列型である idx の先頭の(キーの)インデックスを返す。
// *nump には要素数を返す。
// type には JSMN_* 型を指定すること。
// オブジェクトまたは配列が空なら戻り値 -1 を返す。
// JSON_OBJ_FOR マクロで使用する。
int
json_obj_first(const json *js, int idx, int *nump, int type)
{
	if (__predict_true(js->token[idx].type == type)) {
		int num = json_get_size(js, idx);
		if (num > 0) {
			*nump = num;
			return idx + 1;
		}
	}
	return -1;
}

// keyidx の次のキーのインデックスを返す。
// 個数は呼び出し元が管理しているので見付からないはずはないが、
// 見付からなければ -1 を返す。
// JSON_OBJ_FOR, JSON_ARRAY_FOR マクロで使用する。
int
json_obj_next(const json *js, int keyidx, int parentidx)
{
	// オブジェクトに限定するなら keyidx + 2 が最短の次のキー位置だが、
	// +1 にしておくと配列でも同じ関数が使い回せる。
	for (int i = keyidx + 1; i < js->tokenlen; i++) {
		if (js->token[i].parent == parentidx) {
			return i;
		}
	}
	return -1;
}

// オブジェクト型である idx からキーが target である要素を探す。
// 返されるのは key に対応する値のインデックス。
// 見付からなければ -1 を返す。
int
json_obj_find(const json *js, int idx, const char *target)
{
	JSON_OBJ_FOR(ikey, js, idx) {
		if (json_is_str(js, ikey) && json_equal_cstr(js, ikey, target)) {
			return ikey + 1;
		}
	}

	return -1;
}

// オブジェクト型である idx から key に対応する BOOL の値を返す。
// key が見付からないか値が BOOL 型でなければ false を返す。
bool
json_obj_find_bool(const json *js, int idx, const char *key)
{
	int validx = json_obj_find(js, idx, key);
	if (validx >= 0) {
		return json_is_true(js, validx);
	}
	return false;
}

// オブジェクト型である idx から key に対応する数値の値を int で返す。
// key が見付からないか値が数値型でないか int で表現できなければ 0 を返す。
int
json_obj_find_int(const json *js, int idx, const char *key)
{
	int validx = json_obj_find(js, idx, key);
	if (validx >= 0) {
		return json_get_int(js, validx);
	}
	return 0;
}

// オブジェクト型である idx からキーが key である子オブジェクトの
// インデックスを返す。キーが見付からないかオブジェクトでなければ -1 を返す。
int
json_obj_find_obj(const json *js, int idx, const char *key)
{
	int validx = json_obj_find(js, idx, key);
	if (validx >= 0 && __predict_true(json_is_obj(js, validx))) {
		return validx;
	}
	return -1;
}

// オブジェクト型である idx から key に対応する文字列の値を返す。
// key が見付からないか値が文字列でなければ NULL を返す。
const char *
json_obj_find_cstr(const json *js, int idx, const char *key)
{
	int validx = json_obj_find(js, idx, key);
	if (validx >= 0 && __predict_true(json_is_str(js, validx))) {
		return json_get_cstr(js, validx);
	}
	return NULL;
}
