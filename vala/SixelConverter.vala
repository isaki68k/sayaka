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
  uint8* dst, uint8* src, int w, int h, int plane_count);

// SIXEL 出力モード
// SIXEL のカラーモード値と同じにします。
public enum SixelOutputMode
{
	// 通常の SIXEL を出力します。
	Normal = 1,

	// OR モード SIXEL を出力します。
	Or = 5,
}

// ローダモード
// 画像のロードに使うライブラリを指定します。
public enum SixelLoaderMode
{
	// gdk pixbuf を使用します。
	Gdk,

	// 個別ライブラリを使用します。
	// 現在は jpeg 画像で、libjpeg を使用します。
	// 将来的に libpng などをサポートしたときもこのフラグです。
	// 個別ライブラリが対応していないフォーマットの場合は
	// gdk pixbuf にフォールバックします。
	Lib,
}

// リサイズモード
public enum SixelResizeMode
{
	// リサイズ処理をロード時にライブラリで行います。
	ByLoad,

	// ロードは等倍で行い、その後にリサイズ処理を Gdk.Pixbuf.scale_simple で行います。
	ByScaleSimple,

	// ロードは等倍で行い、その後にリサイズ処理を ImageReductor で行います。
	ByImageReductor,
}

// リサイズ軸
// リサイズでどの軸を使うかの設定
public enum ResizeAxisMode
{
	// 幅が ResizeWidth になり、
	// 高さが ResizeHeight になるようにリサイズします。
	// ResizeWidth == 0 のときは Height と同じ動作をします。
	// ResizeHeight == 0 のときは Width と同じ動作をします。
	// ResizeWidth と ResizeHeight の両方が 0 のときは原寸大です。
	Both,

	// 幅が ResizeWidth になるように縦横比を保持してリサイズします。
	// ResizeWidth == 0 のときは原寸大です。
	Width,

	// 高さが ResizeHeight になるように縦横比を保持してリサイズします。
	// ResizeHeight == 0 のときは原寸大です。
	Height,

	// 長辺優先リサイズ
	// 原寸 Width >= Height のときは Width と同じ動作をします。
	// 原寸 Width < Height のときは Height と同じ動作をします。
	// 例:
	// 長辺を特定のサイズにしたい場合は、ResizeWidth と ResizeHeight に同じ値を設定します。
	Long,

	// 短辺優先リサイズ
	// 原寸 Width <= Height のときは Width と同じ動作をします。
	// 原寸 Width > Height のときは Height と同じ動作をします。
	Short,
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

	private ImageReductor_Image *img = null;

	//////////////// 設定

	// Sixel の出力カラーモード値
	public SixelOutputMode OutputMode = SixelOutputMode.Normal;

	// Sixel にパレットを出力する場合 true
	public bool OutputPalette = true;

	// カラーモード
	public ReductorColorMode ColorMode = ReductorColorMode.Fixed256;

	// ファインダーモード
	public ReductorFinderMode FinderMode = ReductorFinderMode.RFM_Default;

	// グレーカラーのときの色数。
	// グレー以外のときは無視されます。
	public int GrayCount = 256;

	// 減色モード
	public ReductorReduceMode ReduceMode = ReductorReduceMode.HighQuality;

	// リサイズモード
	public SixelResizeMode ResizeMode = SixelResizeMode.ByLoad;

	// ローダモード
	public SixelLoaderMode LoaderMode = SixelLoaderMode.Gdk;

	// ノイズ付加
	// ベタ塗り画像で少ない色数へ減色するとき、ノイズを付加すると画質改善できる
	public int AddNoiseLevel = 0;

	// リサイズ情報
	// リサイズで希望する幅と高さです。
	// 0 を指定すると、その情報は使用されません。
	public int ResizeWidth;
	public int ResizeHeight;

	// リサイズ処理で使用する軸
	public ResizeAxisMode ResizeAxis = ResizeAxisMode.Both;

	//////////////// 画像の読み込み

