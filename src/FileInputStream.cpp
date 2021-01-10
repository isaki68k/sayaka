#include "FileInputStream.h"

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
FileInputStream::Read(char *dst, size_t dstsize)
{
	ssize_t rv = 0;

	while (rv < dstsize) {
		size_t reqsize = dstsize - rv;
		int n = fread(dst + rv, 1, reqsize, fp);
		if (n < 1)
			break;
		rv += n;
	}

	return rv;
}

void
FileInputStream::Close()
{
	// この fp を所有するモードならクローズ
	if (own) {
		fclose(fp);
		fp = NULL;
	}
}