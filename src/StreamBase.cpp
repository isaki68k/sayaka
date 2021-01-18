#include "StreamBase.h"
#include <cstring>

//
// 入力ストリームの基本クラス
//

// デストラクタ
InputStream::~InputStream()
{
}

// 読み出す。
ssize_t
InputStream::Read(void *dst, size_t dstsize)
{
	ssize_t rv = 0;

	// すでに Peek() で読んだのがあれば使う
	auto peeksize = peekbuf.size();
	if (__predict_false(peeksize > 0)) {
		auto len = std::min(peeksize, dstsize);
		memcpy(dst, peekbuf.data(), len);
		rv = len;
		peekbuf.erase(peekbuf.begin(), peekbuf.begin() + len);

		if (rv >= dstsize) {
			return rv;
		}
	}

	// 残りを読み込む
	auto r = NativeRead((char *)dst + rv, dstsize - rv);
	if (r < 0) {
		return r;
	}
	rv += r;
	return rv;
}

// 覗き見する。
ssize_t
InputStream::Peek(void *dst, size_t dstsize)
{
	std::vector<uint8> tmp(dstsize);

	auto r = NativeRead(tmp.data(), tmp.size());
	if (__predict_false(r <= 0)) {
		return r;
	}

	// 内部バッファに追加して
	tmp.resize(r);
	for (const auto c : tmp) {
		peekbuf.emplace_back(c);
	}

	// dst に書き出す
	memcpy(dst, tmp.data(), r);
	return r;
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

// クローズ
void
InputStream::Close()
{
}

//
// 出力ストリームの基本クラス
//

// デストラクタ
OutputStream::~OutputStream()
{
}

// クローズ
void
OutputStream::Close()
{
}
