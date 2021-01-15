#pragma once

#include <sys/endian.h>
#include <sys/types.h>

#if _BYTE_ORDER == _LITTLE_ENDIAN
#define UTF32_HE "utf-32le"
#else
#define UTF32_HE "utf-32be"
#endif

using uint8 = uint8_t;
using unichar = uint32_t;

static const int ColorFixedX68k = -1;
