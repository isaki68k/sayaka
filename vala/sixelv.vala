
public class SixelV
{
	public static void main(string[] args)
	{
		var sixelv = new SixelV();
		sixelv.main2(args);
	}


	public enum ColorMode {
		Gray,
		Fixed8,
		Fixed16,
		Fixed256,
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
	public bool opt_custom = false;
	public bool opt_graymean = false;

	public void main2(string[] args)
	{
		int convert_count = 0;

		for (int i = 1; i < args.length; i++) {
			switch (args[i]) {
				case "-g":
					opt_colormode = ColorMode.Gray;
					opt_graylevel = 256;
					break;
				case "-8":
					opt_colormode = ColorMode.Fixed8;
					break;
				case "-16":
					opt_colormode = ColorMode.Fixed16;
					break;
				case "-256":
					opt_colormode = ColorMode.Fixed256;
					break;

				case "--custom":
					opt_custom = true;
					break;

				case "--fixed":
					opt_custom = false;
					break;

				case "--graymean":
					opt_graymean = true;
					break;
				case "--grayntsc":
					opt_graymean = false;
					break;

				case "-w":
					if (i == args.length - 1) {
						usage();
					}
					opt_width = int.parse(args[++i]);
					break;
				case "-h":
					if (i == args.length - 1) {
						usage();
					}
					opt_height = int.parse(args[++i]);
					break;

				case "-s":
					opt_reduce = ReduceMode.Simple;
					break;

				case "-d":
					opt_reduce = ReduceMode.Diffuse;
					break;

				case "--mul":
					if (i == args.length - 1) {
						usage();
					}
					opt_diffusemultiplier = int.parse(args[++i]);
					if (opt_diffusemultiplier <= 0 || opt_diffusemultiplier >= 32768) {
						usage();
					}
					break;
				case "--div":
					if (i == args.length - 1) {
						usage();
					}
					opt_diffusedivisor = int.parse(args[++i]);
					if (opt_diffusedivisor <= 0 || opt_diffusedivisor >= 32768) {
						usage();
					}
					break;

				default:
					int n = 0;
					if (args[i].scanf("-g%d", &n) == 1) {
						if (n <= 1 || n > 256) {
							usage();
						}
						opt_colormode = ColorMode.Gray;
						opt_graylevel = n;
					} else {
						if (convert_count > 0) stdout.putc('\n');
						Convert(args[i]);
						convert_count++;
					}
					break;
			}
		}

		if (convert_count == 0) {
			usage();
		}
	}

	public void usage()
	{
		stdout.printf(
"""sixelv [color] [size] [algorithm] [colorfind] file...

 color
   -g[graylevel]
     Select grayscale mode.
     graylevel allows 2 .. 256. default = 256
   -8
     Select 8 color (3bit) mode.
   -16
     Select 16 color (4bit) mode.
   -256
     Select 256 color (8bit) mode. This is default.

 size
   -w {width}
     Resize width (pixel).
     If omit -w, width = original image width.
     If set -w but omit -h, height follows aspect ratio by width.
   -h {height}
     Resize height (pixel). Must need -w.

 algorithm
   -d
     Diffusion algorithm. This is default.
   -s
     Simple algorithm.
   --mul <int>
     Diffusion multiplier. allows 1 .. 32768. default = 1
   --div <int>
     Diffusion divisor. allows 1 .. 32768. default = 3

 colorfind
   --fixed
     Internal fixed color finder. Fast. This is default.
   --custom
     Internal custom color finder. Slow, but more high quarity (maybe.)
   --grayntsc
     Use NTSC Intensity like gray. This is default.
     Only affects in grayscale mode.
   --graymean
     Use mean of RGB gray.
     Only affects in grayscale mode.
""");
		Process.exit(1);
	}

	public void Convert(string filename)
	{
		SixelConverter sx = new SixelConverter();

		try {
			sx.Load(filename);
		} catch {
			stderr.printf("File load error at %s\n", filename);
			Process.exit(1);
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
				finder = sx.FindGray;
				if (opt_graymean) {
					finder = sx.FindGrayMean;
				}
				break;
			case ColorMode.Fixed8:
				sx.SetPaletteFixed8();
				finder = sx.FindFixed8;
				break;
			case ColorMode.Fixed16:
				sx.SetPaletteFixed16();
				finder = sx.FindFixed16;
				break;
			case ColorMode.Fixed256:
				sx.SetPaletteFixed256();
				finder = sx.FindFixed256;
				break;
		}

		if (opt_custom) {
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


