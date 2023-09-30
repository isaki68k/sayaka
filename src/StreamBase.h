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
#include <vector>

//
// 入力ストリームの基底クラス
//
class InputStream
{
 public:
	virtual ~InputStream();

	// このストリームから dst に読み出す。
	// dstsize までもしくは内部ストリームが EOF になるまで。
	// 読み出したバイト数を返す。
	// エラーなら errno をセットし -1 を返す。この場合 dst の内容は不定。
	ssize_t Read(void *dst, size_t dstsize);

	// 読み込みポインタを進めず、このストリームから dst に読み出す。
	// 読み出したバイト数を返す。
	// エラーなら errno をセットし -1 を返す。この場合 dst の内容は不定。
	ssize_t Peek(void *dst, size_t dstsize);

	// 読み込みポインタを移動する。
	off_t Seek(off_t pos, int whence);

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

	virtual void Close();

 protected:
	// このストリームから dst に読み出す。
	// dstsize までもしくは内部ストリームが EOF になるまで。
	// 読み出したバイト数を返す。
	// エラーなら errno をセットし -1 を返す。この場合 dst の内容は不定。
	virtual ssize_t NativeRead(void *dst, size_t dstsize) = 0;

 private:
	// ピーク用のバッファ
	std::vector<uint8> peekbuf {};

	size_t pos {};
};

//
// 出力ストリームの基底クラス
//
class OutputStream
{
 public:
	virtual ~OutputStream();

	// buf から len バイトを書き出す。
	// 書き出したバイト数を返す。
	// エラーなら errno をセットし -1 を返す。この場合書き出し内容は不定。
	virtual ssize_t Write(const void *buf, size_t len) = 0;

	virtual void Flush() = 0;

	virtual void Close();

	// std::string を書き出す。
	// 成功すれば true、失敗すれば false を返す。
	// 下位の Write() が size 未満の値を返したのか -1 を返したのかは
	// 判別できないので errno も参照不可。
	bool Write(const std::string& str) {
		auto len = str.size();
		auto n = Write(str.c_str(), len);
		if (n < len) {
			return false;
		}
		return true;
	}
};
