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

#pragma once

#include "header.h"
#include <string>

// ストリーム (基本クラス)
class Stream
{
 public:
	virtual ~Stream();

	virtual void Close();

	// ストリームから dst に最大 dstlen バイト読み込む。
	// 成功すれば読み込んだバイト数を、
	// 失敗すれば errno をセットして -1 を返す。
	virtual ssize_t Read(void *dst, size_t dstlen);

	// ストリームに src から srclen バイトを書き込む。
	// 成功すれば書き込んだバイト数を、
	// 失敗すれば errno をセットして -1 を返す。
	virtual ssize_t Write(const void *src, size_t srclen);

	// ストリームをフラッシュする。
	virtual void Flush();

	// ストリームの現在位置を設定する。
	// 成功すれば true を、
	// 失敗すれば errno をセットして false を返す。
	virtual bool Seek(ssize_t offset, int whence);

	// ストリームの現在位置を取得する。
	// 成功すれば現在位置を、
	// 失敗すれば errno をセットして -1 を返す。
	virtual ssize_t GetPos() const;

	// 1行読み出す。
	// 改行まで読み込み、その改行を取り除いた文字列を *retval に書き戻す。
	// 改行が来ずにストリームが終端した場合はそこまでの文字列を書き戻す。
	// いずれの場合も戻り値にはストリームから読み込んだバイト数を返す
	// (*retval に書き戻した文字列の長さではない点に注意)。
	// エラーなら errno をセットして -1 を返す。この場合 *retval は不定。
	//
	// これにより、
	// o 戻り値が 1 以上なら1行を受信。(ただし空行を受信して改行を取り除いた
	//   結果 empty になっているかも知れない)
	// o 戻り値が 0 なら EOF
	// o 戻り値が -1 ならエラー
	// となる。
	ssize_t ReadLine(std::string *retval);

	ssize_t Write(const std::string& str) {
		return Write(str.c_str(), str.length());
	}
};
