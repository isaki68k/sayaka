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

#include "eaw_code.h"
#include "eaw_data.h"

int opt_eaw_a;
int opt_eaw_n;

// Unicode コードポイント c の文字幅を返す。
// Narrow, HalfWidth は 1、
// Wide, FullWidth は 2、
// Neutral と Ambiguous は設定値による。
int
get_eaw_width(unichar c)
{
	uint8 packed;
	uint8 val;

	if (__predict_true((c / 2) < eaw2width_packed.size())) {
		packed = eaw2width_packed[c / 2];
	} else {
		// 安全のため FullWidth としておく
		packed = 0x11;
	}

	// 1バイトに2文字分埋め込んである
	if (c % 2 == 0) {
		val = packed >> 4;
	} else {
		val = packed & 0xf;
	}

	switch (val) {
	 case 0x0:	// H (Narrow, HalfWidth)
		return 1;

	 case 0x1:	// F (Wide, FullWidth)
		return 2;

	 case 0x2:	// N (Neutral)
		return opt_eaw_n;

	 case 0x3:	// A (Ambiguous)
		return opt_eaw_a;

	 default:
		__builtin_unreachable();
	}
}
