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

using System.OS;
using ULib;
using Gdk;

public enum OutputFormat
{
	SIXEL,
	GVRAM,
	PALETTEPNG,
}

public class SixelV
{
	public static void main(string[] args)
	{
		var sixelv = new SixelV();
		sixelv.main2(args);
	}

	public bool opt_debug_sixel = false;
	public bool opt_debug_net = false;
	public ReductorColorMode opt_colormode = ReductorColorMode.Fixed256;
	public int opt_graylevel = 256;
	public int opt_width = 0;
	public int opt_height = 0;
	public ResizeAxisMode opt_resizeaxis = ResizeAxisMode.Both;
	public ReductorReduceMode opt_reduce = ReductorReduceMode.HighQuality;
	public bool opt_x68k = false;
	public bool opt_outputpalette = true;
	public bool opt_ignoreerror = false;
	public bool opt_ormode = false;
	public bool opt_profile = false;
	public SixelResizeMode opt_resizemode = SixelResizeMode.ByLoad;
	public SixelLoaderMode opt_loadermode = SixelLoaderMode.Gdk;
	public OutputFormat opt_outputformat = OutputFormat.SIXEL;
	public int opt_output_x = 0;
	public int opt_output_y = 0;
	public float opt_colorfactor = 1.0f;
	public ReductorDiffuseMethod opt_highqualitydiffusemethod = ReductorDiffuseMethod.RDM_FS;
	public ReductorFinderMode opt_findermode = ReductorFinderMode.RFM_Default;
	public int opt_addnoise = 0;
	public SocketFamily opt_address_family = SocketFamily.INVALID;	// UNSPEC がないので代用
	static SixelV this_sixelv;

	public SixelV()
	{
		this_sixelv = this;
	}

