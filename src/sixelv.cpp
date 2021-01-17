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

#include "sayaka.h"
#include "Diag.h"
#include "FileStream.h"
#include "HttpClient.h"
#include "ImageReductor.h"
#include "SixelConverter.h"
#include "term.h"
#include <array>
#include <chrono>
#include <map>
#include <memory>
#include <tuple>
#include <err.h>
#include <getopt.h>
#include <sys/utsname.h>
#include <gio/gunixoutputstream.h>

using namespace std::chrono;

enum OutputFormat {
	SIXEL,
	GVRAM,
	PALETTEPNG,
};

Diag diag;
Diag diagHttp;
int opt_debug_sixel = 0;
ReductorColorMode opt_colormode = ReductorColorMode::Fixed256;
int opt_graylevel = 256;
int opt_width = 0;
int opt_height = 0;
ResizeAxisMode opt_resizeaxis = ResizeAxisMode::Both;
ReductorReduceMode opt_reduce = ReductorReduceMode::HighQuality;
bool opt_x68k = false;
bool opt_outputpalette = true;
bool opt_ignore_error = false;
bool opt_ormode = false;
bool opt_profile = false;
SixelResizeMode opt_resizemode = SixelResizeMode::ByLoad;
SixelLoaderMode opt_loadermode = SixelLoaderMode::Gdk;
OutputFormat opt_outputformat = OutputFormat::SIXEL;
int opt_output_x = 0;
int opt_output_y = 0;
float opt_color_factor = 1.0f;
ReductorDiffuseMethod opt_highqualitydiffusemethod =
	ReductorDiffuseMethod::RDM_FS;
ReductorFinderMode opt_findermode = ReductorFinderMode::RFM_Default;
int opt_addnoise = 0;
int opt_address_family = AF_UNSPEC;

enum {
	OPT_8 = 0x80,
	OPT_16,
	OPT_256,
	OPT_addnoise,
	OPT_axis,
	OPT_color_factor,
	OPT_debug,
	OPT_debug_http,
	OPT_debug_sixel,
	OPT_finder,
	OPT_gray,
	OPT_height,
	OPT_help,
	OPT_ipv4,
	OPT_ipv6,
	OPT_loader,
	OPT_noerr,
	OPT_x68k,
	OPT_ormode,
	OPT_output_format,
	OPT_output_x,
	OPT_output_y,
	OPT_palette,
	OPT_profile,
	OPT_resize,
};

static const struct option longopts[] = {
	{ "8",				no_argument,		NULL,	OPT_8 },
	{ "16",				no_argument,		NULL,	OPT_16 },
	{ "256",			no_argument,		NULL,	OPT_256 },
	{ "addnoise",		required_argument,	NULL,	OPT_addnoise },
	{ "axis",			required_argument,	NULL,	OPT_axis },
	{ "color",			required_argument,	NULL,	'p' },
	{ "colors",			required_argument,	NULL,	'p' },
	{ "color-factor",	required_argument,	NULL,	OPT_color_factor },
	{ "debug",			required_argument,	NULL,	OPT_debug },
	{ "debug-http",		required_argument,	NULL,	OPT_debug_http },
	{ "debug-sixel",	no_argument,		NULL,	OPT_debug_sixel },
	{ "diffusion",		required_argument,	NULL,	'd' },
	{ "finder",			required_argument,	NULL,	OPT_finder },
	{ "gray",			required_argument,	NULL,	OPT_gray },
	{ "height",			required_argument,	NULL,	'h' },
	{ "ipv4",			no_argument,		NULL,	OPT_ipv4 },
	{ "ipv6",			no_argument,		NULL,	OPT_ipv6 },
	{ "loader",			required_argument,	NULL,	OPT_loader },
	{ "monochrome",		no_argument,		NULL,	'e' },
	{ "noerr",			no_argument,		NULL,	OPT_noerr },
	{ "ormode",			no_argument,		NULL,	OPT_ormode },
	{ "output-format",	required_argument,	NULL,	OPT_output_format },
	{ "output-x",		required_argument,	NULL,	OPT_output_x },
	{ "output-y",		required_argument,	NULL,	OPT_output_y },
	{ "palette",		required_argument,	NULL,	OPT_palette },
	{ "profile",		no_argument,		NULL,	OPT_profile },
	{ "resize",			required_argument,	NULL,	OPT_resize },
	{ "width",			no_argument,		NULL,	'w' },
	{ "x68k",			no_argument,		NULL,	OPT_x68k },
	{ "help",			no_argument,		NULL,	OPT_help },
	{ NULL },
};

