/*
 * Copyright (C) 2014-2023 Tetsuya Isaki
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
#include "JsonInc.h"
#include "term.h"

// 現在行にアイコンを表示。
// 呼び出し時点でカーソルは行頭にあるため、必要なインデントを行う。
// アイコン表示後にカーソル位置を表示前の位置に戻す。
// 実際のアイコン表示そのものはサービスごとに callback(user, userid) で行う。
// userid はキャッシュファイルに使うユーザ名(アカウント名)文字列。
// 呼び出し元ではすでに持ってるはずなので。
// callback() はアイコンを表示できれば true を返すこと。
void
ShowIcon(bool (*callback)(const Json&, const std::string&),
	const Json& user, const std::string& userid)
{
	// 改行x3 + カーソル上移動x3 を行ってあらかじめスクロールを
	// 発生させ、アイコン表示時にスクロールしないようにしてから
	// カーソル位置を保存する
	// (スクロールするとカーソル位置復元時に位置が合わない)
	printf("\n\n\n" CSI "3A" ESC "7");

	// インデント。
	// CSI."0C" は0文字でなく1文字になってしまうので、必要な時だけ。
	if (indent_depth > 0) {
		int left = indent_cols * indent_depth;
		printf(CSI "%dC", left);
	}

	bool shown = false;
	if (__predict_true(use_sixel != UseSixel::No)) {
		// ここがサービスごとに違う部分。
		// user から実際に画像を表示する。
		shown = callback(user, userid);
	}

	if (__predict_true(shown)) {
		// アイコン表示後、カーソル位置を復帰
		printf("\r");
		// カーソル位置保存/復元に対応していない端末でも動作するように
		// カーソル位置復元前にカーソル上移動x3を行う
		printf(CSI "3A" ESC "8");
	} else {
		// アイコンを表示してない場合はここで代替アイコンを表示。
		printf(" *");
		// これだけで復帰できるはず
		printf("\r");
	}
}
