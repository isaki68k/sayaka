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
// sayaka, sixelv 共通ヘッダ
//

#ifndef sayaka_common_h
#define sayaka_common_h

#include "header.h"
#include <assert.h>
#include <stdio.h>

// diag.c
#define Debug(diag_, fmt...)	do {	\
	if (diag_get_level(diag_) >= 1)	\
		diag_print(diag_, fmt);	\
} while (0)

#define Trace(diag_, fmt...)	do {	\
	if (diag_get_level(diag_) >= 2)	\
		diag_print(diag_, fmt);	\
} while (0)

#define Verbose(diag_, fmt...)	do {	\
	if (diag_get_level(diag_) >= 3)	\
		diag_print(diag_, fmt);	\
} while (0)

struct diag
{
	int level;
	char name[32];
};

static inline int diag_get_level(const struct diag *diag)
{
	return diag->level;
}
extern struct diag *diag_alloc(void);
extern void diag_free(struct diag *);
extern void diag_set_name(struct diag *, const char *name);
extern void diag_set_level(struct diag *, int level);
extern void diag_print(const struct diag *, const char *fmt, ...)
	__attribute__((format(printf, 2, 3)));

// fdstream.c
extern FILE *fdstream_open(int fd);

// netstream.c
struct netstream_opt {
	// 接続に使用する cipher suites を RSA_WITH_AES_128_CBC_SHA に限定する。
	bool use_rsa_only;
};
extern void netstream_opt_init(struct netstream_opt *);
extern FILE *netstream_open(const char *, const struct netstream_opt *,
	const struct diag *);
extern void netstream_global_cleanup(void);

// string.c
typedef struct string_ string;
extern string *string_init(void);
extern string *string_alloc(uint);
extern bool string_realloc(string *, uint);
extern void string_free(string *);
extern const char *string_get(const string *);
extern uint string_len(const string *);
extern bool string_equal(const string *, const string *);
extern bool string_equal_cstr(const string *, const char *);
extern void string_clear(string *);
extern void string_append_char(string *, char);
extern void string_append_cstr(string *, const char *);

// util.c
extern const char *strerrno(void);

#endif // !sayaka_common_h