std::map<const std::string, ReductorColorMode> colormode_map = {
	{ "8",					ReductorColorMode::Fixed8 },
	{ "16",					ReductorColorMode::FixedANSI16 },
	{ "256",				ReductorColorMode::Fixed256 },
	{ "256rgbi",			ReductorColorMode::Fixed256RGBI },
	{ "mono",				ReductorColorMode::Mono },
	{ "gray",				ReductorColorMode::Gray },
	{ "graymean",			ReductorColorMode::GrayMean },
	{ "x68k",				ReductorColorMode::FixedX68k },
};

std::map<const std::string, ResizeAxisMode> resizeaxis_map = {
	{ "both",				ResizeAxisMode::Both },
	{ "w",					ResizeAxisMode::Width },
	{ "width",				ResizeAxisMode::Width },
	{ "h",					ResizeAxisMode::Height },
	{ "height",				ResizeAxisMode::Height },
	{ "long",				ResizeAxisMode::Long },
	{ "short",				ResizeAxisMode::Short },
	{ "sdboth",				ResizeAxisMode::ScaleDownBoth },
	{ "scaledown-both",		ResizeAxisMode::ScaleDownBoth },
	{ "sdw",				ResizeAxisMode::ScaleDownWidth },
	{ "sdwidth",			ResizeAxisMode::ScaleDownWidth },
	{ "scaledown-width",	ResizeAxisMode::ScaleDownWidth },
	{ "sdh",				ResizeAxisMode::ScaleDownHeight },
	{ "sdheight",			ResizeAxisMode::ScaleDownHeight },
	{ "scaledown-height",	ResizeAxisMode::ScaleDownHeight },
	{ "sdlong",				ResizeAxisMode::ScaleDownLong },
	{ "scaledown-long",		ResizeAxisMode::ScaleDownLong },
	{ "sdshort",			ResizeAxisMode::ScaleDownShort },
	{ "scaledown-short",	ResizeAxisMode::ScaleDownShort },
};

std::map<const std::string, SixelResizeMode> resizemode_map = {
	{ "load",				SixelResizeMode::ByLoad },
	{ "scale",				SixelResizeMode::ByScaleSimple },
	{ "imagereductor",		SixelResizeMode::ByImageReductor },
};

std::map<const std::string, SixelLoaderMode> loadermode_map = {
	{ "gdk",				SixelLoaderMode::Gdk },
	{ "lib",				SixelLoaderMode::Lib },
};

std::map<const std::string, OutputFormat> outputformat_map = {
	{ "sixel",				OutputFormat::SIXEL },
	{ "gvram",				OutputFormat::GVRAM },
	{ "palettepng",			OutputFormat::PALETTEPNG },
};

std::map<const std::string, ReductorFinderMode> findermode_map = {
	{ "default",			ReductorFinderMode::RFM_Default },
	{ "rgb",				ReductorFinderMode::RFM_Default },
	{ "hsv",				ReductorFinderMode::RFM_HSV },
};

#define RRM ReductorReduceMode
#define RDM ReductorDiffuseMethod
std::map<const std::string, std::pair<RRM, RDM>> reduce_map = {
	{ "auto",		{ RRM::HighQuality,	(RDM)-1 } },
	{ "none",		{ RRM::Simple,		(RDM)-1 } },
	{ "fast",		{ RRM::Fast,		(RDM)-1 } },
	{ "high",		{ RRM::HighQuality,	(RDM)-1 } },
	{ "fs",			{ RRM::HighQuality,	RDM::RDM_FS } },
	{ "atkinson",	{ RRM::HighQuality,	RDM::RDM_ATKINSON } },
	{ "jajuni",		{ RRM::HighQuality,	RDM::RDM_JAJUNI } },
	{ "stucki",		{ RRM::HighQuality,	RDM::RDM_STUCKI } },
	{ "burkes",		{ RRM::HighQuality,	RDM::RDM_BURKES } },
	{ "2",			{ RRM::HighQuality,	RDM::RDM_2 } },
	{ "3",			{ RRM::HighQuality,	RDM::RDM_3 } },
	{ "rgb",		{ RRM::HighQuality,	RDM::RDM_RGB } },
};
#undef RRM
#undef RDM

