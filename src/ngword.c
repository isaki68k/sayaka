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

#include "sayaka.h"
#include "ngword.h"
#include <err.h>
#include <errno.h>
#include <string.h>

// NG ワードファイルを読み込んで内部構造にして返す。
// エラーなら NULL を返す。
// ファイルがない、ファイルが空、JSON の配列が空、は 0 個のエントリで正常。
struct ngwords *
ngword_read_file(const char *filename, const struct diag *diag)
{
	struct json *js;
	string *filebody;
	struct ngwords *dict;
	int r;
	uint ncount;
	uint srcidx;
	uint i;

	js = NULL;
	filebody = NULL;
	dict = NULL;

	// ファイルがないのは構わないが、それ以外はエラー。
	FILE *fp = fopen(filename, "r");
	if (fp == NULL) {
		if (errno == ENOENT) {
			goto empty;
		}
		warn("%s", filename);
		return NULL;
	}

	filebody = string_fgets(fp);
	fclose(fp);
	if (filebody == NULL) {
		// ファイルが空でも構わない。
		goto empty;
	}

	// JSON を作成。
	js = json_create(diag);
	if (js == NULL) {
		warnx("%s: json_create failed", __func__);
		goto done;
	}

	// JSON パース。
	r = json_parse(js, filebody);
	if (r < 0) {
		warnx("%s: json_parse failed: %d", __func__, r);
		goto done;
	}

	if (0) {
		json_jsmndump(js);
	}

	// ファイルは以下のような構造。
	// [
	//   {
	//      "type":"text",
	//      "text":"Foo",
	//      "user":"@user",
	//   },
	//   { ...
	//   },
	// ]

	// トップは配列。
	if (json_is_array(js, 0) == false) {
		warnx("%s: Invalid JSON format (top array not found)", filename);
		goto done;
	}

	// 要素数。
	ncount = json_get_size(js, 0);
	if (ncount == 0) {
		goto empty;
	}

	// 要素数が決まったのでここでメモリを確保。
	dict = malloc(sizeof(struct ngwords) + sizeof(struct ngword) * ncount);
	if (dict == NULL) {
		warnx("%s: malloc failed", __func__);
		goto done;
	}
	dict->count = ncount;
	memset(&dict->item[0], 0, sizeof(struct ngword) * ncount);

	// エントリを読み込む。
	i = 0;
	srcidx = 0;
	JSON_ARRAY_FOR(aidx, js, 0) {
		string *text;
		string *user;

		srcidx++;

		// type を取り出す。なければこのエントリを捨てる。
		int type;
		const char *typestr = json_obj_find_cstr(js, aidx, "type");
		if (typestr == NULL) {
			warnx("%s[%u]: type not found", filename, srcidx);
			continue;
		} else if (strcmp(typestr, "text") == 0) {
			type = NG_TEXT;
		} else if (strcmp(typestr, "regex") == 0) {
			type = NG_REGEX;
		} else {
			warnx("%s[%u]: Unknown type: %s", filename, srcidx, typestr);
			continue;
		}

		// text を取り出す。なければ空文字列?
		const char *rawtext = json_obj_find_cstr(js, aidx, "text");
		text = rawtext ? json_unescape(rawtext) : string_init();

		// user を取り出す。ない、あるいは空文字列なら NULL。
		const char *rawuser = json_obj_find_cstr(js, aidx, "user");
		if (rawuser == NULL || rawuser[0] == '\0') {
			user = NULL;
		} else {
			user = json_unescape(rawuser);
		}

		struct ngword *ng = &dict->item[i];

		// type ごとの処理。
		switch (type) {
		 case NG_REGEX:
		 {
			// コンパイル出来ない要素があれば停止するか?
			const char *ctext = string_get(text);
			r = regcomp(&ng->ng_regex, ctext, REG_EXTENDED);
			if (r != 0) {
				warnx("%s[%u]: \"%s\": Regex compilation failed",
					filename, srcidx, ctext);
				string_free(text);
				string_free(user);
				goto error;
			}
			break;
		 }
		 default:
			break;
		}

		// 確保した string をそのまま引き継ぐ。
		ng->ng_text = text;
		ng->ng_user = user;
		ng->ng_type = type;

		if (0) {
			printf("[%u] ", i);
			switch (ng->ng_type) {
			 case NG_TEXT:
				printf("text=\"%s\"", string_get(ng->ng_text));
				break;
			 case NG_REGEX:
				printf("regex=\"%s\"", string_get(ng->ng_text));
				break;
			 case NG_NONE:
				printf("type=none");
			 default:
				printf("type=%d?", (int)ng->ng_type);
				break;
			}
			if (ng->ng_user) {
				printf(" user=\"%s\"", string_get(ng->ng_user));
			} else {
				printf(" user=all");
			}
			printf("\n");
		}

		i++;
	}

 done:
	json_destroy(js);
	string_free(filebody);
	return dict;

 error:
	// エラー終了する。
	free(dict);
	dict = NULL;
	goto done;

 empty:
	// 空配列を作成して返す。
	assert(dict == NULL);
	dict = calloc(sizeof(struct ngwords), 1);
	if (dict) {
		dict->count = 0;
	}
	goto done;
}

// dict があれば解放する。
void
ngword_destroy(struct ngwords *dict)
{
	if (dict) {
		for (uint i = 0; i < dict->count; i++) {
			struct ngword *ng = &dict->item[i];
			string_free(ng->ng_text);
			regfree(&ng->ng_regex);
			string_free(ng->ng_user);
		}
		free(dict);
	}
}
