/*
 * Copyright (C) 2025-2026 Tetsuya Isaki
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

// <ctype.h> の isxxx(3) はロケールに準じた動作をする仕様なので将来に渡って
// 知らない言語で想定外の振る舞いをする可能性をゼロに出来ないのと、
// int を受け取るため 0x80 以上の char はそのままだと暗黙変換で負数になるが、
// -1 以外の負数は範囲外で未定義動作という仕様との合わせ技が起きやすい。
// さらに NetBSD 11 でこの未定義のケースは abort する挙動になってしまった。
// 未定義なのでそう実装しても問題はないが本当にするやつがあるか…。
// これではただでさえロケールの問題だけでも使いにくいのに、キャストに気をつけ
// 続けないと簡単に、コンパイル時ではなく、実行時に即死する実行ファイルが
// 出来上がってしまう。
// さすがに痛すぎるので車輪の再開発をして自前で安全な似たのを用意する。
// うーんこの…。

// 以下の is_ascii_*() 関数群は、ロケールの影響を受けない。
// また入力として EOF は扱わない。そもそも文字ではないのだから事前に弾くべき。
// ただ unsigned char で受け取るため EOF は 255 になり、現在定義している
// いずれの is_ascii_*() も安全に false を返す。

#ifndef nono_ascii_ctype_h
#define nono_ascii_ctype_h

#if defined(__cplusplus)
#define INLINE inline
#else
#define INLINE static inline
#endif

// 今どきテーブルより条件分岐のほうが早そう。

#include <stdbool.h>

INLINE bool
is_ascii_space(unsigned char c)
{
	// 空白文字はスペース、タブの他 CR, LF, FF, VT を含む。
	// この集合は isspace(3) 準拠。
	// TAB(\x9),LF(\xa),VT(\xb),FF(\xc),CR(\xd) は連続しているので
	// たぶん最適化される。
	if (c == ' ' ||
	    c == '\t' || c == '\r' || c == '\n' || c == '\f' || c == '\v')
	{
		return true;
	}
	return false;
}

INLINE bool
is_ascii_digit(unsigned char c)
{
	if ('0' <= c && c <= '9') {
		return true;
	}
	return false;
}

INLINE bool
is_ascii_upper(unsigned char c)
{
	if ('A' <= c && c <= 'Z') {
		return true;
	}
	return false;
}

INLINE bool
is_ascii_lower(unsigned char c)
{
	if ('a' <= c && c <= 'z') {
		return true;
	}
	return false;
}

INLINE bool
is_ascii_alpha(unsigned char c)
{
	if (is_ascii_upper(c) || is_ascii_lower(c)) {
		return true;
	}
	return false;
}

INLINE bool
is_ascii_alnum(unsigned char c)
{
	if (is_ascii_alpha(c) || is_ascii_digit(c)) {
		return true;
	}
	return false;
}

INLINE bool
is_ascii_xdigit(unsigned char c)
{
	if (is_ascii_digit(c) || ('A' <= c && c <= 'F') || ('a' <= c && c <= 'f')) {
		return true;
	}
	return false;
}

INLINE bool
is_ascii_print(unsigned char c)
{
	if (' ' <= c && c < 0x7f) {
		return true;
	}
	return false;
}

#undef INLINE

#endif /* nono_ascii_ctype_h */
