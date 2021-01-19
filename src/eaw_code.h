#pragma once

#include "sayaka.h"

extern int opt_eaw_a;
extern int opt_eaw_n;

extern int get_eaw_width(unichar c);

#if defined(SELFTEST)
extern void test_eaw_code();
#endif
