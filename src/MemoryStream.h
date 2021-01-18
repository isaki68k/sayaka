#pragma once

#include "StreamBase.h"
#include <list>
#include <vector>

class MemoryInputStream : public InputStream
{
 public:
	MemoryInputStream();
	MemoryInputStream(const std::vector<uint8>& src);
	virtual ~MemoryInputStream() override;

	// エラーなら -1 を返す仕様だが現状ここではエラーは起きない。
	ssize_t NativeRead(void *buf, size_t bufsize) override;

	// データを末尾に追加
	void AddData(const std::vector<uint8>& src);
	void AddData(const char *src, int srclen);

	// このストリームの残りバイト数を返す。
	size_t GetSize() const;

 private:
	// 内部バッファ list< pair(vector<uint8>, int) >
	std::list<std::pair<std::vector<uint8>, int>> chunks {};
};

#if defined(SELFTEST)
extern void test_MemoryInputStream();
#endif