	public void main2(string[] args)
	{
		int convert_count = 0;

		// X68k なら、デフォルトで --x68k 相当にする。
		var un = Posix.utsname();
		//stderr.printf("%s\n", un.machine);;
		if (un.machine == "x68k") {
			opt_colormode = ReductorColorMode.FixedX68k;
			opt_ormode = true;
			opt_outputpalette = false;
		}

		foreach (var opt in new OptArgs(args)) {
			if (opt.IsOption() && opt.Opt() != "-") {
				switch (opt.Opt()) {
					case "--debug":
						gDiag.global_debug = true;
						gDiag.Debug("Global Debug ON");
						break;
					case "--debug-sixel":
						opt_debug_sixel = true;
						ImageReductor.debug = 1;
						break;
					case "--debug-net":
						opt_debug_net = true;
						break;

					case "--trace":
						gDiag.global_trace = true;
						break;

					case "--profile":
						opt_profile = true;
						break;

					case "-e":
					case "--monochrome":
						opt_colormode = ReductorColorMode.Mono;
						break;
						
					case "--gray":
						opt_colormode = ReductorColorMode.Gray;
						if (opt.ValueString() != "") {
							opt_graylevel = opt.ValueInt();
						}
						if (opt_graylevel <= 1 || opt_graylevel > 256) {
							usage();
						}
						break;

					case "-p":
					case "--color":
					case "--colors":
						switch (opt.ValueString()) {
							case "8":
								opt_colormode = ReductorColorMode.Fixed8;
								break;
							case "16":
								opt_colormode = ReductorColorMode.FixedANSI16;
								break;
							case "256":
								opt_colormode = ReductorColorMode.Fixed256;
								break;
							case "256rgbi":
								opt_colormode = ReductorColorMode.Fixed256RGBI;
								break;
							case "mono":
								opt_colormode = ReductorColorMode.Mono;
								break;
							case "gray":
								opt_colormode = ReductorColorMode.Gray;
								break;
							case "graymean":
								opt_colormode = ReductorColorMode.GrayMean;
								break;
							case "x68k":
								opt_colormode = ReductorColorMode.FixedX68k;
								break;
							default:
								usage();
								break;
						}
						break;

					case "-8":
						opt_colormode = ReductorColorMode.Fixed8;
						break;
					case "-16":
						opt_colormode = ReductorColorMode.FixedANSI16;
						break;
					case "-256":
						opt_colormode = ReductorColorMode.Fixed256;
						break;

					case "-w":
					case "--width":
						opt_width = opt.ValueInt();
						break;

					case "-h":
					case "--height":
						opt_height = opt.ValueInt();
						break;

					case "--axis":
						switch (opt.ValueString()) {
							case "both":
								opt_resizeaxis = ResizeAxisMode.Both;
								break;
							case "w":
							case "width":
								opt_resizeaxis = ResizeAxisMode.Width;
								break;
							case "h":
							case "height":
								opt_resizeaxis = ResizeAxisMode.Height;
								break;
							case "long":
								opt_resizeaxis = ResizeAxisMode.Long;
								break;
							case "short":
								opt_resizeaxis = ResizeAxisMode.Short;
								break;
						}
						break;

					case "-d":
					case "--diffusion":
						switch (opt.ValueString()) {
							case "auto":
								opt_reduce = ReductorReduceMode.HighQuality;
								break;
							case "none":
								opt_reduce = ReductorReduceMode.Simple;
								break;
							case "fast":
								opt_reduce = ReductorReduceMode.Fast;
								break;
							case "high":
								opt_reduce = ReductorReduceMode.HighQuality;
								break;
							case "fs":
								opt_reduce = ReductorReduceMode.HighQuality;
								opt_highqualitydiffusemethod = ReductorDiffuseMethod.RDM_FS;
								break;
							case "atkinson":
								opt_reduce = ReductorReduceMode.HighQuality;
								opt_highqualitydiffusemethod = ReductorDiffuseMethod.RDM_ATKINSON;
								break;
							case "jajuni":
								opt_reduce = ReductorReduceMode.HighQuality;
								opt_highqualitydiffusemethod = ReductorDiffuseMethod.RDM_JAJUNI;
								break;
							case "stucki":
								opt_reduce = ReductorReduceMode.HighQuality;
								opt_highqualitydiffusemethod = ReductorDiffuseMethod.RDM_STUCKI;
								break;
							case "burkes":
								opt_reduce = ReductorReduceMode.HighQuality;
								opt_highqualitydiffusemethod = ReductorDiffuseMethod.RDM_BURKES;
								break;
							case "2":
								opt_reduce = ReductorReduceMode.HighQuality;
								opt_highqualitydiffusemethod = ReductorDiffuseMethod.RDM_2;
								break;
							case "3":
								opt_reduce = ReductorReduceMode.HighQuality;
								opt_highqualitydiffusemethod = ReductorDiffuseMethod.RDM_3;
								break;
							case "rgb":
								opt_reduce = ReductorReduceMode.HighQuality;
								opt_highqualitydiffusemethod = ReductorDiffuseMethod.RDM_RGB;
								break;
							default:
								usage();
								break;
						}
						break;

					case "--x68k":
						opt_colormode = ReductorColorMode.FixedX68k;
						opt_ormode = true;
						opt_outputpalette = false;
						break;

					case "--noerr":
						opt_ignoreerror = opt.ValueBool();
						break;

					case "--ipv4":
						opt_address_family = SocketFamily.IPV4;
						break;

					case "--ipv6":
						opt_address_family = SocketFamily.IPV6;
						break;

					case "--ormode":
						opt_ormode = opt.ValueBool();
						break;

					case "--palette":
						opt_outputpalette = opt.ValueBool();
						break;

					case "--resize":
						switch (opt.ValueString()) {
							case "load":
								opt_resizemode = SixelResizeMode.ByLoad;
								break;
							case "scale":
								opt_resizemode = SixelResizeMode.ByScaleSimple;
								break;
							case "imagereductor":
								opt_resizemode = SixelResizeMode.ByImageReductor;
								break;
							default:
								usage();
								break;
						}
						break;

					case "--loader":
						switch (opt.ValueString()) {
							case "gdk":
								opt_loadermode = SixelLoaderMode.Gdk;
								break;
							case "lib":
								opt_loadermode = SixelLoaderMode.Lib;
								break;
							default:
								usage();
								break;
						}
						break;

					case "--outputformat":
						switch (opt.ValueString()) {
							case "sixel":
								opt_outputformat = OutputFormat.SIXEL;
								break;
							case "gvram":
								opt_outputformat = OutputFormat.GVRAM;
								break;
							case "palettepng":
								opt_outputformat = OutputFormat.PALETTEPNG;
								break;
							default:
								usage();
								break;
						}
						break;

					case "--output-x":
						opt_output_x = opt.ValueInt();
						break;
					case "--output-y":
						opt_output_y = opt.ValueInt();
						break;

					case "--colorfactor":
						opt_colorfactor = opt.ValueFloat();
						break;

					case "--finder":
						switch (opt.ValueString()) {
							case "rgb":
							case "default":
								opt_findermode = ReductorFinderMode.RFM_Default;
								break;
							case "hsv":
								opt_findermode = ReductorFinderMode.RFM_HSV;
								break;
						}
						break;

					case "--addnoise":
						opt_addnoise = opt.ValueInt();
						break;

					default:
						usage();
						break;
				}
			} else {
				if (convert_count > 0) stdout.putc('\n');
				if (opt.Opt() == "-") {
					Convert("std://in");
				} else {
					Convert(opt.ValueString());
				}
				convert_count++;
			}
		}

		if (convert_count == 0) {
			usage();
		}
	}

