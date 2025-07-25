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

#ifndef sayaka_harada_h
#define sayaka_harada_h

#include "common.h"
#include <time.h>

enum {
	BG_AUTO = -1,	// 背景色を自動判別する
	BG_DARK = 0,	// 背景色は暗い
	BG_LIGHT = 1,	// 背景色は明るい
};

enum {
	STYLE_USERNAME,
	STYLE_USERID,
	STYLE_TIME,
	STYLE_RENOTE,
	STYLE_REACTION,
	STYLE_URL,
	STYLE_TAG,
	STYLE_NG,

	STYLE_MAX,
};

enum {
	NSFW_HIDE,		// このノート自体を表示しない
	NSFW_ALT,		// 画像を表示せず MIME type のみ表示する
	NSFW_BLUR,		// ぼかし画像を表示する
	NSFW_SHOW,		// 元画像を表示する
};

typedef uint32 unichar;

struct json;
struct ngwords;

// Unicode 文字列型。common.h の string と揃えること。
struct ustring_ {
	unichar *buf;	// len == 0 の時 buf を触らないこと。
	uint len;		// 文字列の長さ ('\0' の位置)
	uint capacity;	// 確保してあるバイト数
};
typedef struct ustring_ ustring;

// eaw_data.c
extern const uint8 eaw2width_packed[0x8000];

// json.c
extern struct json *json_create(const struct diag *);
extern void json_destroy(struct json *);
extern int  json_parse(struct json *, string *);
extern void json_jsmndump(const struct json *);
extern void json_dump(const struct json *, int);
extern bool json_is_obj(const struct json *, int);
extern bool json_is_array(const struct json *, int);
extern bool json_is_str(const struct json *, int);
extern bool json_is_num(const struct json *, int);
extern bool json_is_bool(const struct json *, int);
extern bool json_is_true(const struct json *, int);
extern bool json_is_null(const struct json *, int);
extern uint json_get_len(const struct json *, int);
extern uint json_get_size(const struct json *, int);
extern const char *json_get_cstr(const struct json *, int);
extern string *json_unescape(const char *);
extern int  json_get_int(const struct json *, int);
extern int  json_obj_first(const struct json *, int, int *, int);
extern int  json_obj_next(const struct json *, int, int);
extern int  json_obj_find(const struct json *, int, const char *);
extern bool json_obj_find_bool(const struct json *, int, const char *);
extern int  json_obj_find_int(const struct json *, int, const char *);
extern int  json_obj_find_obj(const struct json *, int, const char *);
extern const char *json_obj_find_cstr(const struct json *, int, const char *);

#define JSON_FOR(var, js, parentidx, type)							\
	for (int num_, i_ = 0, parent_ = (parentidx),					\
			var = json_obj_first((js), parent_, &num_, (type));		\
		var >= 0 && i_ < num_;										\
		var = json_obj_next((js), var, parent_), i_++)

#define JSON_OBJ_FOR(v, j, p)	JSON_FOR(v, j, p, 1/*JSMN_OBJECT*/)
#define JSON_ARRAY_FOR(v, j, p)	JSON_FOR(v, j, p, 2/*JSMN_ARRAY*/)

// mathalpha.c
extern unichar conv_mathalpha(unichar);

// misskey.c
extern void cmd_misskey_stream(const char *, bool, const char *);
extern void cmd_misskey_play(const char *);

// print.c
extern uint image_count;
extern uint image_next_cols;
extern uint image_max_rows;
extern int  max_image_count;
extern uint indent_depth;
extern uint opt_eaw_a;
extern uint opt_eaw_n;
extern bool opt_mathalpha;
extern bool opt_nocombine;
extern void init_color(void);
extern const char *style_begin(uint);
extern const char *style_end(uint);
extern void ustring_append_ascii_style(ustring *, const char *, uint);
extern void ustring_append_utf8_style(ustring *, const char *, uint);
extern void print_indent(uint);
extern void iprint(const ustring *);
extern bool show_image(const char *, const char *, uint, uint, bool, int);

// sayaka.c
extern const char *cachedir;
extern uint colormode;
extern char colorname[];
extern struct diag *diag_format;
extern struct diag *diag_image;
extern struct diag *diag_json;
extern struct diag *diag_net;
extern struct diag *diag_term;
extern uint fontwidth;
extern uint fontheight;
extern uint iconsize;
extern uint imagesize;
extern uint indent_cols;
extern bool in_sixel;
extern struct net_opt netopt_image;
extern struct net_opt netopt_main;
extern struct ngwords *ngwords;
extern int opt_bgtheme;
extern const char *opt_codeset;
extern uint opt_nsfw;
extern bool opt_overwrite_cache;
extern const char *opt_record_file;
extern bool opt_show_cw;
extern int  opt_show_image;
extern uint screen_cols;

// subr.c
extern uint32 rnd_get32(void);
extern void rnd_fill(void *, uint);
extern uint32 hash_fnv1a(const char *);
extern string *base64_encode(const void *, uint);
extern time_t decode_isotime(const char *);
extern string *format_time(time_t);

// terminal.c
extern uint32 terminal_get_bgcolor(void);
extern int  terminal_support_sixel(void);

// ustring.c
extern bool init_codeset(const char *);
extern ustring *ustring_init(void);
extern ustring *ustring_alloc(uint);
extern bool ustring_realloc(ustring *, uint);
extern void ustring_free(ustring *);
extern ustring *ustring_from_utf8(const char *);
extern const unichar *ustring_get(const ustring *);
extern unichar ustring_at(const ustring *, int);
extern void ustring_append(ustring *, const ustring *);
extern void ustring_append_unichar(ustring *, unichar);
extern void ustring_append_ascii(ustring *, const char *);
extern void ustring_append_utf8(ustring *, const char *);
extern void ustring_tolower_inplace(ustring *);
extern string *ustring_to_utf8(const ustring *);
extern string *ustring_to_string(const ustring *);
extern void ustring_dump(const ustring *, const char *);
extern uint uchar_to_utf8(char *, unichar);
static inline uint ustring_len(const ustring *u) {
	assert(u);
	return u->len;
}
static inline void ustring_clear(ustring *u) {
	assert(u);
	u->len = 0;
}

// wsclient.c
extern struct wsclient *wsclient_create(const struct diag *);
extern void wsclient_destroy(struct wsclient *);
extern void wsclient_init(struct wsclient *, void (*)(const string *));
extern int  wsclient_connect(struct wsclient *, const char *,
	const struct net_opt *);
extern int  wsclient_process(struct wsclient *);
extern ssize_t wsclient_send_text(struct wsclient *, const char *);

#endif // !sayaka_harada_h
