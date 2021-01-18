#pragma once

#include "Diag.h"
#include "StreamBase.h"
#include "MemoryStream.h"

class ChunkedInputStream : public InputStream
{
 public:
	ChunkedInputStream(InputStream *src_, Diag& diag_);
	virtual ~ChunkedInputStream() override;

	ssize_t NativeRead(void *dst, size_t dstsize) override;

 private:
	// 入力ストリーム
	InputStream *src {};

	// 内部バッファ
	MemoryInputStream Chunks {};

	Diag& diag;
};

#if defined(SELFTEST)
extern void test_ChunkedInputStream();
#endif