	public void usage()
	{
		stderr.printf(
"""sixelv [color] [size] [algorithm] [colorfind] file...

 color
   --gray={graylevel}
     Select grayscale mode and set grayscale level.
     graylevel allows 2 .. 256. default = 256

   -p={color}
   -p {color}
   --color[s]={color}
     Select {color} mode.
       8        : Fixed 8 colors
       16       : Fixed 16 colors
       256      : Fixed 256 colors (MSX SCREEN 8 compatible palette)
                  This is default.
       256rgbi  : Fixed 256 colors (R2G2B2I2 palette)
       mono     : monochrome (1bit)
       gray     : grayscale with NTSC intensity
       graymean : grayscale with mean of RGB
       x68k     : Fixed x68k 16 color palette

   -8, -16, -256
     Shortcut for -p=8, -p=16, -p=256

   -e
   --monochrome
     Shortcut for -p=mono

 size
   -w {width}
   --width={width}
     Resize width (pixel).
     If omit -w, width = original image width.
     If set -w but omit -h, height follows aspect ratio by width.
   -h {height}
   --height={height}
     Resize height (pixel). Must need -w.
   --resize={load, scale, imagereductor, libjpeg}
     Select Resize alogrithm. (for debug)

 algorithm
   -d
   --diffusion={diffuse type}
     auto : This is default. (now implements = high)
     none : Simple algorithm. (no diffuser)
     fast : Fast algorithm.
     high : 2D-Diffusion algorithm.

     Following options, select detailed algorithm with 2D-Diffusion.
     fs       : Floyd Steinberg (default)
     atkinson : Atkinson
     jajuni   : Jarvis, Judice, Ninke
     stucki   : Stucki
     burkes   : Burkes
     2        : 2 pixel (right, down)
     3        : 3 pixel (right, down, rightdown)
     rgb      : for debug

 misc
   --x68k
     SHARP X680x0 mode.
     force X68k 16 fixed color, ormode=on, palette=off

   --ormode={on|off}
     Output OR-mode Sixel.  Default = off

   --palette={on|off}
     Output palette definision. Default = on

   --outputformat={sixel|gvram}
     Output SIXEL or original X68k gvram file format.

   --output-x={y}, --output-y={y}
     X, Y offset for gvram format file. No effect for SIXEL.

   --ipv4
     Connect IPv4 only.

   --ipv6
     Connect IPv6 only.

   --noerr={on|off}
     if turn on, ignore error at open.

 debug
   --debug, --trace, --profile, --debug-sixel, --debug-net
""");
		Process.exit(1);
	}

