
typedef enum
{
	RCM_Mono,
	RCM_Gray,
	RCM_GrayMean,
	RCM_Fixed8,
	RCM_FixedX68k,
	RCM_CustomX68k,
	RCM_FixedANSI16,
	RCM_Fixed256,
	RCM_Custom,
} ReductorColorMode;

typedef enum
{
	RIC_OK = 0,
	RIC_ARG_NULL = 1,
	RIC_ABORT_JPEG = 2,
} ReductorImageCode;
