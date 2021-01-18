#pragma once

#include "StreamBase.h"

// ファイルディスクリプタ入力ストリーム
class FdInputStream : public InputStream
{
	using inherited = InputStream;
 public:
	FdInputStream(int fd, bool own);
	virtual ~FdInputStream() override;

	ssize_t Read(void *buf, size_t buflen) override;
	void Close() override;

 private:
	int fd {};
	bool own {};
};

#if 0 // notused
// ファイルディスクリプタ出力ストリーム
class FdOutputStream : public OutputStream
{
	using inherited = OutputStream;
 public:
	FdOutputStream(int fd, const Diag& diag);
	virtual ~FdOutputStream();

	ssize_t Write(char *buf, size_t buflen) override;
	void Close() override;

 private:
	int fd {};
};
#endif
