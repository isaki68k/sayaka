#pragma once

#include <sys/endian.h>
#include <sys/types.h>

#if _BYTE_ORDER == _LITTLE_ENDIAN
#define UTF32_HE "utf-32le"
#else
#define UTF32_HE "utf-32be"
#endif

using int8    = int8_t;
using int16   = int16_t;
using int32   = int32_t;
using uint8   = uint8_t;
using uint16  = uint16_t;
using uint32  = uint32_t;
using unichar = uint32_t;

static const int ColorFixedX68k = -1;
