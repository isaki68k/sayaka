using Gdk;

public class Diag
{
	public static void DEBUG(string fmt, ...)
	{
		va_list va = va_list();
		stderr.vprintf(fmt, va);
		stderr.puts("\n");
	}
}

public class SixelConverter
{
	private Pixbuf pix;

	public uint8[,] Palette = new uint8[256, 3];

	public int PaletteCount;

	public void Load(string filename) throws Error
	{
		pix = new Pixbuf.from_file(filename);
		Diag.DEBUG("filename=%s", filename);
		Diag.DEBUG("Size=(%d,%d)", pix.get_width(), pix.get_height());
		Diag.DEBUG("bits=%d", pix.get_bits_per_sample());
		Diag.DEBUG("nCh=%d", pix.get_n_channels());
		Diag.DEBUG("rowstride=%d", pix.get_rowstride());
	}

	// ----- 前処理

	public void ResizeByWidth(int width)
	{
		int h = pix.get_height() * width / pix.get_width();
		Resize(width, h);
	}	

	public void Resize(int width, int height)
	{
		pix = pix.scale_simple(width, height, InterpType.BILINEAR);
	}

	// ----- パレットの設定

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

	// ANSI 16 色の固定パレットを生成します。
	public void SetPaletteFixed16()
	{
		// ANSI 16 色といっても色実体は実装依存らしい。

		for (int i = 0; i < 16; i++) {
			uint8 R = (uint8)((i     ) & 0x01);
			uint8 G = (uint8)((i >> 1) & 0x01);
			uint8 B = (uint8)((i >> 2) & 0x01);
			uint8 I = (uint8)((i >> 3) & 0x01);

			R = R * 170 + I * 85;
			G = G * 170 + I * 85;
			B = B * 170 + I * 85;

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

	// グレースケールパレット時に、最も近いパレット番号を返します。
	public uint8 FindGray(uint8 r, uint8 g, uint8 b)
	{
		// NTSC 輝度
		return (uint8)((((int)r * 76 + (int)g * 153 + (int)b * 26) / 255 * (PaletteCount - 1) / 255));
	}

	// 固定8色時に、最も近いパレット番号を返します。
	public uint8 FindFixed8(uint8 r, uint8 g, uint8 b)
	{
		uint8 R = (uint8)(r >= 128);
		uint8 G = (uint8)(g >= 128);
		uint8 B = (uint8)(b >= 128);
		return R + (G << 1) + (B << 2);
	}

	// 固定16色時に、最も近いパレット番号を返します。
	public uint8 FindFixed16(uint8 r, uint8 g, uint8 b)
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

	// カスタムパレット時に、最も近いパレット番号を返します。
	public uint8 FindCustom(uint8 r, uint8 g, uint8 b)
	{
		// RGB の各色の距離の和が最小、にしてある。
		// YCC で判断したほうが良好なのは知ってるけど、そこまで必要じゃない。
		uint8 rv = 0;
		int min_d = int.MAX;
		for (int i = 0; i < PaletteCount; i++) {
			int dR = (int)Palette[i, 0] - (int)r;
			int dG = (int)Palette[i, 1] - (int)g;
			int dB = (int)Palette[i, 2] - (int)b;
			int d = dR.abs() + dG.abs() + dB.abs();
			if (d < min_d) {
				rv = (uint8)i;
				min_d = d;
				if (d == 0) break;
			}
		}
		return rv;
	}

	// ----- カラー変換 (減色)

	// ----- 単純減色法
	// ----- 要は誤差拡散法ではなく、当該ピクセルのみの値で減色するアルゴリズム。

	// 単純減色法で NTSC 輝度減色でグレースケールにします。
	public void SimpleReduceGray()
	{
		SimpleReduceCustom(FindGray);
	}

	// 単純減色法で 固定8色にします。
	public void SimpleReduceFixed8()
	{
		SimpleReduceCustom(FindFixed8);
	}

	// 単純減色法で 固定16色にします。
	public void SimpleReduceFixed16()
	{
		SimpleReduceCustom(FindFixed16);
	}

	// 単純減色法で 固定256色にします。
	public void SimpleReduceFixed256()
	{
		SimpleReduceCustom(FindFixed256);
	}

	// 単純減色法を適用します。
	// この関数を実行すると、pix の内容は画像からパレット番号の列に変わります。
	public void SimpleReduceCustom(FindFunc op)
	{
		unowned uint8[] p0 = pix.get_pixels();
		int w = pix.get_width();
		int h = pix.get_height();
		int nch = pix.get_n_channels();
		int ybase = 0;
		int dst = 0;

		for (int y = 0; y < h; y++) {
			for (int x = 0; x < w; x++) {
				uint8* psrc = &p0[ybase + x * nch];
				uint8 r = psrc[0];
				uint8 g = psrc[1];
				uint8 b = psrc[2];
				// パレット番号の列として、画像を破壊して書き込み
				p0[dst++] = op(r, g, b);
			}
			ybase += pix.get_rowstride();
		}
	}

	private uint8 satulate_add(uint8 a, int16 b)
	{
		int16 rv = (int16)a + b;
		if (rv > 255) rv = 255;
		if (rv < 0) rv = 0;
		return (uint8)rv;
	}

	public void DiffuseReduceGray()
	{
		DiffuseReduceCustom(FindGray);
	}

	public void DiffuseReduceFixed8()
	{
		DiffuseReduceCustom(FindFixed8);
	}

	public void DiffuseReduceFixed16()
	{
		DiffuseReduceCustom(FindFixed16);
	}

	public void DiffuseReduceFixed256()
	{
		DiffuseReduceCustom(FindFixed256);
	}

	public int16 DiffuseMultiplier = 1;
	public int16 DiffuseDivisor = 3;

	private void DiffusePixel(uint8 r, uint8 g, uint8 b, uint8* ptr, uint8 c)
	{
		ptr[0] = satulate_add(ptr[0], ((int16)r - Palette[c, 0]) * DiffuseMultiplier / DiffuseDivisor);
		ptr[1] = satulate_add(ptr[1], ((int16)g - Palette[c, 1]) * DiffuseMultiplier / DiffuseDivisor);
		ptr[2] = satulate_add(ptr[2], ((int16)b - Palette[c, 2]) * DiffuseMultiplier / DiffuseDivisor);
	}

	public void DiffuseReduceCustom(FindFunc op)
	{
		unowned uint8[] p0 = pix.get_pixels();
		int w = pix.get_width();
		int h = pix.get_height();
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

				uint8 C = op(r, g, b);
				// パレット番号の列として、画像を破壊して書き込み
				p0[dst++] = C;

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


	// ----- Sixel 出力

	private const string ESC = "\x1b";
	private const string DCS = ESC + "P";

	// デバッグ用に、pixbuf を16進テキスト出力。
	public void RawHexToStream(FileStream stream)
	{
		unowned uint8[] p0 = pix.get_pixels();
		int w = pix.get_width();
		int h = pix.get_height();
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
		int w = pix.get_width();
		int h = pix.get_height();
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

	// Sixel 出力
	public void SixelToStream(FileStream stream)
	{
		unowned uint8[] p0 = pix.get_pixels();
		int w = pix.get_width();
		int h = pix.get_height();
		int src = 0;

		// Sixel 開始
		stream.puts(DCS);
		// Pixel Aspect Ratio = 1:1, current color
		stream.puts("7;1;q");

		// パレットの出力
		for (int i = 0; i < PaletteCount; i++) {
			stream.printf("#%d;%d;%d;%d;%d", i, 2,
				Palette[i, 0] * 100 / 255,
				Palette[i, 1] * 100 / 255,
				Palette[i, 2] * 100 / 255);
		}

#if false
		// 一番効率のよくない方法。
		for (int y = 0; y < h; y++) {
			for (int x = 0; x < w; x++) {
				int I = p0[src++];
				stream.printf("#%d", I);
				stream.putc(SixelChars[1 << (y % 6)]);
			}
			if (y % 6 == 5) {
				stream.putc('-');
			} else {
				stream.putc('$');
			}
		}

#elif false
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
				stream.printf("#%d", c);
				int n = 1;
				uint8 t = buf[c, 0];
				for (int x = 1; x < w; x++) {
					if (t != buf[c, x]) {
						stream.puts(SixelRepunit(n, t));
						n = 1;
						t = buf[c, x];
					} else {
						n++;
					}
				}
				if (t != 0) {
					stream.puts(SixelRepunit(n, t));
				}

				if (c != PaletteCount - 1) {
					stream.putc('$');
				}

				// clear
				for (int i = 0; i < buf.length[1]; i++) {
					buf[c, i] = 0;
				}
			}
			stream.putc('-');
		}

#else
		// 030 ターゲット

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
					stream.printf("#%d", min_color);

					// 相対 X シーク処理
					int16 space = min_x[min_color] - (mx + 1);
					if (space > 0) {
						stream.puts(SixelRepunit(space, 0));
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
								stream.puts(SixelRepunit(n, prev_t));
							}
							prev_t = t;
							n = 1;
						} else {
							n++;
						}
					}
					// 最後のパターン
					if (prev_t != 0 && n > 0) {
						stream.puts(SixelRepunit(n, prev_t));
					}

					// X 位置を更新
					mx = max_x[min_color];
					// 済んだ印
					min_x[min_color] = -1;

				} while (true);

				stream.putc('$');

				// 最後までやったら抜ける
				if (mx == -1) break;

			} while (true);

			stream.putc('-');

		}
#endif

		stream.puts(ESC);
		stream.putc('\\');
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

