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

public class SixelV
{
	public static void main(string[] args)
	{
		var sixelv = new SixelV();
		sixelv.main2(args);
	}

	public ReductorColorMode opt_colormode = ReductorColorMode.Fixed256;
	public int opt_graylevel = 0;
	public int opt_width = 0;
	public int opt_height = 0;
	public ReductorReduceMode opt_reduce = ReductorReduceMode.HighQuality;
	public bool opt_x68k = false;
	public bool opt_outputpalette = true;
	public bool opt_ignoreerror = false;
	public bool opt_ormode = false;
	public bool opt_profile = false;
	public SixelResizeMode opt_resizemode = SixelResizeMode.ByLoad;
	public SocketFamily opt_address_family = SocketFamily.INVALID;	// UNSPEC がないので代用
	static SixelV this_sixelv;

	public SixelV()
	{
		this_sixelv = this;
	}

	public void main2(string[] args)
	{
		int convert_count = 0;

		foreach (var opt in new OptArgs(args)) {
			if (opt.IsOption()) {
				switch (opt.Opt()) {
					case "--debug":
						gDiag.global_debug = true;
						gDiag.Debug("Debug ON");
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
						opt_graylevel = 256;
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
						switch (opt.ValueInt()) {
							case 8:
								opt_colormode = ReductorColorMode.Fixed8;
								break;
							case 16:
								opt_colormode = ReductorColorMode.FixedANSI16;
								break;
							case 256:
								opt_colormode = ReductorColorMode.Fixed256;
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

					case "--colorfinder":
						switch (opt.ValueString()) {
							case "graymean":
								opt_colormode = ReductorColorMode.GrayMean;
								break;
							default:
								usage();
								break;
						}
						break;

					case "-w":
					case "--width":
						opt_width = opt.ValueInt();
						break;

					case "-h":
					case "--height":
						opt_height = opt.ValueInt();
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

					default:
						usage();
						break;
				}
			} else {
				if (convert_count > 0) stdout.putc('\n');
				Convert(opt.ValueString());
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
     Select grayscale mode.
     graylevel allows 2 .. 256. default = 256

   -p={color}
   -p {color}
   --color[s]={color}
     Select {color} mode.
     color = 8, 16, 256 only supports now.

   -8
     Select 8 color (3bit) mode.
   -16
     Select 16 ANSI color (4bit) mode.
   -256
     Select 256 color (8bit) mode. This is default.

   -e
   --monochrome
	 Select monochrome mode.

 size
   -w {width}
   --width={width}
     Resize width (pixel).
     If omit -w, width = original image width.
     If set -w but omit -h, height follows aspect ratio by width.
   -h {height}
   --height={height}
     Resize height (pixel). Must need -w.
   --resize={load, scale, imagereductor}
     Select Resize alogrithm. (for debug)

 algorithm
   -d
   --diffusion={diffuse type}
     auto : This is default. (now implements = high)
     none : Simple algorithm. (no diffuser)
     fast : Fast algorithm.
	 high : 2D-Diffusion algorithm.

 colorfind
   --colorfinder={finder}
     graymean : Use mean of RGB gray.

 misc
   --x68k
     SHARP X680x0 mode.
     force X68k 16 fixed color, ormode=on, palette=off

   --ormode={on|off}
     Output OR-mode Sixel.  Default = off

   --palette={on|off}
     Output palette definision. Default = on

   --ipv4
     Connect IPv4 only.

   --ipv6
     Connect IPv6 only.

   --noerr={on|off}
     if turn on, ignore error at open.

 debug
   --debug, --trace, --profile
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

		sx.ColorMode = opt_colormode;
		sx.ReduceMode = opt_reduce;
		sx.ResizeMode = opt_resizemode;
		sx.OutputPalette = opt_outputpalette;
		sx.GrayCount = opt_graylevel;

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

		if (filename.contains("://")) {
			try {
				var file = new HttpClient(filename);
				file.Family = opt_address_family;
				gDiag.Debug(@"Downloading $(filename)");
				var stream = file.GET();
				sx.LoadFromStream(stream, opt_width);
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
				sx.Load(filename, opt_width);
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
		sx.ConvertToIndexed(opt_width, opt_height);

		if (opt_profile) {
			sw.StopLog_ms("Convert image");
			sw.Restart();
		}

		Posix.@signal(SIGINT, signal_handler);
		sx.SixelToStream(stdout);

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

