/*
 * Copyright (C) 2015 Y.Sugahara (moveccr)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

using Gdk;
using System.OS;

extern int sixel_image_to_sixel_h6_ormode(
  uint8* dst, uint8* src, int w, int h);

// SIXEL カラーモード
public enum SixelColorMode
{
	FixedX68k,
	X68k,
	Gray,
	GrayMean,
	Fixed8,
	FixedANSI16,
	FixedWindows16,
	Fixed256,
	// カスタムカラー。外部でカラーパレットを生成します。
	Custom,
}

// SIXEL 減色モード
public enum SixelReduceMode
{
	// 速度優先法
	Fast,

	// 2次元誤差分散法
	Diffuse,

	// 単純一致法
	Simple,
}

// SIXEL 出力モード
public enum SixelOutputMode
{
	// 通常の SIXEL を出力します。
	Normal = 1,

	// OR モード SIXEL を出力します。
	Or = 5,
}

public class SixelConverter
{
	private Diag diag = new Diag("SixelConverter");

	private ImageReductor rd = new ImageReductor();


	// 元画像
	public Pixbuf pix;

	// パレット
	public uint8[,] Palette;
	public int PaletteCount;

	// インデックスカラー画像バッファ
	public uint8[] Indexed;


	// 画像の幅と高さ。Resize すると変更されます。
	public int Width { get; private set; }
	public int Height { get; private set; }

	//////////////// 設定

	// Sixel の出力カラーモード値
	// 1 : 通常
	// 5 : X68k OR-ed Mode
	public SixelOutputMode OutputMode = SixelOutputMode.Normal;

	// Sixel にパレットを出力する場合 true
	public bool OutputPalette = true;

	// カラーモード
	public SixelColorMode ColorMode = SixelColorMode.Fixed256;

	// 減色モード
	public SixelReduceMode ReduceMode = SixelReduceMode.Diffuse;


	//////////////// 画像の読み込み

	public void Load(string filename) throws Error
	{
		pix = new Pixbuf.from_file(filename);
		Width = pix.get_width();
		Height = pix.get_height();
		diag.Debug(@"filename=$(filename)");
		diag.Debug(@"Size=($(Width),$(Height))");
		diag.Debug(@"bits=$(pix.get_bits_per_sample())");
		diag.Debug(@"nCh=$(pix.get_n_channels())");
		diag.Debug(@"rowstride=$(pix.get_rowstride())");
	}

	public void LoadFromStream(InputStream stream) throws Error
	{
		pix = new Pixbuf.from_stream(stream);
		Width = pix.get_width();
		Height = pix.get_height();
	}

	// ----- 前処理

	// 必要に応じてリサイズします。
	public void ResizeIfNeeded(int width = 0, int height = 0)
	{
		if (width == 0) {
			Width = pix.get_width();
		}
		if (height == 0) {
			Height = pix.get_height() * Width / pix.get_width();
		}

		// Fast はリサイズを減色と同時に行うので
		// プロパティだけ変更して、画像処理は行わない。
		if (ReduceMode == SixelReduceMode.Fast) return;

		Resize(Width, Height);
	}

	public void ResizeByWidth(int width)
	{
		int h = Height * width / Width;
		Resize(width, h);
	}	

	public void Resize(int width, int height)
	{
		pix = pix.scale_simple(width, height, InterpType.BILINEAR);
		Width = pix.get_width();
		Height = pix.get_height();
	}

	// ----- パレットの設定

	// モードを指定してから呼び出すと、よしなにパレットを設定します。
	public void SetPalette()
	{
		switch (ColorMode) {
			case SixelColorMode.FixedX68k:
				SetPaletteFixedX68k();
				ColorFinder = FindFixedX68k;
				break;

			case SixelColorMode.X68k:
				SetPaletteX68k();
				ColorFinder = FindCustom16;
				break;

			case SixelColorMode.Gray:
				SetPaletteGray(256);
				ColorFinder = FindGray;
				break;

			case SixelColorMode.GrayMean:
				SetPaletteGray(256);
				ColorFinder = FindGrayMean;
				break;

			case SixelColorMode.Fixed8:
				SetPaletteFixed8();
				ColorFinder = FindFixed8;
				break;

			case SixelColorMode.FixedANSI16:
				SetPaletteFixedANSI16();
				ColorFinder = FindFixedANSI16;
				break;

			case SixelColorMode.FixedWindows16:
				SetPaletteFixedWindows16();
				ColorFinder = FindFixedWindows16;
				break;

			case SixelColorMode.Fixed256:
			default:
				SetPaletteFixed256();
				ColorFinder = FindFixed256;
				break;

			case SixelColorMode.Custom:
				PaletteCount = 256;
				ColorFinder = FindCustom;
				break;
		}
	}

	// グレースケールパレットを生成します。
	public void SetPaletteGray(int count)
	{
		for (int i = 0; i < count; i++) {
			uint8 c = (uint8)(i * 255 / (count - 1));
			Palette[i, 0] = Palette[i, 1] = Palette[i, 2] = c;
		}
		PaletteCount = count;
	}

	// デジタル8色の固定パレットを生成します。
	public void SetPaletteFixed8()
	{
		for (int i = 0; i < 8; i++) {
			uint8 R = (uint8)((i     ) & 0x01) * 255;
			uint8 G = (uint8)((i >> 1) & 0x01) * 255;
			uint8 B = (uint8)((i >> 2) & 0x01) * 255;

			Palette[i, 0] = R;
			Palette[i, 1] = G;
			Palette[i, 2] = B;
		}
		PaletteCount = 8;
	}

	// x68k 16 色の固定パレットを生成します。
	public void SetPaletteFixedX68k()
	{
		for (int i = 0; i < 8; i++) {
			uint8 R = (uint8)((i     ) & 0x01) * 255;
			uint8 G = (uint8)((i >> 1) & 0x01) * 255;
			uint8 B = (uint8)((i >> 2) & 0x01) * 255;

			Palette[i, 0] = R;
			Palette[i, 1] = G;
			Palette[i, 2] = B;
		}
		for (int i = 0; i < 8; i++) {
			uint8 R = (uint8)((i     ) & 0x01) * 128;
			uint8 G = (uint8)((i >> 1) & 0x01) * 128;
			uint8 B = (uint8)((i >> 2) & 0x01) * 128;

			if (i == 0) {
				R = G = B = 192;
			}
			Palette[i + 8, 0] = R;
			Palette[i + 8, 1] = G;
			Palette[i + 8, 2] = B;
		}
		PaletteCount = 16;
	}

	// x68k 16 色のパレットをシステムから取得して生成します。
	public void SetPaletteX68k()
	{
		for (var i = 0; i < 16; i++) {
			int val;
			int r, g, b;
			var sname = "hw.ite.tpalette%X".printf(i);
			if (sysctl.getbyname_int(sname, out val) == -1) {
				// エラーになったらとりあえず内蔵固定16色
//stderr.printf("sysctl error\n");
				SetPaletteFixedX68k();
				return;
			}
			// x68k のパレットは GGGG_GRRR_RRBB_BBBI
			g = (((val >> 11) & 0x1f) << 1) | (val & 1);
			r = (((val >>  6) & 0x1f) << 1) | (val & 1);
			b = (((val >>  1) & 0x1f) << 1) | (val & 1);

			Palette[i, 0] = (uint8)(r * 255 / 63);
			Palette[i, 1] = (uint8)(g * 255 / 63);
			Palette[i, 2] = (uint8)(b * 255 / 63);
		}
		PaletteCount = 16;
	}

	// ANSI 16 色の固定パレットを生成します。
	public void SetPaletteFixedANSI16()
	{
		for (int i = 0; i < 16; i++) {
			uint8 R = (uint8)((i     ) & 0x01);
			uint8 G = (uint8)((i >> 1) & 0x01);
			uint8 B = (uint8)((i >> 2) & 0x01);
			uint8 I = (uint8)((i >> 3) & 0x01);

			// ANSI 16 色といっても色実体は実装依存らしい。

			R = R * 170 + I * 85;
			G = G * 170 + I * 85;
			B = B * 170 + I * 85;

			Palette[i, 0] = R;
			Palette[i, 1] = G;
			Palette[i, 2] = B;
		}
		PaletteCount = 16;
	}

	// Windows XP CMD 16 色の固定パレットを生成します。
	public void SetPaletteFixedWindows16()
	{
		for (int i = 0; i < 16; i++) {
			uint8 R = (uint8)((i     ) & 0x01);
			uint8 G = (uint8)((i >> 1) & 0x01);
			uint8 B = (uint8)((i >> 2) & 0x01);
			uint8 I = (uint8)((i >> 3) & 0x01);

			// Windows XP CMD カラーテーブル

			R = R * 128 + I * 127;
			G = G * 128 + I * 127;
			B = B * 128 + I * 127;
			if (i == 7) {
				R = G = B = 192;
			} else if (i == 8) {
				R = G = B = 128;
			}

			Palette[i, 0] = R;
			Palette[i, 1] = G;
			Palette[i, 2] = B;
		}
		PaletteCount = 16;
	}

	// R3,G3,B2 bit の256色固定パレットを生成します。
	public void SetPaletteFixed256()
	{
		for (int i = 0; i < 256; i++) {
			Palette[i, 0] = (uint8)(((i >> 5) & 0x07) * 255 / 7);
			Palette[i, 1] = (uint8)(((i >> 2) & 0x07) * 255 / 7);
			Palette[i, 2] = (uint8)(((i     ) & 0x03) * 255 / 3);
		}
		PaletteCount = 256;
	}

	// ----- カラーパレットから色をさがす

	// Find* のデリゲートです。
	public delegate uint8 FindFunc(uint8 r, uint8 g, uint8 b);

	// Find* のデリゲートを保持します。
	// パレットを選択したときに設定します。
	private FindFunc ColorFinder;

	// グレースケールパレット時に、最も近いパレット番号を返します。
	public uint8 FindGray(uint8 r, uint8 g, uint8 b)
	{
		// NTSC 輝度
		return (uint8)(((int)r * 76 + (int)g * 153 + (int)b * 26) * (PaletteCount - 1) / 255 / 255);
	}

	// グレースケールパレット時に、RGB 平均で最も近いパレット番号を返します。
	public uint8 FindGrayMean(uint8 r, uint8 g, uint8 b)
	{
		return (uint8)(((int)r + (int)g + (int)b) * (PaletteCount - 1) / 3 / 255);
	}

	// 固定8色時に、最も近いパレット番号を返します。
	public uint8 FindFixed8(uint8 r, uint8 g, uint8 b)
	{
		uint8 R = (uint8)(r >= 128);
		uint8 G = (uint8)(g >= 128);
		uint8 B = (uint8)(b >= 128);
		return R + (G << 1) + (B << 2);
	}

	// X68k 固定16色時に、最も近いパレット番号を返します。
	public uint8 FindFixedX68k(uint8 r, uint8 g, uint8 b)
	{
		int I = (int)r + (int)g + (int)b;
		int R;
		int G;
		int B;
		if (I >= 160 * 3) {
			R = r >= 160;
			G = g >= 160;
			B = b >= 160;
			if (R == G && G == B) {
				if (r > 224) {
					return 7;
				} else {
					return 8;
				}
			}
			return R + (G << 1) + (B << 2);
		} else {
			R = r >= 80;
			G = g >= 80;
			B = b >= 80;
			if (R == G && G == B) {
				if (r > 64) {
					return 15;
				} else {
					return 0;
				}
			}
			return R + (G << 1) + (B << 2) | 8;
		}
	}

	// ANSI 固定16色時に、最も近いパレット番号を返します。
	public uint8 FindFixedANSI16(uint8 r, uint8 g, uint8 b)
	{
		// TODO: 最適実装
		return FindCustom(r, g, b);
	}

	// Windows 固定16色時に、最も近いパレット番号を返します。
	public uint8 FindFixedWindows16(uint8 r, uint8 g, uint8 b)
	{
		// TODO: 最適実装
		return FindCustom(r, g, b);
	}

	// 固定256色時に、最も近いパレット番号を返します。
	public uint8 FindFixed256(uint8 r, uint8 g, uint8 b)
	{
		// 0 1 2 3 4 5 6 7 8 9 a b c d e f
		// 0 0 1 1 2 2 3 3 4 4 5 5 6 6 7 7
		uint8 R = r >> 5;
		uint8 G = g >> 5;
		uint8 B = b >> 6;
		return (R << 5) + (G << 2) + B;
	}

	// カスタム 16 色パレット時に、最も近いパレット番号を返します。
	public uint8 FindCustom16(uint8 r, uint8 g, uint8 b)
	{
		// TODO: 最適実装
		return FindCustom(r, g, b);
	}

	// カスタムパレット時に、最も近いパレット番号を返します。
	public uint8 FindCustom(uint8 r, uint8 g, uint8 b)
	{
		// RGB の各色の距離の和が最小、にしてある。
		// YCC で判断したほうが良好なのは知ってるけど、そこまで必要じゃない。
		// とおもったけどやっぱり品質わるいので色差も考えていく。

		// 色差情報を重みにしていく。
		int K1 = ((int)r*2 - (int)g - (int)b); if (K1 < 1) K1 = 1; if (K1 > 8) K1 = 4;
		int K2 = ((int)g*2 - (int)r - (int)b); if (K2 < 1) K2 = 1; if (K2 > 8) K2 = 4;
		int K3 = ((int)b*2 - (int)r - (int)g); if (K3 < 1) K3 = 1; if (K3 > 8) K3 = 4;
		uint8 rv = 0;
		int min_d = int.MAX;
		for (int i = 0; i < PaletteCount; i++) {
			int dR = (int)Palette[i, 0] - (int)r;
			int dG = (int)Palette[i, 1] - (int)g;
			int dB = (int)Palette[i, 2] - (int)b;
			int d = dR.abs() * K1 + dG.abs() * K2 + dB.abs() * K3;

			if (d < min_d) {
				rv = (uint8)i;
				min_d = d;
				if (d == 0) break;
			}
		}
		return rv;
	}

	// ----- カラー変換 (減色)

	public void ReduceColor()
	{
		if (ColorFinder == NULL) {
			stderr.printf("PROGERR Color == NULL\n");
			return;
		}

		switch (ReduceMode) {
			case SixelReduceMode.Fast:
				FastReduce();
				break;

			case SixelReduceMode.Diffuse:
				DiffuseReduce();
				break;

			case SixelReduceMode.Simple:
				SimpleReduce();
				break;
		}
	}

	// ----- 単純減色法
	// ----- 要は誤差拡散法ではなく、当該ピクセルのみの値で減色するアルゴリズム。

	// 飽和加算
	private uint8 satulate_add(uint8 a, int16 b)
	{
		int16 rv = (int16)a + b;
		if (rv > 255) rv = 255;
		if (rv < 0) rv = 0;
		return (uint8)rv;
	}

	// 単純減色法を適用します。
	// ColorFinder が設定されていることを保証してください。
	public void SimpleReduce()
	{
		unowned uint8[] p0 = pix.get_pixels();
		int w = Width;
		int h = Height;
		Indexed = new uint8[w * h];
		int nch = pix.get_n_channels();
		int ybase = 0;
		int dst = 0;

		for (int y = 0; y < h; y++) {
			for (int x = 0; x < w; x++) {
				uint8* psrc = &p0[ybase + x * nch];
				uint8 r = psrc[0];
				uint8 g = psrc[1];
				uint8 b = psrc[2];
				// パレット番号の列を書き込み
				Indexed[dst++] = ColorFinder(r, g, b);
			}
			ybase += pix.get_rowstride();
		}
	}

	public int16 DiffuseMultiplier = 1;
	public int16 DiffuseDivisor = 3;

	private void DiffusePixel(uint8 r, uint8 g, uint8 b, uint8* ptr, uint8 c)
	{
		ptr[0] = satulate_add(ptr[0], ((int16)r - Palette[c, 0]) * DiffuseMultiplier / DiffuseDivisor);
		ptr[1] = satulate_add(ptr[1], ((int16)g - Palette[c, 1]) * DiffuseMultiplier / DiffuseDivisor);
		ptr[2] = satulate_add(ptr[2], ((int16)b - Palette[c, 2]) * DiffuseMultiplier / DiffuseDivisor);
	}

	// 二次元誤差分散法を適用します。
	// ColorFinder が設定されていることを保証してください。
	public void DiffuseReduce()
	{
		unowned uint8[] p0 = pix.get_pixels();
		int w = Width;
		int h = Height;
		Indexed = new uint8[w * h];
		int nch = pix.get_n_channels();
		int stride = pix.get_rowstride();
		int ybase = 0;
		int dst = 0;

		for (int y = 0; y < h; y++) {
			for (int x = 0; x < w; x++) {
				uint8* psrc = &p0[ybase + x * nch];
				uint8 r = psrc[0];
				uint8 g = psrc[1];
				uint8 b = psrc[2];

				uint8 C = ColorFinder(r, g, b);
				// パレット番号の列を書き込み
				Indexed[dst++] = C;

				if (x < w - 1) {
					// 右のピクセルに誤差を分散
					DiffusePixel(r, g, b, &p0[ybase + (x + 1) * nch], C);
				}
				if (y < h - 1) {
					// 下のピクセルに誤差を分散
					DiffusePixel(r, g, b, &p0[ybase + stride + x * nch], C);
				}
				if (x < w - 1 && y < h - 1) {
					// 右下のピクセルに誤差を分散
					DiffusePixel(r, g, b, &p0[ybase + stride + (x + 1) * nch], C);
				}
			}
			ybase += stride;
		}
	}

	// 高速変換を適用します。
	// リサイズも同時に実行します。
	public void FastReduce()
	{
		unowned uint8[] p0 = pix.get_pixels();
		int w = pix.get_width();
		int h = pix.get_height();
		Indexed = new uint8[Width * Height];
		int nch = pix.get_n_channels();
		int stride = pix.get_rowstride();

		imagereductor_fast(output, Width, Height, p0, w, h, nch, stride, Palette, ColorFinder);
	}


	// ----- Sixel 出力

	private const string ESC = "\x1b";
	private const string DCS = ESC + "P";

	// デバッグ用に、pixbuf を16進テキスト出力。
	public void RawHexToStream(FileStream stream)
	{
		unowned uint8[] p0 = pix.get_pixels();
		int w = Width;
		int h = Height;
		int ybase = 0;
		int stride = pix.get_rowstride();

		// ナマの16進値 
		for (int y = 0; y < h; y++) {
			for (int x = 0; x < w; x++) {
				stream.printf("%02x%02x%02x ",
					p0[ybase + x * 3 + 0],
					p0[ybase + x * 3 + 1],
					p0[ybase + x * 3 + 2]);
			}
			ybase += stride;
			stream.putc('\n');
		}
	}

	// デバッグ用に、4ビットアスキーアート風出力。
	public void MonoCharToStream(FileStream stream)
	{
		unowned uint8[] p0 = pix.get_pixels();
		int w = Width;
		int h = Height;
		int src = 0;

		// モノクロキャラクタ
		for (int y = 0; y < h; y++) {
			for (int x = 0; x < w; x++) {
				uint8 I = p0[src++];
				// 4bit
				stream.putc(" .:iltoeUO9B@%#M"[I >> 4]);
			}
			stream.putc('\n');
		}
	}

	// ----- Sixel 出力

	// Sixel の開始コードとパレットを文字列で返します。
	public string SixelPreamble()
	{
		StringBuilder linebuf = new StringBuilder.sized(1024);

		// Sixel 開始コード
		linebuf.append(DCS);
		linebuf.append_printf("7;%d;q\"1;1;%d;%d", OutputColorMode, Width, Height);

		// パレットの出力
		if (OutputPalette) {
			for (int i = 0; i < PaletteCount; i++) {
				linebuf.append_printf("#%d;%d;%d;%d;%d", i, 2,
					Palette[i, 0] * 100 / 255,
					Palette[i, 1] * 100 / 255,
					Palette[i, 2] * 100 / 255);
			}
		}

		return linebuf.str;
	}

	// Sixel コア部分を stream に出力します。
	// 一番効率の良くない方法。
	private void SixelToStreamCore_v1(FileStream stream)
	{
		StringBuilder linebuf = new StringBuilder.sized(1024);

		unowned uint8[] p0 = IndexedBuffer;
		int w = Width;
		int h = Height;
		int src = 0;

		// 一番効率のよくない方法。
		for (int y = 0; y < h; y++) {
			for (int x = 0; x < w; x++) {
				int I = p0[src++];
				linebuf.append_printf("#%d", I);
				linebuf.append(SixelRepunit(1, 1 << (y % 6)));
			}
			if (y % 6 == 5) {
				linebuf.append_c('-');
			} else {
				linebuf.append_c('$');
			}
			stream.puts(linebuf.str);
			linebuf.len = 0;
			stream.flush();
		}
	}

	// Sixel コア部分を stream に出力します。
	// おおきな(といっても今どきなら小さい)バッファが要るが性能はまあまあ良い
	private void SixelToStreamCore_v2(FileStream stream)
	{
		StringBuilder linebuf = new StringBuilder.sized(1024);

		unowned uint8[] p0 = IndexedBuffer;
		int w = Width;
		int h = Height;

		// おおきな(といっても今どきなら小さい)バッファが要るが性能はまあまあ良い

		uint8[,] buf = new uint8[PaletteCount, w + 1];
		// clear
		for (int i = 0; i <  buf.length[0]; i++) {
			for (int j = 0; j < buf.length[1]; j++) {
				buf[i, j] = 0;
			}
		}

		for (int y = 0; y < h; y += 6) {

			int max_dy = y + 6;
			if (max_dy > h) max_dy = h;
			uint8 m = 1;
			for (int dy = y; dy < max_dy; dy++) {
				for (int x = 0; x < w; x++) {
					uint8 I = p0[dy * w + x];
					buf[(int)I, x] |= m;
					// 存在チェック
					buf[(int)I, w] = 1;
				}
				m <<= 1;
			}
			for (int c = 0; c < PaletteCount; c++) {
				if (buf[c, w] == 0) continue;
				linebuf.append_printf("#%d", c);
				int n = 1;
				uint8 t = buf[c, 0];
				for (int x = 1; x < w; x++) {
					if (t != buf[c, x]) {
						linebuf.append(SixelRepunit(n, t));
						n = 1;
						t = buf[c, x];
					} else {
						n++;
					}
				}
				if (t != 0) {
					linebuf.append(SixelRepunit(n, t));
				}

				if (c != PaletteCount - 1) {
					linebuf.append_c('$');
				}

				// clear
				for (int i = 0; i < buf.length[1]; i++) {
					buf[c, i] = 0;
				}
			}
			linebuf.append_c('-');

			stream.puts(linebuf.str);
			linebuf.len = 0;
			stream.flush();
		}
	}


	// 切り上げする整数の log2
	private int MyLog2(int n)
	{
		for (int i = 0; i < 8; i++) {
			if (n <= (1 << i)) {
				return i;
			}
		}
		return 8;
	}

#if false
	// OR-ed Mode で Sixel コア部分を stream に出力します。
	private void SixelToStreamCore_ORedMode(FileStream stream)
	{
		StringBuilder linebuf = new StringBuilder.sized(1024);

		unowned uint8[] p0 = IndexedBuffer;
		int w = Width;
		int h = Height;

		// パレットのビット数
		int bcnt = MyLog2(PaletteCount);
stderr.printf("bcnt=%d\n", bcnt);
		uint8[] buf = new uint8[w];

		for (int y = 0; y < h; y += 6) {

			int max_dy = y + 6;
			if (max_dy > h) max_dy = h;

			uint8 mC = 1;	// カラーマスク
			for (int b = 0; b < bcnt; b++) {
				uint8 mY = 1;	// Y マスク

				Memory.@set(buf, 0, buf.length);

				for (int dy = y; dy < max_dy; dy++) {
					for (int x = 0; x < w; x++) {
						uint8 I = p0[dy * w + x];
						if ((I & mC) != 0) {
							buf[x] |= mY;
						}
					}
					mY <<= 1;
				}

				// TODO: 書き込みが無い時のcontinue 条件

				linebuf.append_printf("#%d", mC);
				int n = 1;
				uint8 t = buf[0];
				for (int x = 1; x < w; x++) {
					if (t != buf[x]) {
						linebuf.append(SixelRepunit(n, t));
						n = 1;
						t = buf[x];
					} else {
						n++;
					}
				}
				if (t != 0) {
					linebuf.append(SixelRepunit(n, t));
				}
				linebuf.append_c('$');

				mC <<= 1;
			}

			linebuf.append_c('-');

			stream.puts(linebuf.str);
			linebuf.len = 0;
			stream.flush();
		}
	}

#else
	// OR-ed Mode で Sixel コア部分を stream に出力します。
	private void SixelToStreamCore_ORedMode(FileStream stream)
	{
		StringBuilder linebuf = new StringBuilder.sized(1024);

		unowned uint8[] p0 = IndexedBuffer;
		int w = Width;
		int h = Height;

		uint8[] sixelbuf = new uint8[w * 16 + 12];

		// パレットのビット数
		int bcnt = MyLog2(PaletteCount);
stderr.printf("bcnt=%d\n", bcnt);

		uint8* p = p0;
		int y;
		for (y = 0; y < h - 6; y += 6) {

			int len = sixel_image_to_sixel_h6_ormode(
				sixelbuf, p, w, 6);
			stream.write(sixelbuf[0 : len]);
			stream.flush();
			p += w * 6;
		}
		int len = sixel_image_to_sixel_h6_ormode(
			sixelbuf, p, w, h - y);
		stream.write(sixelbuf[0 : len]);
		stream.flush();
	}
#endif

	// Sixel コア部分を stream に出力します。
	private void SixelToStreamCore_v3(FileStream stream)
	{
		// 030 ターゲット

		StringBuilder linebuf = new StringBuilder.sized(1024);

		unowned uint8[] p0 = IndexedBuffer;
		int w = Width;
		int h = Height;
		int src = 0;

		// カラー番号ごとの、X 座標の min, max を計算する
		// short でいいよね。。
		int16[] min_x = new int16[PaletteCount];
		int16[] max_x = new int16[PaletteCount];

		for (int16 y = 0; y < h; y += 6) {
			src = y * w;

			// 配列をクリアするいい方法を知らない
			for (int i = 0; i < min_x.length; i++) min_x[i] = -1;
			for (int i = 0; i < max_x.length; i++) max_x[i] = 0;

			// h が 6 の倍数でないときには溢れてしまうので、上界を計算する。
			int16 max_dy = 6;
			if (y + max_dy > h) max_dy = (int16)(h - y);

			// 各カラーの X 座標範囲を計算する
			for (int16 dy = 0; dy < max_dy; dy++) {
				for (int16 x = 0; x < w; x++) {
					uint8 I = p0[src++];
					if (min_x[I] < 0 || min_x[I] > x) min_x[I] = x;
					if (max_x[I] < x) max_x[I] = x;
				}
			}


			do {
				// 出力するべきカラーがなくなるまでのループ
//				Diag.DEBUG("do1");
				int16 mx = -1;

				do {
					// 1行の出力で出力できるカラーのループ
//					Diag.DEBUG("do2");

					uint8 min_color = 0;
					int16 min = int16.MAX;

					// min_x から、mx より大きいもののうち最小のカラーを探して、塗っていく
					for (int16 c = 0; c < PaletteCount; c++) {
						if (mx < min_x[c] && min_x[c] < min) {
							min_color = (uint8)c;
							min = min_x[c];
						}
					}
					// なければ抜ける
					if (min_x[min_color] <= mx) break;

//stderr.printf("min_color=%d min_x=%d max_x=%d mx=%d\n", min_color, min_x[min_color], max_x[min_color], mx);

					// Sixel に色コードを出力
					linebuf.append_printf("#%d", min_color);

					// 相対 X シーク処理
					int16 space = min_x[min_color] - (mx + 1);
					if (space > 0) {
						linebuf.append(SixelRepunit(space, 0));
					}

					// パターンが変わったら、それまでのパターンを出していくアルゴリズム
					uint8 prev_t = 0;
					int16 n = 0;
					for (int16 x = min_x[min_color]; x <= max_x[min_color]; x++) {
						uint8 t = 0;
						for (int16 dy = 0; dy < max_dy; dy++) {
							uint8 I = p0[(y + dy) * w + x];
							if (I == min_color) {
								t |= 1 << dy;
							}
						}

						if (prev_t != t) {
							if (n > 0) {
								linebuf.append(SixelRepunit(n, prev_t));
							}
							prev_t = t;
							n = 1;
						} else {
							n++;
						}
					}
					// 最後のパターン
					if (prev_t != 0 && n > 0) {
						linebuf.append(SixelRepunit(n, prev_t));
					}

					// X 位置を更新
					mx = max_x[min_color];
					// 済んだ印
					min_x[min_color] = -1;

				} while (true);

				linebuf.append_c('$');

				// 最後までやったら抜ける
				if (mx == -1) break;

			} while (true);

			linebuf.append_c('-');

			stream.puts(linebuf.str);
			linebuf.len = 0;
			stream.flush();
		}
	}


	// Sixel を stream に出力します。
	public void SixelToStream(FileStream stream)
	{
		// 開始コードとかの出力。
		stream.puts(SixelPreamble());

#if false
		SixelToStreamCore_v1(stream);
#elif false
		SixelToStreamCore_v2(stream);
#else
		if (OutputColorMode == 5) {
			SixelToStreamCore_ORedMode(stream);
		} else {
			SixelToStreamCore_v3(stream);
		}
#endif

		stream.puts(SixelPostamble());
		stream.flush();
	}

	// Sixel の終了コードを文字列で返します。
	private string SixelPostamble()
	{
		return ESC + "\\";
	}

	// 繰り返しのコードを考慮して、Sixel パターン文字を返します。
	private static string SixelRepunit(int n, uint8 ptn)
	{
		if (n >= 4) {
			return "!%d%c".printf(n, ptn + 0x3f);
		} else {
			return string.nfill(n, ptn + 0x3f);
		}
	}
}

