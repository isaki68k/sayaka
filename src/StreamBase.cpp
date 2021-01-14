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

// 1行読み出す。
ssize_t
InputStream::ReadLine(std::string *retval)
{
	std::string& str = *retval;
	char buf[1];
	ssize_t retlen;

	str.clear();
	retlen = 0;

	for (;;) {
		ssize_t readlen;
		readlen = Read(buf, sizeof(buf));
		if (__predict_false(readlen < 0))
			return readlen;

		if (__predict_false(readlen == 0)) {
			break;
		}

		str += buf[0];
		retlen++;
		if (buf[0] == '\n')
			break;
	}

	// 返す文字列から改行を削除
	for (;;) {
		char c = str.back();
		if (c == '\r' || c == '\n') {
			str.pop_back();
		} else {
			break;
		}
	}

	// (改行を削除する前の) 受信したバイト数を返す
	return retlen;
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
