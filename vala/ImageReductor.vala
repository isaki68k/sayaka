using Gdk;

// vala 宣言
// 減色モード
public enum ReductorReduceMode
{
	// 速度優先法
	Fast,

	// 単純一致法
	Simple,

	// 2次元誤差分散法
	HighQuality,
}

// C の定義と全く同じにしてください。

[CCode (cname="ReductorColorMode", cprefix="RCM_", has_type_id=false)]
public enum ReductorColorMode
{
	Mono,
	Gray,
	GrayMean,
	Fixed8,
	FixedX68k,
	CustomX68k,
	FixedANSI16,
	Fixed256,
	Fixed256RGBI,
	Custom,
}

[CCode (cname="ReductorFinderMode", has_type_id=false)]
public enum ReductorFinderMode
{
	RFM_Default,
	RFM_HSV,
}

[CCode (cname="ReductorImageCode", has_type_id=false)]
public enum ReductorImageCode
{
	RIC_OK = 0,
	RIC_ARG_NULL = 1,
	RIC_ABORT_JPEG = 2,
}

[CCode (cname="ReductorDiffuseMethod", has_type_id=false)]
public enum ReductorDiffuseMethod
{
	RDM_FS,			// Floyd Steinberg
	RDM_ATKINSON,	// Atkinson
	RDM_JAJUNI,		// Jarvis, Judice, Ninke
	RDM_STUCKI,		// Stucki
	RDM_BURKES,		// Burkes
	RDM_2,			// (x+1,y), (x,y+1)
	RDM_3,			// (x+1,y), (x,y+1), (x+1,y+1)
	RDM_RGB,		// RGB color sepalated
}

[CCode(cname="ColorRGBuint8")]
public struct ColorRGBuint8
{
	uint8 r;
	uint8 g;
	uint8 b;
}


[CCode(cname="ImageReductor_Image")]
public struct ImageReductor_Image
{
	[CCode(array_length_cname="DataLen", array_length_type="int32_t")]
	uint8[] Data;
	int32 Width;
	int32 Height;
	int32 ChannelCount;
	int32 RowStride;
	int32 OriginalWidth;
	int32 OriginalHeight;

	ImageReductor_ReadCallback ReadCallback;

	// ユーザが自由に使っていい。コールバック元の this 入れるとか。
	void *UserObject;

	uint8 ReadBuffer[4096];
}

// なぜか ImageReductor_Image より後ろに書かないと C コンパイルできない
// valac のバグと思われる
[CCode(cname="ImageReductor_ReadCallback", has_target=false)]
public delegate int ImageReductor_ReadCallback(ImageReductor_Image* img);


// C コードの宣言

extern void ImageReductor_SetColorMode(
	ReductorColorMode mode, ReductorFinderMode finder, int count);

extern int ImageReductor_Fast(
	uint8[] dst,
	int dstWidth, int dstHeight,
	uint8[] src,
	int srcWidth, int srcHeight,
	int srcNch, int srcStride);

extern int ImageReductor_Simple(
	uint8[] dst,
	int dstWidth, int dstHeight,
	uint8[] src,
	int srcWidth, int srcHeight,
	int srcNch, int srcStride);

extern int ImageReductor_HighQuality(
	uint8[] dst,
	int dstWidth, int dstHeight,
	uint8[] src,
	int srcWidth, int srcHeight,
	int srcNch, int srcStride);

extern ImageReductor_Image* ImageReductor_AllocImage();

extern void ImageReductor_FreeImage(ImageReductor_Image* img);

extern ReductorImageCode ImageReductor_LoadJpeg(
	ImageReductor_Image* img,
	int requestWidth, int requestHeight);

public class ImageReductor
{
	[CCode(cname="ImageReductor_Debug")]
	public static extern int debug;

	[CCode(cname="PaletteCount")]
	public static extern int PaletteCount;

	[CCode(cname="Palette")]
	public static extern ColorRGBuint8* Palette;

