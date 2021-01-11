#pragma once

#include "Json.h"
#include <ctime>
#include <string>
#include <vector>

using unichar = uint32_t;

extern std::string formatname(const std::string& text);
extern std::string formatid(const std::string& text);
extern std::string unescape(const std::string& text);
extern std::string strip_tags(const std::string& text);

extern std::string formattime(const Json& obj);
extern time_t get_datetime(const Json& status);
extern time_t conv_twtime_to_unixtime(const std::string& s);
extern int my_strptime(const std::string& buf, const std::string& fmt);

extern std::vector<unichar> Utf8ToUnicode(const std::string& utf8str);

#if defined(SELFTEST)
extern void test_subr();
#endif