	public void Load(string filename) throws Error
	{
		diag.Debug(@"filename=$(filename)");
		diag.Debug(@"LoaderMode=$(LoaderMode)");
		diag.Debug(@"ResizeMode=$(ResizeMode)");

		if (LoaderMode == SixelLoaderMode.Lib) {
			var f = File.new_for_path(filename);
			var ps = new PeekableInputStream(f.read());
			var isLoaded = LoadJpeg(ps);
			ps.close();
			if (isLoaded) {
				LoadAfter();
				return;
			} else {
				diag.Debug("fallback to gdk");
			}
		}

		if (ResizeMode == SixelResizeMode.ByLoad) {
			int width = -1;
			int height = -1;
			CalcResizeGdkLoad(ref width, ref height);
			pix = new Pixbuf.from_file_at_size(filename, width, height);
		} else {
			pix = new Pixbuf.from_file(filename);
		}
		LoadAfter();
	}

	public void LoadFromStream(InputStream stream) throws Error
	{
		var ps = new PeekableInputStream(stream);

		diag.Debug(@"LoaderMode=$(LoaderMode)");
		diag.Debug(@"ResizeMode=$(ResizeMode)");

		if (LoaderMode == SixelLoaderMode.Lib) {
			if (LoadJpeg(ps) == true) {
				LoadAfter();
				return;
			} else {
				diag.Debug("fallback to gdk");
			}
		}

		if (ResizeMode == SixelResizeMode.ByLoad) {
			int width = -1;
			int height = -1;
			CalcResizeGdkLoad(ref width, ref height);
			pix = new Pixbuf.from_stream_at_scale(ps, width, height, true);
		} else {
			pix = new Pixbuf.from_stream(ps);
		}
		LoadAfter();
	}


	private bool LoadJpeg(PeekableInputStream ps)
	{
		try {
			uint8[] magic = new uint8[2];
			diag.Debug(@"magic.len=$(magic.length)");
			var n = ps.peek(magic);
			diag.Debug(@"LoadJpeg n=$(n)");
			if (n == magic.length) {
				diag.DebugHex("magic=", magic);
				if (magic[0] == 0xff && magic[1] == 0xd8) {
					img = ImageReductor.AllocImage();
					img.ReadCallback = img_readcallback;
					img.UserObject = ps;
					var r = ImageReductor.LoadJpeg(img, ResizeWidth, ResizeHeight, ResizeAxis);
					if (r == ReductorImageCode.RIC_OK) {
						diag.Debug(@"img.Width=$(img.Width) img.Height=$(img.Height) img.RowStride=$(img.RowStride)");
						pix = new Pixbuf.with_unowned_data(
							img.Data,
							Colorspace.RGB,
							/* has_alpha= */ false,
							8,
							img.Width,
							img.Height,
							img.RowStride,
							img_freecallback);
						return true;
					}
				}
			} else {
				diag.Warn(@"LoadJpeg n=$(n)");
			}
		} catch {
		}
		return false;
	}

	private void LoadAfter()
	{
		Width = pix.get_width();
		Height = pix.get_height();
		diag.Debug(@"Size=($(Width),$(Height))");
		diag.Debug(@"bits=$(pix.get_bits_per_sample())");
		diag.Debug(@"nCh=$(pix.get_n_channels())");
		diag.Debug(@"rowstride=$(pix.get_rowstride())");
	}

	static int img_readcallback(ImageReductor_Image *img)
	{
		//gDiag.Debug("img_readcallback");
		unowned InputStream? s = (InputStream) img.UserObject;
		try {
			var n = (int) s.read(img.ReadBuffer);
			//gDiag.Debug(@"read len=$(n)");
			return n;
		} catch {
			return 0;
		}
	}

	private void img_freecallback(void *pixels)
	{
		if (img != null) {
			ImageReductor.FreeImage(img);
		}
	}

	// ----- リサイズ計算

