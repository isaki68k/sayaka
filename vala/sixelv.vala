using ULib;

public class SixelV
{
	public static void main(string[] args)
	{
		var sixelv = new SixelV();
		sixelv.main2(args);
	}


	public enum ColorMode {
		Gray,
		GrayMean,
		Fixed8,
		Fixed16,
		Fixed256,
		Custom,
	}
	public enum ReduceMode {
		Diffuse,
		Simple,
	}

	public ColorMode opt_colormode = ColorMode.Fixed256;
	public int opt_graylevel = 0;
	public int opt_width = 0;
	public int opt_height = 0;
	public ReduceMode opt_reduce = ReduceMode.Diffuse;
	public int opt_diffusemultiplier = 1;
	public int opt_diffusedivisor = 3;
	public bool opt_x68k = false;
	public bool opt_ignoreerror = false;
	public ColorMode opt_findfunc = ColorMode.Custom;

	public void main2(string[] args)
	{
		int convert_count = 0;

		foreach (var opt in new OptArgs(args)) {
			if (opt.IsOption()) {
				switch (opt.Opt()) {
					case "--debug":
						Diag.global_debug = true;
						break;

					case "--trace":
						Diag.global_trace = true;
						break;

					case "-e":
					case "--monochrome":
						opt_colormode = ColorMode.Gray;
						opt_findfunc = ColorMode.Gray;
						opt_graylevel = 2;
						break;
						
					case "--gray":
						opt_colormode = ColorMode.Gray;
						opt_findfunc = ColorMode.Gray;
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
								opt_colormode = ColorMode.Fixed8;
								opt_findfunc = ColorMode.Fixed8;
								break;
							case 16:
								opt_colormode = ColorMode.Fixed16;
								opt_findfunc = ColorMode.Fixed16;
								break;
							case 256:
								opt_colormode = ColorMode.Fixed256;
								opt_findfunc = ColorMode.Fixed256;
								break;
							default:
								usage();
								break;
						}
						break;

					case "-8":
						opt_colormode = ColorMode.Fixed8;
						opt_findfunc = ColorMode.Fixed8;
						break;
					case "-16":
						opt_colormode = ColorMode.Fixed16;
						opt_findfunc = ColorMode.Fixed16;
						break;
					case "-256":
						opt_colormode = ColorMode.Fixed256;
						opt_findfunc = ColorMode.Fixed256;
						break;

					case "--colorfinder":
						switch (opt.ValueString()) {
							case "grayntsc":
								opt_findfunc = ColorMode.Gray;
								break;
							case "graymean":
								opt_findfunc = ColorMode.GrayMean;
								break;
							case "fixed8":
								opt_findfunc = ColorMode.Fixed8;
								break;
							case "fixed16":
								opt_findfunc = ColorMode.Fixed16;
								break;
							case "fixed256":
								opt_findfunc = ColorMode.Fixed256;
								break;
							case "custom":
								opt_findfunc = ColorMode.Custom;
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
								opt_reduce = ReduceMode.Diffuse;
								break;
							case "none":
								opt_reduce = ReduceMode.Simple;
								break;
							default:
								usage();
								break;
						}
						break;

					case "--x68k":
						opt_x68k = !opt_x68k;
						break;

					case "--mul":
						opt_diffusemultiplier = opt.ValueInt();
						if (opt_diffusemultiplier <= 0 || opt_diffusemultiplier >= 32768) {
							usage();
						}
						break;
					case "--div":
						opt_diffusedivisor = opt.ValueInt();
						if (opt_diffusedivisor <= 0 || opt_diffusedivisor >= 32768) {
							usage();
						}
						break;

					case "--noerr":
						opt_ignoreerror = !opt_ignoreerror;
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
   --gray=[graylevel]
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
     Select 16 color (4bit) mode.
   -256
     Select 256 color (8bit) mode. This is default.

   -e
   --monochrome
     equals --gray=2

 size
   -w {width}
   --width={width}
     Resize width (pixel).
     If omit -w, width = original image width.
     If set -w but omit -h, height follows aspect ratio by width.
   -h {height}
   --height={height}
     Resize height (pixel). Must need -w.

 algorithm
   -d
   --diffusion={diffuse type}
     auto
       Diffusion algorithm. This is default.
     none
       Simple algorithm. (no diffuser)

   --mul <int>
     Diffusion multiplier. allows 1 .. 32768. default = 1
   --div <int>
     Diffusion divisor. allows 1 .. 32768. default = 3

 colorfind
   --colorfinder={finder}
     grayntsc
       Use NTSC Intensity like gray. This is default at grayscale.
     graymean 
       Use mean of RGB gray.

     fixed8
     fixed16
     fixed256
       Internal fixed color finder for 8, 16, 256. Fast. This is default.
     custom
       Internal custom color finder. Slow, but more high quarity (maybe.)
""");
		Process.exit(1);
	}

	public void Convert(string filename)
	{
		SixelConverter sx = new SixelConverter();

		if (filename.contains("://")) {
			try {
				var file = new HttpClient(filename);
stderr.printf("%s\n", filename);
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
				sx.Load(filename);
			} catch {
				stderr.printf("File load error at %s\n", filename);
				if (opt_ignoreerror) {
					return;
				}
				Process.exit(1);
			}
		}

		sx.DiffuseMultiplier = (int16)opt_diffusemultiplier;
		sx.DiffuseDivisor = (int16)opt_diffusedivisor;

		if (opt_width != 0 && opt_height != 0) {
			sx.Resize(opt_width, opt_height);
		} else if (opt_width != 0) {
			sx.ResizeByWidth(opt_width);
		}

		unowned SixelConverter.FindFunc finder = sx.FindCustom;

		switch (opt_colormode) {
			case ColorMode.Gray:
				sx.SetPaletteGray(opt_graylevel);
				break;
			case ColorMode.Fixed8:
				sx.SetPaletteFixed8();
				break;
			case ColorMode.Fixed16:
				sx.SetPaletteFixed16();
				break;
			case ColorMode.Fixed256:
				sx.SetPaletteFixed256();
				break;
		}

		switch (opt_findfunc) {
			case ColorMode.Gray:
				finder = sx.FindGray;
				break;
			case ColorMode.GrayMean:
				finder = sx.FindGrayMean;
				break;
			case ColorMode.Fixed8:
				finder = sx.FindFixed8;
				break;
			case ColorMode.Fixed16:
				finder = sx.FindFixed16;
				break;
			case ColorMode.Fixed256:
				finder = sx.FindFixed256;
				break;
			case ColorMode.Custom:
				finder = sx.FindCustom;
				break;
		}

		if (opt_x68k) {
			sx.SetPaletteFixed8();
			int[] tbl = new int[] { 0x600000, 0xa00000, 0x006000, 0x00a000, 0x000060, 0x0000a0, 0x606060, 0xa0a0a0};
			for (int i = 0; i < 8; i++) {
				sx.Palette[i + 8, 0] = (uint8)(tbl[i] >> 16);
				sx.Palette[i + 8, 1] = (uint8)(tbl[i] >> 8);
				sx.Palette[i + 8, 2] = (uint8)(tbl[i]);
			}
			sx.PaletteCount = 16;
			finder = sx.FindCustom;
		}

		switch (opt_reduce) {
			case ReduceMode.Simple:
				sx.SimpleReduceCustom(finder);
				break;
			case ReduceMode.Diffuse:
				sx.DiffuseReduceCustom(finder);
				break;
		}

		sx.SixelToStream(stdout);
	}
}

