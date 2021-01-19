#include "FileUtil.h"
#include "autofd.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>

// .Net の File.ReadAllText() のようなもの。
// ただしエンコーディングはサポートしていない。
std::string
FileReadAllText(const std::string& filename)
{
	struct stat st;
	autofd fd;
	void *m;

	fd = open(filename.c_str(), O_RDONLY);
	if (fd < 0) {
		return "";
	}

	if (fstat(fd, &st) < 0) {
		return "";
	}

	m = mmap(NULL, st.st_size, PROT_READ, MAP_FILE, fd, 0);
	if (m == MAP_FAILED) {
		return "";
	}

	// ファイル全域を文字列とする
	std::string rv((const char *)m, st.st_size);

	munmap(m, st.st_size);
	return rv;
}

// .Net の File.WriteAllText() のようなもの。
bool
FileWriteAllText(const std::string& filename, const std::string& text)
{
	autofd fd;

	fd = open(filename.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (fd < 0) {
		return false;
	}

	auto r = write(fd, text.c_str(), text.size());
	if (r < text.size()) {
		return false;
	}
	return true;
}

// ファイルが存在すれば true を返す。
bool
FileUtil::Exists(const std::string& filename)
{
	if (access(filename.c_str(), F_OK) < 0) {
		return false;
	}
	return true;
}

#if defined(SELFTEST)
#include "test.h"

void
test_FileReadWriteAllText()
{
	// File{Read,Write}AllText() を両方一度にテストする
	printf("%s\n", __func__);

	autotemp filename("a.txt");
	{
		std::string exp = "hoge";

		auto r = FileWriteAllText(filename, exp);
		xp_eq(true, r);

		auto actual = FileReadAllText(filename);
		xp_eq(exp, actual);
	}
	{
		// 空文字列
		std::string exp;

		auto r = FileWriteAllText(filename, exp);
		xp_eq(true, r);

		auto actual = FileReadAllText(filename);
		xp_eq(exp, actual);
	}
}

void
test_FileUtil_Exists()
{
	autotemp filename("a");
	bool r;

	printf("%s\n", __func__);

	// 適当なチェックしかしてない

	// ファイルがない
	r = FileUtil::Exists(filename);
	xp_eq(false, r);

	// ファイルがある
	int fd = open(filename.c_str(), O_WRONLY | O_CREAT, 0644);
	if (fd >= 0)
		close(fd);
	r = FileUtil::Exists(filename);
}

void
test_FileUtil()
{
	test_FileReadWriteAllText();
	test_FileUtil_Exists();
}
#endif