	// Gdk Load のとき、scale に渡す幅と高さを計算する
	private void CalcResizeGdkLoad(ref int width, ref int height)
	{
		// gdk では -1 が原寸基準
		width = -1;
		height = -1;
		switch (ResizeAxis) {
		 case ResizeAxisMode.Both:
			if (ResizeWidth > 0) {
				width = ResizeWidth;
			}
			if (ResizeHeight > 0) {
				height = ResizeHeight;
			}
			break;
		 case ResizeAxisMode.Width:
			width = ResizeWidth;
			break;
		 case ResizeAxisMode.Height:
			height = ResizeHeight;
			break;
		 case ResizeAxisMode.Long:
		 case ResizeAxisMode.Short:
			// Long, Short は ByLoad では処理できない
			break;
		}

		if (width <= 0) width = -1;
		if (height <= 0) height = -1;
	}

	// Load 時以外の リサイズ計算
	private void CalcResize(ref int width, ref int height)
	{
		var ra = ResizeAxis;

		// 条件を丸めていく
		switch (ra) {
		 case ResizeAxisMode.Both:
			if (ResizeWidth == 0) {
				ra = ResizeAxisMode.Height;
			} else if (ResizeHeight == 0) {
				ra = ResizeAxisMode.Width;
			}
			break;

		 case ResizeAxisMode.Long:
			if (Width >= Height) {
				ra = ResizeAxisMode.Width;
			} else {
				ra = ResizeAxisMode.Height;
			}
			break;

		 case ResizeAxisMode.Short:
			if (Width <= Height) {
				ra = ResizeAxisMode.Width;
			} else {
				ra = ResizeAxisMode.Height;
			}
			break;
		}

		// 確定したので計算
		switch (ra) {
		 case ResizeAxisMode.Both:
			width = ResizeWidth;
			height = ResizeHeight;
			break;
		 case ResizeAxisMode.Width:
			if (ResizeWidth > 0) {
				width = ResizeWidth;
			} else {
				width = Width;
			}
			height = Height * width / Width;
			break;
		 case ResizeAxisMode.Height:
			if (ResizeHeight > 0) {
				height = ResizeHeight;
			} else {
				height = Height;
			}
			width = Width * height / Height;
			break;
		}
	}

	// ----- 前処理

	// インデックスカラーに変換します。
	public void ConvertToIndexed()
	{
		// リサイズ

		int width = 0;
		int height = 0;
		CalcResize(ref width, ref height);

		diag.Debug(@"resize to Width=$(width) Height=$(height)");

		if (ResizeMode == SixelResizeMode.ByScaleSimple) {
			if (width == Width || height == Height) {
				diag.Debug("no need to resize");
			} else {
				// Gdk.Pixbuf で事前リサイズする。
				// ImageReductor は減色とリサイズを同時実行できるので、
				// 事前リサイズは品質の問題が出た時のため。
				pix = pix.scale_simple(width, height, InterpType.BILINEAR);
				diag.Debug("scale_simple resized");
			}
		}

		Width = width;
		Height = height;

		Indexed = new uint8[Width * Height];

		ImageReductor.SetColorMode(ColorMode, FinderMode, GrayCount);
		diag.Debug(@"SetColorMode=$(ColorMode), $(FinderMode), $(GrayCount)");

		diag.Debug(@"SetAddNoiseLevel=$(AddNoiseLevel)");
		ImageReductor.SetAddNoiseLevel(AddNoiseLevel);

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

	// OR mode で Sixel コア部分を stream に出力します。
	private void SixelToStreamCore_ORmode(FileStream stream)
	{
		unowned uint8[] p0 = Indexed;
		int w = Width;
		int h = Height;

		// パレットのビット数
		int bcnt = MyLog2(ImageReductor.PaletteCount);
//stderr.printf("bcnt=%d\n", bcnt);

		uint8[] sixelbuf = new uint8[(w + 5) * bcnt];

		uint8* p = p0;
		int y;
		// 一つ手前の SIXEL 行まで変換
		for (y = 0; y < h - 6; y += 6) {

			int len = sixel_image_to_sixel_h6_ormode(
				sixelbuf, p, w, 6, bcnt);
			stream.write(sixelbuf[0 : len]);
			stream.flush();
			p += w * 6;
		}
		// 最終 SIXEL 行を変換
		int len = sixel_image_to_sixel_h6_ormode(
			sixelbuf, p, w, h - y, bcnt);
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

