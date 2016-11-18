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

// リサイズモード
public enum SixelResizeMode
{
	// リサイズ処理を Pixbuf の load at_size, at_scale で行います。
	ByLoad,

	// リサイズ処理を Gdk.Pixbuf.scale_simple で行います。
	ByScaleSimple,

	// リサイズ処理を ImageReductor で行います。
	ByImageReductor,

	ByLibJpeg,
}

public class PeekableInputStream
 : InputStream
{
	private InputStream target;
	private uint8[] peekbuffer;

	public PeekableInputStream(InputStream baseStream)
	{
		target = baseStream;
	}

	public override bool close(Cancellable? cancellable = null) throws IOError
	{
		target.close();
		return true;
	}

	public override ssize_t read(uint8[] buffer, Cancellable? cancellable = null) throws IOError
	{
		if (peekbuffer.length > 0 && peekbuffer.length < buffer.length) {
			var n = peekbuffer.length;
			Memory.copy(buffer, peekbuffer, peekbuffer.length);
			peekbuffer.length = 0;
			return n;
		}
		return (ssize_t)target.read(buffer);
	}

	public ssize_t peek(uint8[] buffer) throws IOError
	{
		peekbuffer = new uint8[buffer.length];
		var n = target.read(peekbuffer);
		Memory.copy(buffer, peekbuffer, buffer.length);
		return n;
	}
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

	// ノイズ付加
	// ベタ塗り画像で少ない色数へ減色するとき、ノイズを付加すると画質改善できる
	public int AddNoiseLevel = 0;

	//////////////// 画像の読み込み

	public void Load(string filename, int width = 0) throws Error
	{
		diag.Debug(@"filename=$(filename)");
		diag.Debug(@"ResizeMode=$(ResizeMode)");
		if (ResizeMode == SixelResizeMode.ByLibJpeg) {
			var f = File.new_for_path(filename);
			var ps = new PeekableInputStream(f.read());
			var isLoaded = LoadJpeg(ps, width);
			ps.close();
			if (isLoaded == false) {
				diag.Debug("fallback to gdk");
				if (width <= 0) width = -1;
				pix = new Pixbuf.from_file_at_size(filename, width, -1);
			}
		} else {
			if (ResizeMode == SixelResizeMode.ByLoad) {
				if (width <= 0) width = -1;
				pix = new Pixbuf.from_file_at_size(filename, width, -1);
			} else {
				pix = new Pixbuf.from_file(filename);
			}
		}
		LoadAfter();
	}

	public void LoadFromStream(InputStream stream, int width = 0) throws Error
	{
		var ps = new PeekableInputStream(stream);

		diag.Debug(@"ResizeMode=$(ResizeMode)");
		if (ResizeMode == SixelResizeMode.ByLibJpeg) {
			if (LoadJpeg(ps, width) == false) {
				diag.Debug("fallback to gdk");
				if (width <= 0) width = -1;
				pix = new Pixbuf.from_stream_at_scale(ps, width, -1, true);
			}
		} else {
			if (ResizeMode == SixelResizeMode.ByLoad) {
				if (width <= 0) width = -1;
				pix = new Pixbuf.from_stream_at_scale(ps, width, -1, true);
			} else {
				pix = new Pixbuf.from_stream(ps);
			}
		}
		LoadAfter();
	}

	private bool LoadJpeg(PeekableInputStream ps, int width = 0)
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
					var r = ImageReductor.LoadJpeg(img, width, 0);
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


	// ----- 前処理

	// インデックスカラーに変換します。
	// width: リサイズ後の幅。0 を指定すると、元画像の幅を使用します。
	// height: リサイズ後の高さ。0 を指定すると、width からアスペクト比を維持した値が計算されます。
	public void ConvertToIndexed(int width = 0, int height = 0)
	{
		// リサイズ

		diag.Debug(@"pre width=$(width) height=$(height)");
		if (width <= 0) {
			Width = pix.get_width();
		} else {
			Width = width;
		}
		if (height <= 0) {
			Height = pix.get_height() * Width / pix.get_width();
			if (Height <= 0) {
				Height = 1;
			}
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