[[noreturn]] static void usage();
static bool optbool(const char *arg);
static void Convert(const std::string& filename);
static void signal_handler(int signo);

// map から key を検索しその値を返す。
// 見付からなければ自分で usage() を呼ぶ。
template <typename T> T
select_opt(const std::map<const std::string, T>& map, const char *key)
{
	if (map.find(key) == map.end()) {
		usage();
	}
	return map.at(key);
}

int main(int ac, char *av[])
{
	struct utsname ut;
	int c;
	int r;

	diagHttp.SetClassname("HttpClient");

	// X68k なら、デフォルトで --x68k 相当にする。
	uname(&ut);
	if (strcmp(ut.machine, "x68k") == 0) {
		opt_colormode = ReductorColorMode::FixedX68k;
		opt_ormode = true;
		opt_outputpalette = false;
	}

	while ((c = getopt_long(ac, av, "d:eh:p:w:", longopts, NULL)) != -1) {
		switch (c) {
		 case OPT_debug:
			diag.SetLevel(atoi(optarg));
			break;

		 case OPT_debug_http:
			diagHttp.SetLevel(atoi(optarg));
			break;

		 case OPT_debug_sixel:
			opt_debug_sixel = 1;
			ImageReductor::Debug = 1;
			break;

		 case 'e':
			opt_colormode = ReductorColorMode::Mono;
			break;

		 case OPT_gray:
			opt_colormode = ReductorColorMode::Gray;
			opt_graylevel = atoi(optarg);
			if (opt_graylevel <= 1 || opt_graylevel > 256) {
				usage();
			}
			break;

		 case OPT_profile:
			opt_profile = true;
			break;

		 case 'p':
			opt_colormode = select_opt(colormode_map, optarg);
			break;

		 case OPT_8:
			opt_colormode = ReductorColorMode::Fixed8;
			break;
		 case OPT_16:
			opt_colormode = ReductorColorMode::FixedANSI16;
			break;
		 case OPT_256:
			opt_colormode = ReductorColorMode::Fixed256;
			break;

		 case 'w':
			opt_width = atoi(optarg);
			break;
		 case 'h':
			opt_height = atoi(optarg);
			break;

		 case OPT_axis:
			opt_resizeaxis = select_opt(resizeaxis_map, optarg);
			break;

		 case 'd':
		 {
			auto [ reduce, diffuse ] = select_opt(reduce_map, optarg);
			opt_reduce = reduce;
			if (diffuse != (ReductorDiffuseMethod)-1) {
				opt_highqualitydiffusemethod = diffuse;
			}
			break;
		 }

		 case OPT_x68k:
			opt_colormode = ReductorColorMode::FixedX68k;
			opt_ormode = true;
			opt_outputpalette = false;
			break;

		 case OPT_noerr:
			opt_ignore_error = optbool(optarg);
			break;

		 case OPT_ipv4:
			opt_address_family = AF_INET;
			break;

		 case OPT_ipv6:
			opt_address_family = AF_INET6;
			break;

		 case OPT_ormode:
			opt_ormode = optbool(optarg);
			break;

		 case OPT_palette:
			opt_outputpalette = optbool(optarg);
			break;

		 case OPT_resize:
			opt_resizemode = select_opt(resizemode_map, optarg);
			break;

		 case OPT_loader:
			opt_loadermode = select_opt(loadermode_map, optarg);
			break;

		 case OPT_output_format:
			opt_outputformat = select_opt(outputformat_map, optarg);
			break;

		 case OPT_output_x:
			opt_output_x = atoi(optarg);
			break;
		 case OPT_output_y:
			opt_output_y = atoi(optarg);
			break;

		 case OPT_color_factor:
			opt_color_factor = atof(optarg);
			break;

		 case OPT_finder:
			opt_findermode = select_opt(findermode_map, optarg);
			break;

		 case OPT_addnoise:
			opt_addnoise = atoi(optarg);
			break;

		 default:
			usage();
			break;
		}
	}
	ac -= optind;
	av += optind;

	int nfiles;
	for (nfiles = 0; nfiles < ac; nfiles++) {
		if (nfiles > 0)
			printf("\n");
		Convert(av[nfiles]);
	}
	if (nfiles == 0) {
		usage();
	}
	return 0;
}

// 引数の "yes", "no" をブール値にして返す
static bool
optbool(const char *arg_)
{
	std::string arg(arg_);

	if (arg == "yes") {
		return true;
	}
	return false;
}

