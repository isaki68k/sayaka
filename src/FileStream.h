#pragma once

#include "StreamBase.h"
#include <cstdio>

class FileInputStream : public InputStream
{
	using inherited = InputStream;
 public:
	FileInputStream(FILE *fp, bool own);
	virtual ~FileInputStream() override;

	ssize_t NativeRead(void *dst, size_t dstsize) override;
	void Close() override;

 private:
	FILE *fp {};
	bool own {};
};

class FileOutputStream : public OutputStream
{
	using inherited = OutputStream;
 public:
	FileOutputStream(FILE *fp, bool own);
	virtual ~FileOutputStream() override;

	ssize_t Write(const void *buf, size_t len) override;
	void Flush() override;
	void Close() override;

 private:
	FILE *fp {};
	bool own {};
};
