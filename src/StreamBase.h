#pragma once

#include "sayaka.h"
#include <string>
#include <vector>

// 入力ストリームの基底クラス
class InputStream
{
 public:
	virtual ~InputStream();

	// このストリームから dst に読み出す。
	// dstsize までもしくは内部ストリームが EOF になるまで。
	// 読み出したバイト数を返す。
	// エラーなら -1 を返し dst の内容は不定。
	virtual ssize_t Read(char *dst, size_t dstsize) = 0;

	virtual void Close();

	// 1行読み出す。
	// 改行まで読むが、retval に返す文字列は改行を取り除いたもの。
	// 改行が来ずに終端した場合はそこまでの文字列を返す。
	// EOF またはエラーなら false を返し retval は不定。
	// EOF とエラーの区別は付かない。
	bool ReadLine(std::string *retval);
};

// 出力ストリームの基底クラス
class OutputStream
{
 public:
	virtual ~OutputStream();
	virtual ssize_t Write(const char *buf, size_t len) = 0;
	virtual void Close();
};
