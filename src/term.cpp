/*
 * Copyright (C) 2015-2021 Tetsuya Isaki
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

#include "sayaka.h"
#include "term.h"
#include <cstdio>
#include <string>
#include <err.h>
#include <errno.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/param.h>
#include <sys/select.h>
#include <sys/sysctl.h>

// ざっくり遅マシン判定
#if defined(__hppa__)	|| \
    defined(__m68k__)	|| \
    defined(__sh3__)	|| \
    (defined(__sparc__) && !defined(__sparc64__))	|| \
    defined(__vax__)
#define SLOW_MACHINES
#endif

static int query_terminal(const std::string& query, char *dst, size_t dstsize);

#if defined(TEST)
static std::string
dump(const char *src)
{
	std::string r;

	for (int i = 0; src[i]; i++) {
		if (src[i] == ESCchar) {
			r += "<ESC>";
		} else {
			r.push_back(src[i]);
		}
	}
	return r;
}
#endif

// 端末が SIXEL をサポートしていれば true を返す
bool
terminal_support_sixel()
{
	std::string query;
	char result[128];
	int n;

	// 出力先が端末でない(パイプとか)なら帰る。
	if (isatty(STDOUT_FILENO) == 0) {
		return false;
	}

	// 問い合わせる。
	query = ESC "[c";
	n = query_terminal(query, result, sizeof(result));
	if (n < 0) {
		warn("%s query_terminal failed", __func__);
		return false;
	}
	if (n == 0) {
		warnx("%s: timeout", __func__);
		return false;
	}

	// ケーパビリティを調べる。応答は
	// ESC "[?63;1;2;3;4;7;29c" のような感じで "4" があれば SIXEL 対応。
	char *p;
	char *e;
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
			return true;
		}
	}

	return false;
}

// 端末の背景色を調べる。
// 黒に近ければ BG_BLACK、白に近ければ BG_WHITE、取得できなければ BG_NONE を
// 返す。
enum bgcolor
terminal_bgcolor()
{
	std::string query;
	char result[128];
	int n;
	int ri, gi, bi;

	// 出力先が端末でない(パイプとか)なら帰る。
	if (isatty(STDOUT_FILENO) == 0) {
		return BG_NONE;
	}

	// 問い合わせる
	query = ESC "]11;?" ESC "\\";
	n = query_terminal(query, result, sizeof(result));
	if (n < 0) {
		warn("%s: query_terminal failed", __func__);
		return BG_NONE;
	}
	if (n == 0) {
		warnx("%s: timeout", __func__);
		return BG_NONE;
	}

	// 背景色を調べる。応答は
	// <ESC> "]11;rgb:0000/0000/0000" <ESC> "\" で、
	// 色は 0000-ffff の 65535 階調らしい。
	char *p;
	char *e;
	p = strstr(result, "rgb:");
	if (p == NULL) {
		return BG_NONE;
	}
	// R
	p += 4;
	errno = 0;
	ri = strtol(p, &e, 16);
	if (p == e || *e != '/' || errno == ERANGE) {
		return BG_NONE;
	}
	// G
	p = e + 1;
	errno = 0;
	gi = strtol(p, &e, 16);
	if (p == e || *e != '/' || errno == ERANGE) {
		return BG_NONE;
	}
	// B
	p = e + 1;
	errno = 0;
	bi = strtol(p, &e, 16);
	if (p == e || *e != ESCchar || errno == ERANGE) {
		return BG_NONE;
	}

	float r = (float)ri / 65536;
	float g = (float)gi / 65536;
	float b = (float)bi / 65536;
	// グレースケールで、黒に近ければ 0、白に近ければ 1 を返す。
	// 厳密に言えばどの式を使うかとかガンマ補正とかいろいろあるけど、
	// ここは前景色と背景色が常用に耐えるレベルで明るさに違いがあるはずで、
	// その背景色の明暗だけ分かればいいはずなので、細かいことは気にしない。
	float I = (0.2126 * r) + (0.7152 * g) + (0.0722 * b);
	// 四捨五入。
	// 白が 1、黒が 0 だと知っている。
	return (enum bgcolor)(int)(I + 0.5);
}

// ファイルディスクリプタ fd から dstbuf に読み込む。
// 成功すれば、読み出した内容を dst に '\0' 終端で書き戻し、読み出した
// バイト数を返す。
// タイムアウトすれば 0 を返す。
// エラーなら -1 を返す (今の所 select か read かの区別はつかない)。
static int
query_terminal(const std::string& query, char *dst, size_t dstsize)
{
	struct timeval timeout;
	struct termios tc;
	struct termios old;
	fd_set rfds;
	int r;

	// select(2) の用意
	FD_ZERO(&rfds);
	FD_SET(STDOUT_FILENO, &rfds);
	memset(&timeout, 0, sizeof(timeout));
#if defined(SLOW_MACHINES)
	timeout.tv_sec = 10;
#else
	timeout.tv_usec = 500 * 1000;
#endif

	// 応答受け取るため非カノニカルモードにするのと
	// その応答を画面に表示してしまわないようにエコーオフにする。
	tcgetattr(STDOUT_FILENO, &tc);
	old = tc;
	tc.c_lflag &= ~(ECHO | ICANON);
	tcsetattr(STDOUT_FILENO, TCSANOW, &tc);

	// 問い合わせ
	r = write(STDOUT_FILENO, query.c_str(), query.size());

	// 念のため応答がなければタイムアウトするようにしておく
	r = select(STDOUT_FILENO + 1, &rfds, NULL, NULL, &timeout);
	if (__predict_false(r <= 0)) {
		goto done;
	}

	r = read(STDOUT_FILENO, dst, dstsize);
	if (__predict_false(r < 0)) {
		goto done;
	}
	dst[r] = '\0';

#if defined(TEST)
	// デバッグ表示
	if (0) {
		fprintf(stderr, "dst=\"%s\"\n", dump(dst).c_str());
	}
#endif

	// 端末を元に戻して r を持って帰る
 done:
	tcsetattr(STDOUT_FILENO, TCSANOW, &old);
	return r;
}

#if defined(TEST)

int
test_sixel()
{
	bool r = terminal_support_sixel();
	if (r) {
		printf("terminal supports sixel\n");
	} else {
		printf("terminal does not support sixel\n");
	}
	return 0;
}

int
test_bg()
{
	int r = terminal_bgcolor();
	if (r >= 0) {
		printf("terminal bgcolor = %d\n", r);
	} else {
		printf("terminal does not support the query\n");
	}
	return 0;
}

int
main(int ac, char *av[])
{
	if (ac > 1) {
		std::string av1 = std::string(av[1]);
		if (av1 == "sixel") {
			return test_sixel();
		}
		if (av1 == "bg") {
			return test_bg();
		}
	}
	errx(1, "usage: <sixel | bg>");
}
#endif
