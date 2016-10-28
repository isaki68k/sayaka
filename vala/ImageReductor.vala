using Gdk;

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
	Custom,
}

[CCode(cname="ColorRGBuint8")]
public struct ColorRGBuint8
{
	uint8 r;
	uint8 g;
	uint8 b;
}

// C コードの宣言

extern void ImageReductor_SetColorMode(
	ReductorColorMode mode, int count);

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

public class ImageReductor
{
	[CCode(cname="PaletteCount")]
	public static extern int PaletteCount;

	[CCode(cname="Palette")]
	public static extern ColorRGBuint8* Palette;

	[CCode(cname="Palette_Custom")]
	public static extern ColorRGBuint8 Palette_Custom[256];

	// カラーモードを設定します。
	// 変換関数を呼び出す前に、必ずカラーモードを設定してください。
	public static void SetColorMode(ReductorColorMode mode, int count)
	{
		ImageReductor_SetColorMode(mode, count);
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
}

