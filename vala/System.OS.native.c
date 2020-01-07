/*
 * Copyright (C) 2015-2020 Tetsuya Isaki
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

#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/param.h>
#include <sys/select.h>
#include <sys/sysctl.h>

int
native_ioctl_TIOCGWINSZ(int fd, struct winsize *ws)
{
	return ioctl(fd, TIOCGWINSZ, ws);
}

int
native_sysctlbyname(const char *sname,
	void *oldp, size_t *oldlenp,
	const void *newp, size_t newlen)
{
#if defined(__NetBSD__)	/* とりあえず */
	return sysctlbyname(sname, oldp, oldlenp, newp, newlen);
#else
	return -1;
#endif
}

// vala でこの辺の低レベル扱うのは出来るけど骨が折れるので、
// こっちでさくっとやってしまう。応答文字列だけ返して向こうで処理する
// くらいが格好はよさげだけど文字列を C -> vala に受け渡すとかさらに
// 面倒そうなので、もう判定まで全部こっちでやってしまう。
int
native_term_support_sixel()
{
	struct termios tc, old;
	struct timeval timeout;
	fd_set rfds;
	char answer[256];
	char query[4];

	FD_ZERO(&rfds);
	FD_SET(STDOUT_FILENO, &rfds);
	memset(&timeout, 0, sizeof(timeout));
	timeout.tv_sec = 1;

	// 応答受け取るため非カノニカルモードにするのと
	// その応答を画面に表示してしまわないようにエコーオフにする。
	tcgetattr(STDOUT_FILENO, &tc);
	old = tc;
	tc.c_lflag &= ~(ECHO | ICANON);
	tcsetattr(STDOUT_FILENO, TCSANOW, &tc);

	// 問い合わせ
	sprintf(query, "%c[c", 0x1b);
	write(STDOUT_FILENO, query, strlen(query));
	// 念のため応答なければタイムアウトする
	int r = select(STDOUT_FILENO + 1, &rfds, NULL, NULL, &timeout);
	if (r == -1) {
		warn("%s: select() failed", __func__);
		return 0;
	}
	if (r == 0) {
		warnx("%s: select() timeout", __func__);
		return 0;
	}
	int n = read(STDOUT_FILENO, answer, sizeof(answer));
	answer[n] = 0;

	// 端末を元に戻す
	tcsetattr(STDOUT_FILENO, TCSANOW, &old);

	// 応答を調べる。応答は
	// ESC "[?63;1;2;3;4;7;29c" のような感じで "4" があれば SIXEL 対応。
	// XXX use strtok_r
	char *p, *e;
	for (p = answer; p; p = e) {
		e = strchr(p, ';');
		if (e) {
			*e++ = '\0';
		}
		if (strcmp(p, "4") == 0) {
			return 1;
		}
	}
	return 0;
}
