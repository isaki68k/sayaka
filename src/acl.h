#pragma once

#include "Json.h"
#include <string>

extern bool acl(const Json& status, bool is_quoted);

#if defined(SELFTEST)
extern void test_acl();
#endif
