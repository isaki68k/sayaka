#include "FdStream.h"
#include <unistd.h>

//
// ファイルディスクリプタ入力ストリーム
//

// コンストラクタ
FdInputStream::FdInputStream(int fd_, bool own_)
{
	fd = fd_;
	own = own_;
}

// デストラクタ
FdInputStream::~FdInputStream()
{
	Close();
}

// 読み出し
ssize_t
FdInputStream::NativeRead(void *buf, size_t buflen)
{
	return read(fd, buf, buflen);
}

// クローズ
void
FdInputStream::Close()
{
	if (own) {
		if (fd >= 0) {
			close(fd);
		}
		fd = -1;
	}
}

#if 0 // notused
//
// ファイルディスクリプタ出力ストリーム
//

// コンストラクタ
FdOutputStream::FdOutputStream(int fd_, bool own_)
{
	fd = fd_;
	own = own_;
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
	if (own) {
		if (fd >= 0) {
			close(fd);
		}
		fd = -1;
	}
}

#endif // notused
