/* vi:set ts=4: */
/*
 * Copyright (C) 2014-2024 Tetsuya Isaki
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

enum {
	BG_AUTO = -1,	// 背景色を自動判別する
	BG_DARK = 0,	// 背景色は暗い
	BG_LIGHT = 1,	// 背景色は明るい
};

// json.c
typedef struct json_ json;
extern json *json_create(const diag *);
extern void json_destroy(json *);
extern int  json_parse(json *, string *);
extern void json_jsmndump(const json *);
extern void json_dump(const json *, int);
extern bool json_is_obj(const json *, int);
extern bool json_is_array(const json *, int);
extern bool json_is_str(const json *, int);
extern bool json_is_num(const json *, int);
extern bool json_is_bool(const json *, int);
extern bool json_is_null(const json *, int);
extern uint json_get_len(const json *, int);
extern const char *json_get_cstr(const json *, int);
extern int  json_obj_find(const json *, int, const char *);

// misskey.c
extern void cmd_misskey_stream(const char *);
extern void cmd_misskey_play(const char *);

// sayaka.c
extern diag *diag_json;
extern diag *diag_net;
extern diag *diag_term;

// terminal.c
extern int  terminal_get_bgtheme(void);
extern bool terminal_support_sixel(void);

// wsclient.c
struct wsclient;
extern struct wsclient *wsclient_create(const diag *);
extern void wsclient_destroy(struct wsclient *);
extern bool wsclient_init(struct wsclient *, void (*)(const string *));
extern int  wsclient_connect(struct wsclient *, const char *);
extern int  wsclient_process(struct wsclient *);
extern ssize_t wsclient_send_text(struct wsclient *, const char *);

#endif // !sayaka_harada_h
