#include "term.h"
#include <err.h>
#include <stdio.h>
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

// 端末が SIXEL をサポートしていれば true を返す
bool
terminal_support_sixel()
{
	struct termios tc, old;
	struct timeval timeout;
	fd_set rfds;
	char answer[256];

	FD_ZERO(&rfds);
	FD_SET(STDOUT_FILENO, &rfds);
	memset(&timeout, 0, sizeof(timeout));
#if defined(SLOW_MACHINES)
	timeout.tv_sec = 10;
#else
	timeout.tv_sec = 2;
#endif

	// 応答受け取るため非カノニカルモードにするのと
	// その応答を画面に表示してしまわないようにエコーオフにする。
	tcgetattr(STDOUT_FILENO, &tc);
	old = tc;
	tc.c_lflag &= ~(ECHO | ICANON);
	tcsetattr(STDOUT_FILENO, TCSANOW, &tc);

	// 問い合わせ
	write(STDOUT_FILENO, ESC "[c", 3);
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
		} else {
			e = strrchr(p, 'c');
			if (e) {
				*e = '\0';
				e = NULL;
			}
		}

		if (strcmp(p, "4") == 0) {
			return 1;
		}
	}
	return 0;
}
