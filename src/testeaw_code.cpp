/*
 * Copyright (C) 2021 Tetsuya Isaki
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

#include "test.h"
#include "eaw_code.h"

void
test_eaw_code()
{
	// 1バイト内に Full と Half が同居してるところでチェック。
	// U+FF60 (FULLWIDTH RIGHT WHITE PARENTHESIS) は FullWidth、
	// U+FF61 (HALFWIDTH IDEOGRAPHIC FULL STOP) は HalfWidth。
	xp_eq(2, get_eaw_width(0xff60));
	xp_eq(1, get_eaw_width(0xff61));

	// Neutral が変数と連動するかチェック。
	// U+00A9 (COPYRIGHT SIGN) は Neutral。
	opt_eaw_n = 1;
	xp_eq(opt_eaw_n, get_eaw_width(0x00a9));
	opt_eaw_n = 2;
	xp_eq(opt_eaw_n, get_eaw_width(0x00a9));

	// Ambiguous が変数と連動するかチェック。
	// U+0411 (CYRILLIC CAPITAL LETTER BE) は Ambiguous。
	opt_eaw_a = 1;
	xp_eq(opt_eaw_a, get_eaw_width(0x0411));
	opt_eaw_a = 2;
	xp_eq(opt_eaw_a, get_eaw_width(0x0411));

	// U+1F43F (リスの絵文字) は仕様上は何故か Neutral (幅=1) なのだが
	// それではたぶん困るので回避したい。
	opt_eaw_n = -1;
	xp_eq(2, get_eaw_width(0x1f43e));	// Paw Prints (猫の足跡) は Wide
	xp_eq(2, get_eaw_width(0x1f43f));	// Chipmunk (リス)       は Neutral…
}
