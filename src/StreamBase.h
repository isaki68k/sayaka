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
	// 読み出せれば *retval に読み出した1行を格納して true を返す。
	// エラーが発生すれば false を返し、*retval は不定。
	// 読み出した1行には改行を含む。ただし改行なしでストリームが終端すれば
	// 改行のない文字列を返す。
	// EOF なら *retval は empty で true を返す。
	bool ReadLine(std::string *retval);

	// 1行読み出す (エラーを返す手段のない版)
	std::string ReadLine();
};

// 出力ストリームの基底クラス
class OutputStream
{
 public:
	virtual ~OutputStream();
	virtual ssize_t Write(const char *buf, size_t len) = 0;
	virtual void Close();
};
