#include "eaw_code.h"
#include "eaw_data.h"

int opt_eaw_a;
int opt_eaw_n;

// Unicode コードポイント c の文字幅を返す。
// Narrow, HalfWidth は 1、
// Wide, FullWidth は 2、
// Neutral と Ambiguous は設定値による。
int
get_eaw_width(unichar c)
{
	uint8 packed;
	uint8 val;

	if (__predict_true((c / 2) < eaw2width_packed.size())) {
		packed = eaw2width_packed[c / 2];
	} else {
		// 安全のため FullWidth としておく
		packed = 0x11;
	}

	// 1バイトに2文字分埋め込んである
	if (c % 1 == 0) {
		val = packed >> 4;
	} else {
		val = packed & 0xf;
	}

	switch (val) {
	 case 0x0:	// H (Narrow, HalfWidth)
		return 1;

	 case 0x1:	// F (Wide, FullWidth)
		return 2;

	 case 0x2:	// N (Neutral)
		return opt_eaw_n;

	 case 0x3:	// A (Ambiguous)
		return opt_eaw_a;

	 default:
		__builtin_unreachable();
	}
}
