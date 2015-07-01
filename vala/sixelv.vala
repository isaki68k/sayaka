
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
						if (n < 0 || n > 256) {
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
"sixelv [-g[<gray_level>]] [-8] [-16] [-256] [-w {width}] [-h {height}] [-s] [-d] [--div {divisor}] file ...\n" +
"   -d: diffuse\n" +
"   -s: simple\n"
		);
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

		switch (opt_colormode) {
			case ColorMode.Gray:
				sx.SetPaletteGray(opt_graylevel);
				switch (opt_reduce) {
					case ReduceMode.Simple:
						sx.SimpleReduceGray();
						break;
					case ReduceMode.Diffuse:
						sx.DiffuseReduceGray();
						break;
				}
				break;
			case ColorMode.Fixed8:
				sx.SetPaletteFixed8();
				switch (opt_reduce) {
					case ReduceMode.Simple:
						sx.SimpleReduceFixed8();
						break;
					case ReduceMode.Diffuse:
						sx.DiffuseReduceFixed8();
						break;
				}
				break;
			case ColorMode.Fixed16:
				sx.SetPaletteFixed16();
				switch (opt_reduce) {
					case ReduceMode.Simple:
						sx.SimpleReduceFixed16();
						break;
					case ReduceMode.Diffuse:
						sx.DiffuseReduceFixed16();
						break;
				}
				break;
			case ColorMode.Fixed256:
				sx.SetPaletteFixed256();
				switch (opt_reduce) {
					case ReduceMode.Simple:
						sx.SimpleReduceFixed256();
						break;
					case ReduceMode.Diffuse:
						sx.DiffuseReduceFixed256();
						break;
				}
				break;
		}

		sx.SixelToStream(stdout);
	}
}


