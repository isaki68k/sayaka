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

// SIXEL 出力モード
// SIXEL のカラーモード値と同じにします。
public enum SixelOutputMode
{
	// 通常の SIXEL を出力します。
	Normal = 1,

	// OR モード SIXEL を出力します。
	Or = 5,
}

// リサイズモード
public enum SixelResizeMode
{
	// リサイズ処理を Pixbuf の load at_size, at_scale で行います。
	ByLoad,

	// リサイズ処理を Gdk.Pixbuf.scale_simple で行います。
	ByScaleSimple,

	// リサイズ処理を ImageReductor で行います。
	ByImageReductor,
}

public class SixelConverter
{
	public Diag diag = new Diag("SixelConverter");

	// 元画像
	public Pixbuf pix;

	// インデックスカラー画像バッファ
	public uint8[] Indexed;

	// 画像の幅と高さ。Resize すると変更されます。
	public int Width { get; private set; }
	public int Height { get; private set; }

	//////////////// 設定

	// Sixel の出力カラーモード値
	public SixelOutputMode OutputMode = SixelOutputMode.Normal;

	// Sixel にパレットを出力する場合 true
	public bool OutputPalette = true;

	// カラーモード
	public ReductorColorMode ColorMode = ReductorColorMode.Fixed256;

	// グレーカラーのときの色数。
	// グレー以外のときは無視されます。
	public int GrayCount = 256;

	// 減色モード
	public ReductorReduceMode ReduceMode = ReductorReduceMode.HighQuality;

	// リサイズモード
	public SixelResizeMode ResizeMode = SixelResizeMode.ByLoad;

	//////////////// 画像の読み込み

	public void Load(string filename, int width = 0) throws Error
	{
		if (ResizeMode == SixelResizeMode.ByLoad) {
			if (width <= 0) width = -1;
			pix = new Pixbuf.from_file_at_size(filename, width, -1);
		} else {
			pix = new Pixbuf.from_file(filename);
		}
		Width = pix.get_width();
		Height = pix.get_height();
		diag.Debug(@"filename=$(filename)");
		diag.Debug(@"Size=($(Width),$(Height))");
		diag.Debug(@"bits=$(pix.get_bits_per_sample())");
		diag.Debug(@"nCh=$(pix.get_n_channels())");
		diag.Debug(@"rowstride=$(pix.get_rowstride())");
	}

	public void LoadFromStream(InputStream stream, int width = 0) throws Error
	{
		if (ResizeMode == SixelResizeMode.ByLoad) {
			if (width <= 0) width = -1;
			pix = new Pixbuf.from_stream_at_scale(stream, width, -1, true);
		} else {
			pix = new Pixbuf.from_stream(stream);
		}
		Width = pix.get_width();
		Height = pix.get_height();
	}

	// ----- 前処理

	// インデックスカラーに変換します。
	// width: リサイズ後の幅。0 を指定すると、元画像の幅を使用します。
	// height: リサイズ後の高さ。0 を指定すると、width からアスペクト比を維持した値が計算されます。
	public void ConvertToIndexed(int width = 0, int height = 0)
	{
		// リサイズ

		diag.Debug(@"pre width=$(width) height=$(height)");
		if (width == 0) {
			Width = pix.get_width();
		} else {
			Width = width;
		}
		if (height == 0) {
			Height = pix.get_height() * Width / pix.get_width();
		} else {
			Height = height;
		}
		diag.Debug(@"post Width=$(Width) Height=$(Height)");

		if (ResizeMode == SixelResizeMode.ByScaleSimple) {
			if (Width == pix.get_width() || Height == pix.get_height()) {
				diag.Debug("no need to resize");
			} else {
				// Gdk.Pixbuf で事前リサイズする。
				// ImageReductor は減色とリサイズを同時実行できるので、
				// 事前リサイズは品質の問題が出た時のため。
				pix = pix.scale_simple(Width, Height, InterpType.BILINEAR);
				diag.Debug("scale_simple resized");
			}
		}

		Indexed = new uint8[Width * Height];

		ImageReductor.SetColorMode(ColorMode, GrayCount);
		diag.Debug(@"SetColorMode=$(ColorMode)");

		diag.Debug(@"ReduceMode=$(ReduceMode)");
		ImageReductor.Convert(ReduceMode, pix, Indexed, Width, Height);
		diag.Debug(@"Converted");
	}

	// ----- Sixel 出力

	private const string ESC = "\x1b";
	private const string DCS = ESC + "P";

	// Sixel の開始コードとパレットを文字列で返します。
	public string SixelPreamble()
	{
		StringBuilder linebuf = new StringBuilder.sized(1024);

		// Sixel 開始コード
		linebuf.append(DCS);
		linebuf.append_printf("7;%d;q\"1;1;%d;%d", OutputMode, Width, Height);

		// パレットの出力
		if (OutputPalette) {
			for (int i = 0; i < ImageReductor.PaletteCount; i++) {
				linebuf.append_printf("#%d;%d;%d;%d;%d", i, 2,
					ImageReductor.Palette[i].r * 100 / 255,
					ImageReductor.Palette[i].g * 100 / 255,
					ImageReductor.Palette[i].b * 100 / 255);
			}
		}

		return linebuf.str;
	}

#if false
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
#endif

	// OR mode で Sixel コア部分を stream に出力します。
	private void SixelToStreamCore_ORmode(FileStream stream)
	{
		unowned uint8[] p0 = Indexed;
		int w = Width;
		int h = Height;

		uint8[] sixelbuf = new uint8[w * 16 + 12];

#if false
		// パレットのビット数
		int bcnt = MyLog2(ImageReductor.PaletteCount);
//stderr.printf("bcnt=%d\n", bcnt);
#endif

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

	// Sixel コア部分を stream に出力します。
	private void SixelToStreamCore(FileStream stream)
	{
		// 030 ターゲット

		StringBuilder linebuf = new StringBuilder.sized(1024);

		unowned uint8[] p0 = Indexed;
		int w = Width;
		int h = Height;
		int src = 0;

		diag.Debug(@"PaletteCount=$(ImageReductor.PaletteCount)");

		// カラー番号ごとの、X 座標の min, max を計算する
		// short でいいよね。。
		int16[] min_x = new int16[ImageReductor.PaletteCount];
		int16[] max_x = new int16[ImageReductor.PaletteCount];

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
					for (int16 c = 0; c < ImageReductor.PaletteCount; c++) {
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
		diag.Debug("SixelToStream");

		if (ImageReductor.PaletteCount == 0) {
			diag.Error("PaletteCount == 0");
			return;
		}

		// 開始コードとかの出力。
		stream.puts(SixelPreamble());

		if (OutputMode == SixelOutputMode.Or) {
			SixelToStreamCore_ORmode(stream);
		} else {
			SixelToStreamCore(stream);
		}

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

