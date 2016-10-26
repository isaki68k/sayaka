#include <stdio.h>
#include <stdint.h>

typedef struct colorRGBint_t
{
	int r;
	int g;
	int b;
} colorRGBint;

// DDA 計算の基礎となる I + N / D 型の分数ステップ加減算計算機です。
typedef struct StepRational_t
{
	// 整数項です。
	int I;
	// 分子です。
	int N;
	// 分母です。
	int D;
} StepRational;

static StepRational
StepRationalCreate(int i, int n, int d)
{
	StepRational rv;
	rv.I = i;
	rv.N = n;
	rv.D = d;
	return rv;
}

static void
StepRationalAddStep(StepRational* sr, int nStep)
{
	sr->N += nStep;
	while (sr->N < 0) {
		sr->I--;
		sr->N += sr->D;
	}
	while (sr->N >= sr->D) {
		sr->I++;
		sr->N -= sr->D;
	}
}

static int
imagereductor_findfixed8(uint8_t r, uint8_t g, uint8_t b)
{
	uint8_t R = (uint8_t)(r >= 128);
	uint8_t G = (uint8_t)(g >= 128);
	uint8_t B = (uint8_t)(b >= 128);
	return R + (G << 1) + (B << 2);
}

static uint8_t
saturate_byte(int x)
{
	if (x < 0) return 0;
	if (x > 255) return 255;
	return (uint8_t)x;
}

int
imagereductor_resize_reduce_fixed8(
	uint8_t *dst, int dstLen,
	int dstWidth, int dstHeight,
	uint8_t *src, int srcLen,
	int srcWidth, int srcHeight,
	int srcNch, int srcStride)
{
// 水平方向は平均
// 垂直方向はスキップサンプリング

//fprintf(stderr, "dst=(%d,%d) src=(%d,%d)\n", dstWidth, dstHeight, srcWidth, srcHeight);

	colorRGBint col;
	col.r = col.g = col.b = 0;
	StepRational sr_y = StepRationalCreate(0, 0, dstHeight);

	for (int y = 0; y < dstHeight; y++) {
		uint8_t *srcRaster = &src[sr_y.I * srcStride];
//fprintf(stderr, "%d ", sr_y.I);
		StepRationalAddStep(&sr_y, srcHeight);
		StepRational sr_x = StepRationalCreate(0, 0, dstWidth);
		for (int x = 0; x < dstWidth; x++) {

			int sx0 = sr_x.I;
			StepRationalAddStep(&sr_x, srcWidth);
			int sx1 = sr_x.I;
			if (sx0 == sx1) sx1 = sx0 + 1;

			uint8_t *srcPix = &srcRaster[sx0 * srcNch];
			for (int sx = sx0; sx < sx1; sx++) {
				col.r += srcPix[0];
				col.g += srcPix[1];
				col.b += srcPix[2];
				srcPix += srcNch;	
			}

			int sumCount = sx1 - sx0;
			if (sumCount <= 1) {
				// nop
			} else if (sumCount == 2) {
				col.r /= 2;
				col.g /= 2;
				col.b /= 2;
			} else {
				col.r /= sumCount;
				col.g /= sumCount;
				col.b /= sumCount;
			}

			uint8_t f8 = imagereductor_findfixed8(
				saturate_byte(col.r),
				saturate_byte(col.g),
				saturate_byte(col.b));

			col.r -= (f8 & 1) << 8;
			col.g -= ((f8 >> 1) & 1) << 8;
			col.b -= ((f8 >> 2) & 1) << 8;

			*dst++ = f8;
		}
	}

	return 0;
}

