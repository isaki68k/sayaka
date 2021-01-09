#pragma once

#include <string>

extern std::string FileReadAllText(const std::string& filename);
extern bool FileWriteAllText(const std::string& filename,
	const std::string& text);

class FileUtil
{
 public:
	static bool Exists(const std::string& filename);
};

#if defined(SELFTEST)
extern void test_FileUtil();
#endif
