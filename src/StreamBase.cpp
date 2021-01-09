#include "StreamBase.h"

//
// 基底ストリームクラス
//

// デストラクタ
InputStream::~InputStream()
{
}

// クローズ
void
InputStream::Close()
{
}

// 1行読み出す
std::string
InputStream::ReadLine()
{
	std::string rv;
#if 0
	if (ReadLine(&rv) == false) {
		rv.clear();
	}
	return rv;
#else
	char buf[1];
	ssize_t len;

	for (;;) {
		len = Read(buf, sizeof(buf));
		if (len < 1)
			break;

		rv += buf[0];
		if (buf[0] == '\n')
			break;
	}

	return rv;
#endif
}

// 1行読み出す。
bool
InputStream::ReadLine(std::string *retval)
{
	std::string& rv = *retval;
	char buf[1];
	ssize_t len;

	rv.clear();

	for (;;) {
		len = Read(buf, sizeof(buf));
		if (len < 0)
			return false;

		if (len == 0)
			break;

		rv += buf[0];
		if (buf[0] == '\n')
			break;
	}
	return true;
}

// デストラクタ
OutputStream::~OutputStream()
{
}

// クローズ
void
OutputStream::Close()
{
}

#if 0

// ファイルディスクリプタ入力ストリーム
class FdInputStream : public InputStream
{
	using inherited = InputStream;
 public:
	FdInputStream(int fd, const Diag& diag);
	virtual ~FdInputStream();

	ssize_t Read(char *buf, size_t buflen) override;
	void Close() override;

 private:
	int fd {};
};

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

//
// ファイルディスクリプタ入力ストリーム
//

// コンストラクタ
FdInputStream::FdInputStream(int fd_, const Diag& diag_)
	: inherited(diag_)
{
	fd = fd_;
}

// デストラクタ
FdInputStream::~FdInputStream()
{
	Close();
}

// 読み出し
ssize_t
FdInputStream::Read(char *buf, size_t buflen)
{
	return read(fd, buf, buflen);
}

// クローズ
void
FdInputStream::Close()
{
	close(fd);
}

//
// ファイルディスクリプタ出力ストリーム
//

// コンストラクタ
FdOutputStream::FdOutputStream(int fd_, const Diag& diag_)
	: inherited(diag_)
{
	fd = fd_;
}

// デストラクタ
FdOutputStream::~FdOutputStream()
{
	Close();
}

// 書き込み
ssize_t
FdOutputStream::Write(char *buf, size_t buflen)
{
	return write(fd, buf, buflen);
}

// クローズ
void
FdOutputStream::Close()
{
	close(fd);
}


#endif
