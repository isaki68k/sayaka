
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
	RCM_Fixed256RGBI,
	RCM_Custom,
} ReductorColorMode;

typedef enum
{
	RIC_OK = 0,
	RIC_ARG_NULL = 1,
	RIC_ABORT_JPEG = 2,
} ReductorImageCode;

typedef enum
{
	RDM_FS,			// Floyd Steinberg
	RDM_ATKINSON,	// Atkinson
	RDM_JAJUNI,		// Jarvis, Judice, Ninke
	RDM_STUCKI,		// Stucki
	RDM_BURKES,		// Burkes
	RDM_2,			// (x+1,y), (x,y+1)
	RDM_3,			// (x+1,y), (x,y+1), (x+1,y+1)
	RDM_RGB,		// RGB color sepalated
} ReductorDiffuseMethod;

