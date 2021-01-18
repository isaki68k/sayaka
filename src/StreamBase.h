#pragma once

#include "sayaka.h"
#include <string>

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
	virtual ssize_t Read(void *dst, size_t dstsize) = 0;

	// このストリームのポインタを先頭に戻す。
	// 成功すれば true、失敗すれば errno をセットし false を返す。
	virtual bool Rewind();

	virtual void Close();

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
};

//
// 出力ストリームの基底クラス
//
class OutputStream
{
 public:
	virtual ~OutputStream();

	virtual ssize_t Write(const void *buf, size_t len) = 0;

	virtual void Flush() = 0;

	virtual void Close();

	// std::string を書き出す
	ssize_t Write(const std::string& str) {
		return Write(str.c_str(), str.size());
	}
};
