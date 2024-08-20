/* vi:set ts=4: */
/*
 * Copyright (C) 2016-2024 Tetsuya Isaki
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
// sayaka/sixelv 共通の雑多なルーチン
//

#include "common.h"
#include <errno.h>
#include <stdint.h>
#include <string.h>

// strerror(errno) は Debug() 等のマクロ内から呼ぶことが多いのに
// clang だと errno が再帰展開になるとかで怒られるので、回避のため。
const char *
strerrno(void)
{
	return strerror(errno);
}

// 文字列 s 末尾の連続する改行を取り除く。
void
chomp(char *s)
{
	char *p = s + strlen(s);
	while (--p >= s) {
		if (*p == '\r' || *p == '\n') {
			*p = '\0';
		} else {
			break;
		}
	}
}

// 文字列 s を10進数符号なし整数とみなして数値に変換する、簡易版。
// 数値以外の文字が来たところで変換を終了する。
// 変換できればその値を返す。出来なければ errno をセットし defval を返す。
// errno は EINVAL なら s が NULL か数字で始まっていない(負号を含む)。
// errno が ERANGE なら uint32 で表現出来ない。
// strto*(3) と異なり前置されている空白文字をスキップしない。
// endp が NULL でなければ、*endp に数値の次の文字の位置を返す。
// 変換できなければ endp は変更しない。
uint32
stou32def(const char *s, uint32 defval, char **endp)
{
	uint32 val;
	int error;

	val = 0;
	error = EINVAL;	// 1桁もなければエラー

	if (__predict_false(s == NULL)) {
		errno = error;
		return defval;
	}

	for (; *s; s++) {
		int c = *s;

		if (c < '0' || c > '9') {
			break;
		}
		c -= '0';

		// オーバーフローチェックは演算ごとに必要
		uint32 n1 = val * 10;
		uint32 n2 = n1 + c;
		if (__predict_false(n1 < val || n2 < val)) {
			errno = ERANGE;
			return defval;
		}
		val = n2;
		error = 0;
	}

	if (__predict_false(error != 0)) {
		errno = error;
		return defval;
	}

	if (endp) {
		*endp = UNCONST(s);
	}
	return val;
}
