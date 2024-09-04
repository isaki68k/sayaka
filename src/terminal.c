/* vi:set ts=4: */
/*
 * Copyright (C) 2015-2024 Tetsuya Isaki
 * Copyright (C) 2015 Y.Sugahara (moveccr)
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
// 端末周り
//

#include "sayaka.h"
#include <errno.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/param.h>
#include <sys/select.h>
#if defined(HAVE_SYS_SYSCTL_H)
#include <sys/sysctl.h>
#endif

// ざっくり遅マシン判定
#if defined(__hppa__)	|| \
    defined(__m68k__)	|| \
    defined(__sh3__)	|| \
    (defined(__sparc__) && !defined(__sparc64__))	|| \
    defined(__vax__)
#define TIMEOUT (10 * 1000 * 1000)	// [usec]
#else
#define TIMEOUT       (500 * 1000)	// [usec]
#endif

static int terminal_query(const char *, char *, uint);
static void terminal_dump(char *, const char *, uint);
static int parse_bgcolor(char *);

// 端末が SIXEL をサポートしていれば 1 を返す。
// 出力先が端末であること (isatty(3)) は呼び出し側で調べておくこと。
int
terminal_support_sixel(void)
{
	char result[64];
	char *p;
	char *e;
	int n;
	int support = 0;

	// 問い合わせ。
	const char *query = ESC "[c";
	n = terminal_query(query, result, sizeof(result));
	if (n < 1) {
		goto done;
	}

	// ケーパビリティを調べる。応答は
	// ESC "[?63;1;2;3;4;7;29c" のような感じで "4" があれば SIXEL 対応。
	for (p = result; p; p = e) {
		e = strchr(p, ';');
		if (e) {
			*e++ = '\0';
		} else {
			e = strrchr(p, 'c');
			if (e) {
				*e = '\0';
				e = NULL;
			}
		}

		if (strcmp(p, "4") == 0) {
			support = 1;
		}
	}

 done:
	Debug(diag_term, "%s: %s", __func__, (support == 0 ? "false" : "true"));
	return support;
}

// 端末の背景色を調べる。
// 黒に近ければ 1、白に近ければ 0、取得できなければ -1 を返す。
// 出力先が端末であること (isatty(3)) は呼び出し側で調べておくこと。
int
terminal_get_bgtheme(void)
{
	char result[64];
	int n;
	int bgcolor = -1;

	// 問い合わせ。
	const char *query = ESC "]11;?" ESC "\\";
	n = terminal_query(query, result, sizeof(result));
	if (n < 1) {
		goto done;
	}

	bgcolor = parse_bgcolor(result);
 done:
	Debug(diag_term, "%s: %s", __func__,
		(bgcolor == -1 ? "terminal doesn't support the query." :
		(bgcolor == 0 ? "looks dark" :
		(bgcolor == 1 ? "looks light" : "?"))));
	return bgcolor;
}

// 端末の背景色応答行から、背景色を調べる。
// 黒に近ければ 0、白に近ければ 1、取得できなければ -1 を返す。
// result は破壊する。
static int
parse_bgcolor(char *result)
{
	int ri, gi, bi;
	int rn, gn, bn;
	char *p;
	char *e;

	// 背景色を調べる。result は
	// <ESC> "]11;rgb:RRRR/GGGG/BBBB" <ESC> "\" で、
	// RRRR,GGGG,BBBB は各 0000-ffff の 65535 階調らしい。
	// ただし RR/GG/BB の2桁を返す実装があるらしい。
	// OpenBSD は末尾の <ESC> "\" の代わりに BEL('\7') を返すらしい。

	// "]11;rgb:…" のところを "]0;rgb:…" を返す実装もあるらしいので
	// "rgb:" だけで調べる。
	p = strstr(result, "rgb:");
	if (p == NULL) {
		return -1;
	}
	// R
	p += 4;
	ri = stox32def(p, -1, &e);
	if (ri < 0 || *e != '/') {
		return -1;
	}
	rn = e - p;

	// G
	p = e + 1;
	gi = stox32def(p, -1, &e);
	if (gi < 0 || *e != '/') {
		return -1;
	}
	gn = e - p;

	// B
	p = e + 1;
	bi = stox32def(p, -1, &e);
	if (bi < 0) {
		return -1;
	}
	bn = e - p;

	float r = (float)ri / (1U << (rn * 4));
	float g = (float)gi / (1U << (gn * 4));
	float b = (float)bi / (1U << (bn * 4));
	// グレースケールで、黒に近ければ 0、白に近ければ 1 を返す。
	// 厳密に言えばどの式を使うかとかガンマ補正とかいろいろあるけど、
	// ここは前景色と背景色が常用に耐えるレベルで明るさに違いがあるはずで、
	// その背景色の明暗だけ分かればいいはずなので、細かいことは気にしない。
	float I = (0.2126 * r) + (0.7152 * g) + (0.0722 * b);
	// 四捨五入。
	return (int)(I + 0.5);
}

// 端末に問い合わせて応答を受け取る。
// 成功すれば応答の文字数を返す。
// 失敗すれば errno をセットして -1 を返す。
static int
terminal_query(const char *query, char *dst, uint dstsize)
{
	char dumpbuf[64];
	struct timeval timeout;
	struct termios tc;
	struct termios old;
	fd_set rfds;
	int r;

	if (diag_get_level(diag_term) >= 2) {
		terminal_dump(dumpbuf, query, sizeof(dumpbuf));
		diag_print(diag_term, "%s: query  |%s|", __func__, dumpbuf);
	}

	// select(2) の用意。
	FD_ZERO(&rfds);
	FD_SET(STDOUT_FILENO, &rfds);
	timeout.tv_sec  = TIMEOUT / (1000 * 1000);
	timeout.tv_usec = TIMEOUT % (1000 * 1000);

	// 応答受け取るため非カノニカルモードにするのと
	// その応答を画面に表示してしまわないようにエコーオフにする。
	tcgetattr(STDOUT_FILENO, &tc);
	old = tc;
	tc.c_lflag &= ~(ECHO | ICANON);
	tcsetattr(STDOUT_FILENO, TCSANOW, &tc);

	// 問い合わせ。
	r = write(STDOUT_FILENO, query, strlen(query));
	if (__predict_false(r < 0)) {
		goto done;
	}

	// 念のため応答がなければタイムアウトするようにしておく。
	r = select(STDOUT_FILENO + 1, &rfds, NULL, NULL, &timeout);
	if (__predict_false(r <= 0)) {
		goto done;
	}

	r = read(STDOUT_FILENO, dst, dstsize);
	if (__predict_false(r < 0)) {
		goto done;
	}
	dst[r] = '\0';

	// 端末を元に戻して r を持って帰る。
 done:
	tcsetattr(STDOUT_FILENO, TCSANOW, &old);

	if (diag_get_level(diag_term) >= 2) {
		if (r > 0) {
			terminal_dump(dumpbuf, dst, sizeof(dumpbuf));
			diag_print(diag_term, "%s: result |%s|", __func__, dumpbuf);
		} else if (r == 0) {
			diag_print(diag_term, "%s: timeout", __func__);
		} else {
			diag_print(diag_term, "%s: r=%d", __func__, r);
		}
	}
	return r;
}

// デバッグ表示用に src をエスケープした文字列を dst に返す。
static void
terminal_dump(char *dst, const char *src, uint dstsize)
{
	string *s = string_alloc(128);

	for (int i = 0; src[i]; i++) {
		if (src[i] == ESCchar) {
			string_append_cstr(s, "<ESC>");
		} else if (0x20 <= src[i] && src[i] < 0x7f) {
			string_append_char(s, src[i]);
		} else {
			char buf[8];
			snprintf(buf, sizeof(buf), "\\x%02x", (unsigned char)src[i]);
			string_append_cstr(s, buf);
		}
	}

	strlcpy(dst, string_get(s), dstsize);
	string_free(s);
}

#if defined(TEST)
#include <err.h>

diag *diag_term;

int
main(int ac, char *av[])
{
	diag_term = diag_alloc();
	diag_set_level(diag_term, 2);

	if (ac > 1) {
		if (strcmp(av[1], "sixel") == 0) {
			terminal_support_sixel();
			exit(0);
		}
		if (strcmp(av[1], "bg") == 0) {
			terminal_get_bgtheme();
			exit(0);
		}
	}
	errx(1, "usage: <sixel | bg>");
}

#endif // TEST