	[CCode(cname="Palette_Custom")]
	public static extern ColorRGBuint8 Palette_Custom[256];

	[CCode(cname="HighQualityDiffuseMethod")]
	public static extern int HighQualityDiffuseMethod;

	[CCode(cname="AddNoiseLevel")]
	public static extern int AddNoiseLevel;

	// カラーモードを設定します。
	// 変換関数を呼び出す前に、必ずカラーモードを設定してください。
	public static void SetColorMode(ReductorColorMode mode, ReductorFinderMode finder, int count)
	{
		ImageReductor_SetColorMode(mode, finder, count);
	}

	// ノイズ付加モードを設定します。
	public static void SetAddNoiseLevel(int level)
	{
		AddNoiseLevel = level;
	}

	public static void Convert(ReductorReduceMode mode, Pixbuf pix, uint8[] dst, int toWidth, int toHeight)
	{
		switch (mode) {
			case ReductorReduceMode.Fast:
				ImageReductor.Fast(pix, dst, toWidth, toHeight);
				break;
			case ReductorReduceMode.Simple:
				ImageReductor.Simple(pix, dst, toWidth, toHeight);
				break;
			case ReductorReduceMode.HighQuality:
				ImageReductor.HighQuality(pix, dst, toWidth, toHeight);
				break;
		}
	}

	// 高速変換を行います。
	// 変換関数を呼び出す前に、必ずカラーモードを設定してください。
	// dst は呼び出し側が適切なサイズで確保してください。
	public static void Fast(Pixbuf pix, uint8[] dst, int toWidth, int toHeight)
	{
		ImageReductor_Fast(
			dst, toWidth, toHeight,
			pix.get_pixels(), pix.get_width(), pix.get_height(),
			pix.get_n_channels(), pix.get_rowstride());
	}

	// 単純変換を行います。
	// 変換関数を呼び出す前に、必ずカラーモードを設定してください。
	// dst は呼び出し側が適切なサイズで確保してください。
	public static void Simple(Pixbuf pix, uint8[] dst, int toWidth, int toHeight)
	{
		ImageReductor_Simple(
			dst, toWidth, toHeight,
			pix.get_pixels(), pix.get_width(), pix.get_height(),
			pix.get_n_channels(), pix.get_rowstride());
	}

	// 高品質変換を行います。
	// 変換関数を呼び出す前に、必ずカラーモードを設定してください。
	// dst は呼び出し側が適切なサイズで確保してください。
	public static void HighQuality(Pixbuf pix, uint8[] dst, int toWidth, int toHeight)
	{
		ImageReductor_HighQuality(
			dst, toWidth, toHeight,
			pix.get_pixels(), pix.get_width(), pix.get_height(),
			pix.get_n_channels(), pix.get_rowstride());
	}

	public static ImageReductor_Image* AllocImage()
	{
		return ImageReductor_AllocImage();
	}

	public static void FreeImage(ImageReductor_Image* img)
	{
		ImageReductor_FreeImage(img);
	}

	public static ReductorImageCode LoadJpeg(ImageReductor_Image* img, int requestWidth, int requestHeight)
	{
		return ImageReductor_LoadJpeg(img, requestWidth, requestHeight);
	}

	public static uint8 saturate_mul_f(uint8 a, float b)
	{
		var f = a * b;
		if (f < 0) return 0;
		if (f > 255) return 255;
		return (uint8)f;
	}

	public static void ColorFactor(float factor)
	{
		if (Palette != Palette_Custom) {
			for (int i = 0; i < PaletteCount; i++) {
				Palette_Custom[i] = Palette[i];
			}
			Palette = Palette_Custom;
		}
		for (int i = 0; i < PaletteCount; i++) {
			Palette[i].r = saturate_mul_f(Palette[i].r, factor);
			Palette[i].g = saturate_mul_f(Palette[i].g, factor);
			Palette[i].b = saturate_mul_f(Palette[i].b, factor);
		}
	}

}

