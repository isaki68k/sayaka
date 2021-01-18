#include "FileStream.h"

//
// FILE* 入力ストリーム
//

// コンストラクタ
FileInputStream::FileInputStream(FILE *fp_, bool own_)
{
	fp = fp_;
	own = own_;
}

// デストラクタ
FileInputStream::~FileInputStream()
{
	Close();
}

// dst に読み出す
ssize_t
FileInputStream::Read(void *dst, size_t dstsize)
{
	ssize_t rv = 0;

	while (rv < dstsize) {
		size_t reqsize = dstsize - rv;
		size_t n = fread((char *)dst + rv, 1, reqsize, fp);
		if (__predict_false(n == 0)) {
			if (feof(fp)) {
				return 0;
			} else {
				errno = ferror(fp);
				return -1;
			}
		}
		rv += n;
	}

	return rv;
}

// 巻き戻す
bool
FileInputStream::Rewind()
{
	auto r = fseek(fp, 0, SEEK_SET);
	if (r < 0) {
		return false;
	}
	return 0;
}

void
FileInputStream::Close()
{
	// この fp を所有するモードならクローズ
	if (own) {
		if (fp) {
			fclose(fp);
		}
		fp = NULL;
	}
}


//
// FILE* 出力ストリーム
//

// コンストラクタ
FileOutputStream::FileOutputStream(FILE *fp_, bool own_)
{
	fp = fp_;
	own = own_;
}

// デストラクタ
FileOutputStream::~FileOutputStream()
{
	Close();
}

// buf を書き出す
ssize_t
FileOutputStream::Write(const void *buf, size_t len)
{
	return fwrite(buf, 1, len, fp);
}

// フラッシュする
void
FileOutputStream::Flush()
{
	fflush(fp);
}

void
FileOutputStream::Close()
{
	// この fp を所有するモードならクローズ
	if (own) {
		if (fp) {
			fclose(fp);
		}
		fp = NULL;
	}
}
