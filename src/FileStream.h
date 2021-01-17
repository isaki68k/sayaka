#pragma once

#include "StreamBase.h"
#include <cstdio>

class FileInputStream : public InputStream
{
	using inherited = InputStream;
 public:
	FileInputStream(FILE *fp, bool own);
	virtual ~FileInputStream() override;

	ssize_t Read(char *dst, size_t dstsize) override;
	void Close() override;

 private:
	FILE *fp {};
	bool own {};
};