static void
usage()
{
	warnx(R"**(usage: sixelv [color] [size] [algorithm] [colorfind] file...

 color
   --gray={graylevel}
     Select grayscale mode and set grayscale level.
     graylevel allows 2 .. 256. default = 256

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
     Shortcut for -p 8, -p 16, -p 256

   -e
   --monochrome
     Shortcut for -p mono

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

   --axis={ both | w | width | h | height | long | short }

   --loader={ gdk | lib }

   --color-factor={factor}

   --finder={ rgb | hsv | default }

   --addnoise={noiselevel}

 debug
   --debug <0..2>, --profile, --debug-sixel, --debug-http <0..3>
)**");
	exit(1);
}

// プロファイルID
enum {
	Profile_Start,
	Profile_Create,
	Profile_Load,
	Profile_Convert,
	Profile_Output,
	Profile_Max,
};
static const char *profile_name[] = {
	"(Start)",
	"Create",
	"Load",
	"Convert",
	"Output",
};

static void
Convert(const std::string& filename)
{
	// プロファイル時間
	time_point<system_clock> prof[Profile_Max];

	if (opt_profile) {
		prof[Profile_Start] = system_clock::now();
	}

	SixelConverter sx(opt_debug_sixel);
	ImageReductor& ir = sx.GetImageReductor();

	// SixelConverter モード設定
	sx.ColorMode = opt_colormode;
	sx.ReduceMode = opt_reduce;
	sx.ResizeMode = opt_resizemode;
	sx.LoaderMode = opt_loadermode;
	sx.OutputPalette = opt_outputpalette;
	sx.GrayCount = opt_graylevel;
	sx.FinderMode = opt_findermode;
	sx.AddNoiseLevel = opt_addnoise;
	sx.ResizeWidth = opt_width;
	sx.ResizeHeight = opt_height;
	sx.ResizeAxis = opt_resizeaxis;

	ir.HighQualityDiffuseMethod = opt_highqualitydiffusemethod;

	if (opt_ormode) {
		sx.OutputMode = SixelOutputMode::Or;
	} else {
		sx.OutputMode = SixelOutputMode::Normal;
	}

	if (opt_profile) {
		prof[Profile_Create] = system_clock::now();
	}

	// ファイル読み込み

	if (filename == "-") {
		diag.Debug("Loading stdin");
		std::unique_ptr<FileInputStream> stream;
		stream.reset(new FileInputStream(stdin, false));
		if (sx.LoadFromStream(stream.get()) == false) {
			warn("LoadFromStream failed");
			if (opt_ignore_error) {
				return;
			}
			exit(1);
		}
	} else if (filename.find("://") != std::string::npos) {
		diag.Debug("Downloading %s", filename.c_str());
		HttpClient file;
		if (file.Init(diagHttp, filename) == false) {
			warn("File error: %s", filename.c_str());
			if (opt_ignore_error) {
				return;
			}
			exit(1);
		}
		file.family = opt_address_family;
		InputStream *stream = file.GET();
		if (sx.LoadFromStream(stream) == false) {
			warn("Stream error");
			if (opt_ignore_error) {
				return;
			}
			exit(1);
		}
	} else {
		diag.Debug("Loading %s", filename.c_str());
		if (sx.Load(filename) == false) {
			warn("File load error: %s", filename.c_str());
			if (opt_ignore_error) {
				return;
			}
			exit(1);
		}
	}

	if (opt_profile) {
		prof[Profile_Load] = system_clock::now();
	}

	diag.Debug("Converting w=%d, h=%d, axis=%s",
		opt_width, opt_height, ImageReductor::RAX2str(opt_resizeaxis));
	sx.ConvertToIndexed();

	if (opt_profile) {
		prof[Profile_Convert] = system_clock::now();
	}

	if (opt_color_factor != 1.0) {
		ir.ColorFactor(opt_color_factor);
	}

	switch (opt_outputformat) {
	 case OutputFormat::SIXEL:
		signal(SIGINT, signal_handler);
		sx.SixelToFILE(stdout);
		break;
	 case OutputFormat::GVRAM:
	 {
		if (opt_output_x < 0 || opt_output_y < 0) {
			warnx("invalid offset");
			return;
		}
		if (opt_output_y + sx.GetHeight() > 512) {
			warnx("Image height %d is larger than GVRAM",
				opt_output_y + sx.GetHeight());
			return;
		}
		if (ir.GetPaletteCount() <= 16) {
			if (opt_output_x + sx.GetWidth() > 1024) {
				warnx("Image width %d is larger than 16-color mode GVRAM",
					opt_output_x + sx.GetWidth());
				return;
			}
		} else {
			if (opt_output_x + sx.GetWidth() > 512) {
				warnx("Image width %d is larger than 256-color mode GVRAM",
					opt_output_x + sx.GetWidth());
				return;
			}
		}

		std::vector<uint8_t> buf;
		union {
			uint16_t w;
			uint8_t b[2];
		} data;
		// バージョン番号 0x0001 in BE
		data.w = htobe16(0x0001);
		buf.emplace_back(data.b[0]);
		buf.emplace_back(data.b[1]);

		// パレット数
		data.w = htobe16(ir.GetPaletteCount());
		buf.emplace_back(data.b[0]);
		buf.emplace_back(data.b[1]);

		// X68k パレットを作る
		for (int i = 0; i < ir.GetPaletteCount(); i++) {
			auto col = ir.GetPalette(i);
			uint16_t r = col.r >> 3;
			uint16_t g = col.g >> 3;
			uint16_t b = col.b >> 3;
			uint I = (col.r & 0x7) + (col.g & 0x7) + (col.b & 0x7);

			uint8_t h = g << 3 | r >> 2;
			uint8_t l = (r << 6 | b << 1) | (I > (21 / 2) ? 1 : 0);
			buf.emplace_back(h);
			buf.emplace_back(l);
		}
		// x, y, w, h を BE data で出す
		data.w = opt_output_x;
		buf.emplace_back(data.b[0]);
		buf.emplace_back(data.b[1]);
		data.w = opt_output_y;
		buf.emplace_back(data.b[0]);
		buf.emplace_back(data.b[1]);
		data.w = sx.GetWidth();
		buf.emplace_back(data.b[0]);
		buf.emplace_back(data.b[1]);
		data.w = sx.GetHeight();
		buf.emplace_back(data.b[0]);
		buf.emplace_back(data.b[1]);

		fwrite(buf.data(), 1, buf.size(), stdout);

		// GVRAM データを作る
		fwrite(sx.Indexed.data(), 1, sx.Indexed.size(), stdout);
		break;
	 }

	 case OutputFormat::PALETTEPNG:
		// 11 x 11 はどうなのかとか。img2sixel 合わせだが、
		// img2sixel 側の問題でうまくいかない。
		const bool has_alpha = false;
		const int bits_per_sample = 8;
		int width = ir.GetPaletteCount() * 11;
		int height = 11;
		GdkPixbuf *palpix = gdk_pixbuf_new(GDK_COLORSPACE_RGB,
			has_alpha, bits_per_sample, width, height);
		uint8_t *p = gdk_pixbuf_get_pixels(palpix);
		for (int y = 0; y < height; y++) {
			for (int i = 0; i < ir.GetPaletteCount(); i++) {
				for (int x = 0; x < width; x++) {
					auto col = ir.GetPalette(i);
					p[0] = col.r;
					p[1] = col.g;
					p[2] = col.b;
					p += 3;
				}
			}
		}
		GOutputStream *stream = g_unix_output_stream_new(STDOUT_FILENO, false);
		if (!gdk_pixbuf_save_to_stream(palpix, stream, "png", NULL, NULL)) {
			warnx("gdk_pixbuf_save_to_stream faile");
			if (opt_ignore_error) {
				return;
			}
			exit(1);
		}
		break;
	}

	if (opt_profile) {
		prof[Profile_Output] = system_clock::now();
	}

	if (opt_profile) {
		double usec;
		for (int i = 1; i < Profile_Max; i++) {
			usec = duration_cast<microseconds>(prof[i] - prof[i - 1]).count();
			fprintf(stderr, "%-7s %.3fms\n", profile_name[i], usec / 1000);
		}

		auto& start = prof[Profile_Start];
		auto& end   = prof[Profile_Output];
		usec = duration_cast<microseconds>(end - start).count();
		fprintf(stderr, "Total   %.3fms\n", usec / 1000);
	}
}

static void
signal_handler(int signo)
{
	switch (signo) {
	 case SIGINT:
		// SIXEL 出力を中断する (CAN + ST)
		printf(CAN ESC "\\");
		fflush(stdout);
		break;
	}
}