	public void Convert(string filename)
	{
		Stopwatch sw = null;

		if (opt_profile) {
			sw = new Stopwatch();
			sw.Restart();
		}

		SixelConverter sx = new SixelConverter();

		// SixelConverter モード設定
		sx.diag.opt_debug |= opt_debug_sixel;

		sx.ColorMode = opt_colormode;
		sx.ReduceMode = opt_reduce;
		sx.ResizeMode = opt_resizemode;
		sx.LoaderMode = opt_loadermode;
		sx.OutputPalette = opt_outputpalette;
		sx.GrayCount = opt_graylevel;
		sx.FinderMode = opt_findermode;
		sx.AddNoiseLevel = opt_addnoise;
gDiag.Debug(@"$(opt_addnoise)");
		sx.ResizeWidth = opt_width;
		sx.ResizeHeight = opt_height;
		sx.ResizeAxis = opt_resizeaxis;

		ImageReductor.HighQualityDiffuseMethod = opt_highqualitydiffusemethod;

		if (opt_ormode) {
			sx.OutputMode = SixelOutputMode.Or;
		} else {
			sx.OutputMode = SixelOutputMode.Normal;
		}

		if (opt_profile) {
			sw.StopLog_ms("Create objects");
			sw.Restart();
		}

		// ファイル読み込み

		if (filename == "std://in") {
			try {
				gDiag.Debug(@"Loading stdin");
				sx.LoadFromStream(new InputStreamFromFileStream(stdin));
			} catch {
				stderr.printf("File load error at %s\n", filename);
				if (opt_ignoreerror) {
					return;
				}
				Process.exit(1);
			}
		} else if (filename.contains("://")) {
			try {
				var file = new HttpClient(filename);
				file.diag.opt_debug |= opt_debug_net;
				file.Family = opt_address_family;
				gDiag.Debug(@"Downloading $(filename)");
				var stream = file.GET();
				sx.LoadFromStream(stream);
			} catch (Error e) {
				stderr.printf("File error: %s\n", e.message);
				if (opt_ignoreerror) {
					return;
				}
				Process.exit(1);
			}
		} else {
			try {
				gDiag.Debug(@"Loading $(filename)");
				sx.Load(filename);
			} catch {
				stderr.printf("File load error at %s\n", filename);
				if (opt_ignoreerror) {
					return;
				}
				Process.exit(1);
			}
		}

		if (opt_profile) {
			sw.StopLog_ms("Get and Extract image");
			sw.Restart();
		}

		gDiag.Debug(@"Converting w=$(opt_width), h=$(opt_height)");
		sx.ConvertToIndexed();

		if (opt_profile) {
			sw.StopLog_ms("Convert image");
			sw.Restart();
		}

		if (opt_colorfactor != 1.0) {
			ImageReductor.ColorFactor(opt_colorfactor);
		}

		switch (opt_outputformat) {
			case OutputFormat.SIXEL:
				Posix.@signal(SIGINT, signal_handler);
				sx.SixelToStream(stdout);
				break;
			case OutputFormat.GVRAM:
				if (opt_output_x < 0 || opt_output_y < 0) {
					stderr.printf("invalid offset.\n");
					return;
				}
				if (opt_output_y + sx.Height > 512) {
					stderr.printf("Image out of height of GVRAM.\n");
					return;
				}
				if (ImageReductor.PaletteCount <= 16
				  && opt_output_x + sx.Width > 1024) {
					stderr.printf("Image out of width of 16-color GVRAM.\n");
					return;
				}
				if (ImageReductor.PaletteCount > 16
				  && opt_output_x + sx.Width > 512) {
					stderr.printf("Image out of width of 256-color GVRAM.\n");
					return;
				}

				uint8 word[2];

				// バージョン番号
				word[0] = 0;
				word[1] = 1;	// ver 1
				stdout.write(word);

				// パレット数
				word[0] = (uint8)(ImageReductor.PaletteCount >> 8);
				word[1] = (uint8)(ImageReductor.PaletteCount);
				stdout.write(word);

				// X68k パレットを作る
				for (int i = 0; i < ImageReductor.PaletteCount; i++) {
					uint16 r = ImageReductor.Palette[i].r >> 3;
					uint16 g = ImageReductor.Palette[i].g >> 3;
					uint16 b = ImageReductor.Palette[i].b >> 3;
					uint I =
						(ImageReductor.Palette[i].r & 0x7)
					  + (ImageReductor.Palette[i].g & 0x7)
					  + (ImageReductor.Palette[i].b & 0x7);

					word[0] = g << 3 | r >> 2;
					word[1] = r << 6 | b << 1;
					word[1] |= I > (21 / 2) ? 1 : 0;
					stdout.write(word);
				}
				// x, y, w, h を BE word で出す
				word[0] = (uint8)(opt_output_x >> 8);
				word[1] = (uint8)(opt_output_x);
				stdout.write(word);
				word[0] = (uint8)(opt_output_y >> 8);
				word[1] = (uint8)(opt_output_y);
				stdout.write(word);
				word[0] = (uint8)(sx.Width >> 8);
				word[1] = (uint8)(sx.Width);
				stdout.write(word);
				word[0] = (uint8)(sx.Height >> 8);
				word[1] = (uint8)(sx.Height);
				stdout.write(word);

				// GVRAM データを作る
				stdout.write(sx.Indexed);

				break;

			case OutputFormat.PALETTEPNG:
				// 11 x 11 はどうなのかとか。img2sixel 合わせだが、
				// img2sixel 側の問題でうまくいかない。
				var palpix = new Pixbuf(Colorspace.RGB, false, 8, ImageReductor.PaletteCount * 11, 11);
				unowned uint8* p = palpix.get_pixels();
				for (int y = 0; y < 11; y++) {
				for (int i = 0; i < ImageReductor.PaletteCount; i++) {
				for (int x = 0; x < 11; x++) {
					p[0] = ImageReductor.Palette[i].r;
					p[1] = ImageReductor.Palette[i].g;
					p[2] = ImageReductor.Palette[i].b;
					p += 3;
				}
				}
				}
				var so = new OutputStreamFromFileStream(stdout);
				try {
					palpix.save_to_stream(so, "png");
				} catch {
				}
				break;
		}

		if (opt_profile) {
			sw.StopLog_ms("Output SIXEL");
			sw.Restart();
		}

	}

	public static void signal_handler(int signo)
	{
		this_sixelv.signal_handler_2(signo);
	}

	public void signal_handler_2(int signo)
	{
		switch (signo) {
		 case SIGINT:
			// SIXEL 出力を中断する (CAN + ST)
			stdout.printf("\x18\x1b\\");
			stdout.flush();
			break;
		}
	}
}

