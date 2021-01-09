#include "MemoryInputStream.h"
#include <cstring>
#include <cstdio>
#include <tuple>

// GLib の MemoryInputStream 互換っぽいもの

// コンストラクタ
MemoryInputStream::MemoryInputStream()
{
}

// コンストラクタ
MemoryInputStream::MemoryInputStream(const std::vector<uint8>& src)
{
	AddData(src);
}

// デストラクタ
MemoryInputStream::~MemoryInputStream()
{
}

// データを末尾に追加
void
MemoryInputStream::AddData(const char *src, int srclen)
{
	std::vector<uint8> data;
	data.resize(srclen);
	for (auto& c : data) {
		c = *src++;
	}
	AddData(data);
}

// データを末尾に追加
void
MemoryInputStream::AddData(const std::vector<uint8>& src)
{
	chunks.emplace_back(src, 0);
}

// dst に読み出す
ssize_t
MemoryInputStream::Read(char *dst, size_t dstsize)
{
	ssize_t rv = 0;

	while (chunks.empty() == false && rv < dstsize) {
		auto& [buf, offset] = chunks.front();

		// コピー
		auto copylen = std::min(dstsize - rv, buf.size() - offset);
		memcpy(dst + rv, buf.data() + offset, copylen);
		rv += copylen;
		offset += copylen;

		// 末尾まで読んだらこの chunk を捨てる
		if (offset >= buf.size()) {
			chunks.pop_front();
		}
	}

	return rv;
}

// このストリームの残りバイト数を返す。
size_t
MemoryInputStream::GetSize() const
{
	size_t size = 0;

	for (const auto& chunk : chunks) {
		auto& [buf, offset] = chunk;

		size += buf.size() - offset;
	}
	return size;
}

#if defined(SELFTEST)
#include "test.h"
void
test_MemoryInputStream()
{
	printf("%s\n", __func__);

	// 空コンストラクタ
	{
		MemoryInputStream ms;
		xp_eq(0, ms.GetSize());
	}
	// chunk1つを小分けに取り出す
	{
		std::vector<uint8> src { 'a', 'b', 'c' };
		MemoryInputStream ms(src);
		xp_eq(3, ms.GetSize());

		char buf[2];
		memset(buf, 0, sizeof(buf));
		auto actual = ms.Read(buf, sizeof(buf));
		xp_eq(2, actual);
		xp_eq(1, ms.GetSize());
		xp_eq('a', buf[0]);
		xp_eq('b', buf[1]);

		memset(buf, 0, sizeof(buf));
		actual = ms.Read(buf, sizeof(buf));
		xp_eq(1, actual);
		xp_eq(0, ms.GetSize());
		xp_eq('c', buf[0]);

		actual = ms.Read(buf, sizeof(buf));
		xp_eq(0, actual);
		xp_eq(0, ms.GetSize());
	}
	// 複数chunk
	{
		std::vector<uint8> src { 'a', 'b' };
		MemoryInputStream ms(src);

		ms.AddData(src);

		char buf[4];
		memset(buf, 0, sizeof(buf));
		auto actual = ms.Read(buf, sizeof(buf));
		xp_eq(4, actual);
		xp_eq(0, ms.GetSize());
		xp_eq('a', buf[0]);
		xp_eq('b', buf[1]);
		xp_eq('a', buf[2]);
		xp_eq('b', buf[3]);
	}
}
#endif // SELFTEST
