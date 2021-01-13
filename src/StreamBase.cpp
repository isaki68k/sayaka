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
// 改行まで読むが、retval に返す文字列は改行を取り除いたもの。
// 改行が来ずに終端した場合はそこまでの文字列を返す。
// EOF またはエラーなら false を返し retval は不定。
// EOF とエラーの区別は付かない。
bool
InputStream::ReadLine(std::string *retval)
{
	std::string& rv = *retval;
	char buf[1];
	ssize_t len;

	rv.clear();

	for (;;) {
		len = Read(buf, sizeof(buf));
		if (__predict_false(len < 0))
			return false;

		if (__predict_false(len == 0)) {
			if (rv.empty()) {
				// 行頭で EOF なら false で帰る
				return false;
			} else {
				// 行途中で EOF ならここまでの行を一旦返す
				// (次の呼び出しで即 EOF になるはず)
				break;
			}
		}

		rv += buf[0];
		if (buf[0] == '\n')
			break;
	}

	// 読み込んだ行から改行文字は取り除いて返す
	for (;;) {
		char c = rv.back();
		if (c == '\r' || c == '\n') {
			rv.pop_back();
		} else {
			break;
		}
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
