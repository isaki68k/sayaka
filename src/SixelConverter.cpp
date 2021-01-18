#include "FileStream.h"
#include "SixelConverter.h"
#include "sayaka.h"

extern int sixel_image_to_sixel_h6_ormode(uint8* dst, uint8* src,
	int w, int h, int plane_count);

static int  img_readcallback(ImageReductor::Image *img);
static void img_freecallback(guchar *pixels, gpointer data);

// コンストラクタ
SixelConverter::SixelConverter()
{
	diag.SetClassname("SixelConverter");
}

// コンストラクタ
SixelConverter::SixelConverter(int debuglv)
	: SixelConverter()
{
	diag.SetLevel(debuglv);
}

//
// 画像の読み込み
//

bool
SixelConverter::Load(const std::string& filename)
{
	diag.Debug("filename=%s", filename.c_str());
	diag.Debug("LoaderMode=%s", SLM2str(LoaderMode));
	diag.Debug("ResizeMode=%s", SRM2str(ResizeMode));

	if (LoaderMode == SixelLoaderMode::Lib) {
		FILE *fp = fopen(filename.c_str(), "r");
		if (fp == NULL) {
			return false;
		}
		bool loaded = LoadJpeg(fp);
		fclose(fp);
		if (loaded) {
			LoadAfter();
			return true;
		} else {
			diag.Debug("fallback to gdk");
		}
	}

#if 1
	printf("%s gdk not implemented\n", __func__);
	return false;
#else
	if (ResizeMode == SixelResizeMode::ByLoad) {
		int width = -1;
		int height = -1;
		CalcResizeGdkLoad(&width, &height);
		pix = gdk_pixbuf_new_from_file_at_size(filename.c_str(), width, height,
			&gerror);
		if (__predict_false(pix == NULL)) {
			diag.Debug("gdk_pixbuf_new_from_file_at_size failed");
			return false;
		}
	} else {
		pix = gdk_pixbuf_new_from_file(filename.c_str(), &gerror);
		if (__predict_false(pix == NULL)) {
			diag.Debug("gdk_pixbuf_new_from_file failed");
			return false;
		}
	}
	LoadAfter();
	return true;
#endif
}

bool
SixelConverter::LoadFromStream(InputStream *stream)
{
	diag.Debug("LoaderMode=%d", LoaderMode);
	diag.Debug("ResizeMode=%d", ResizeMode);

	if (LoaderMode == SixelLoaderMode::Lib) {
		FileInputStream *fstream = dynamic_cast<FileInputStream *>(stream);
		if (fstream == NULL) {
			return false;
		}
		if (LoadJpeg(fstream->fp) == true) {
			LoadAfter();
			return true;
		} else {
			diag.Debug("fallback to gdk");
		}
	}

#if 1
	printf("%s not implemented\n", __func__);
	return false;
#else
	GInputStream *gstream = g_unix_input_stream_new(fileno(fp), false);
	if (ResizeMode == SixelResizeMode::ByLoad) {
		int width = -1;
		int height = -1;
		CalcResizeGdkLoad(&width, &height);
		pix = gdk_pixbuf_from_stream_at_scale(gstream, width, height, true);
		if (pix == NULL) {
			diag.Debug("gdk_pixbuf_from_stream_at_scale() failed");
			return false;
		}
	} else {
		pix = gtk_pixbuf_from_stream(gstream);
		if (pix == NULL) {
			diag.Debug("gdk_pixbuf_from_stream() failed");
			return false;
		}
	}
	LoadAfter();
	return true;
#endif
}

bool
SixelConverter::LoadJpeg(FILE *fp)
{
	uint8 magic[2];

	// マジックを読んで..
	auto n = fread(magic, 1, sizeof(magic), fp);
	if (n < sizeof(magic)) {
		diag.Debug("fread(magic) failed: %s", strerror(ferror(fp)));
		return false;
	}
	// fp を戻す
	fseek(fp, 0, SEEK_SET);

	// マジックを確認
	if (magic[0] != 0xff || magic[1] != 0xd8) {
		diag.Debug("Bad magic");
		return false;
	}

	img = ImageReductor::AllocImage();
	img->ReadCallback = img_readcallback;
	img->UserObject = fp;

	auto r = ImageReductor::LoadJpeg(img,
		ResizeWidth, ResizeHeight, ResizeAxis);
	if (r != ReductorImageCode::RIC_OK) {
		diag.Debug("ImageReductor::LoadJpeg failed");
		return false;
	}
	diag.Debug("img.Width=%d img.Height=%d img.RowStride=%d",
		img->Width, img->Height, img->RowStride);
	pix = gdk_pixbuf_new_from_data(
		img->Data,
		GDK_COLORSPACE_RGB,
		false,	// has_alpha
		8,		// bits_per_sample
		img->Width,
		img->Height,
		img->RowStride,
		img_freecallback, NULL);
	return true;
}

void
SixelConverter::LoadAfter()
{
	Width = gdk_pixbuf_get_width(pix);
	Height = gdk_pixbuf_get_height(pix);
	diag.Debug("Size=(%d,%d) bits=%d nCh=%d rowstride=%d",
		Width, Height,
		gdk_pixbuf_get_bits_per_sample(pix),
		gdk_pixbuf_get_n_channels(pix),
		gdk_pixbuf_get_rowstride(pix));
}

static int
img_readcallback(ImageReductor::Image *img)
{
	FILE *fp = (FILE *)(img->UserObject);
	return fread(img->ReadBuffer, 1, sizeof(img->ReadBuffer), fp);
}

static void
img_freecallback(guchar *pixels, gpointer data)
{
	ImageReductor::Image *img = (ImageReductor::Image *)data;
	if (img != NULL) {
		ImageReductor::FreeImage(img);
	}
}

//
// ----- リサイズ計算
//

// Loader::Gdk の時、scale に渡す幅と高さを計算する
void
SixelConverter::CalcResizeGdkLoad(int *width, int *height)
{
}

// Loader::Gdk 以外の時のリサイズ計算
void
SixelConverter::CalcResize(int *widthp, int *heightp)
{
	int& width = *widthp;
	int& height = *heightp;

	auto ra = ResizeAxis;
	bool scaledown =
		(ra == ResizeAxisMode::ScaleDownBoth)
	 || (ra == ResizeAxisMode::ScaleDownWidth)
	 || (ra == ResizeAxisMode::ScaleDownHeight)
	 || (ra == ResizeAxisMode::ScaleDownLong)
	 || (ra == ResizeAxisMode::ScaleDownShort);

	// 条件を丸めていく
	switch (ra) {
	 case ResizeAxisMode::Both:
	 case ResizeAxisMode::ScaleDownBoth:
		if (ResizeWidth == 0) {
			ra = ResizeAxisMode::Height;
		} else if (ResizeHeight == 0) {
			ra = ResizeAxisMode::Width;
		} else {
			ra = ResizeAxisMode::Both;
		}
		break;

	 case ResizeAxisMode::Long:
	 case ResizeAxisMode::ScaleDownLong:
		if (Width >= Height) {
			ra = ResizeAxisMode::Width;
		} else {
			ra = ResizeAxisMode::Height;
		}
		break;

	 case ResizeAxisMode::Short:
	 case ResizeAxisMode::ScaleDownShort:
		if (Width <= Height) {
			ra = ResizeAxisMode::Width;
		} else {
			ra = ResizeAxisMode::Height;
		}
		break;

	 case ResizeAxisMode::ScaleDownWidth:
		ra = ResizeAxisMode::Width;
		break;

	 case ResizeAxisMode::ScaleDownHeight:
		ra = ResizeAxisMode::Height;
		break;

	 default:
		__builtin_unreachable();
		break;
	}

	auto rw = ResizeWidth;
	auto rh = ResizeHeight;

	if (rw <= 0)
		rw = Width;
	if (rh <= 0)
		rh = Height;

	// 縮小のみ指示
	if (scaledown) {
		if (Width < rw)
			rw = Width;
		if (Height < rh)
			rh = Height;
	}

	// 確定したので計算
	switch (ra) {
	 case ResizeAxisMode::Both:
		width = rw;
		height = rh;
		break;
	 case ResizeAxisMode::Width:
		width = rw;
		height = Height * width / Width;
		break;
	 case ResizeAxisMode::Height:
		height = rh;
		width = Width * height / Height;
		break;
	 default:
		__builtin_unreachable();
		break;
	}
}

//
// ----- 前処理
//

// インデックスカラーに変換する。
void
SixelConverter::ConvertToIndexed()
{
	// リサイズ
	int width = 0;
	int height = 0;
	CalcResize(&width, &height);

	diag.Debug("resize to (%d, %d)\n", width, height);

	if (ResizeMode == SixelResizeMode::ByScaleSimple) {
		if (width == Width || height == Height) {
			diag.Debug("no need to resize");
		} else {
			// GdkPixbuf で事前リサイズする。
			// ImageReductor は減色とリサイズを同時実行できるので、
			// 事前リサイズは品質の問題が出た時のため。
			GdkPixbuf *pix2 = gdk_pixbuf_scale_simple(pix, width, height,
				GDK_INTERP_BILINEAR);
			diag.Debug("scale_simple resized");

			// 差し替える
			g_object_unref(pix);
			pix = pix2;
		}
	}

	Width = width;
	Height = height;

	Indexed.resize(Width * Height);

	ImageReductor ir;

	diag.Debug("SetColorMode(%s, %s, %d)",
		ImageReductor::RCM2str(ColorMode),
		ImageReductor::RFM2str(FinderMode),
		GrayCount);
	ir.SetColorMode(ColorMode, FinderMode, GrayCount);

	diag.Debug("SetAddNoiseLevel=%d", AddNoiseLevel);
	ir.SetAddNoiseLevel(AddNoiseLevel);

	diag.Debug("ReduceMode=%s", ImageReductor::RRM2str(ReduceMode));
	ir.Convert(ReduceMode, pix, Indexed, Width, Height);
	diag.Debug("Converted");
}

void
SixelConverter::SixelToFILE(FILE *stream)
{
	printf("%s not implemented\n", __func__);
}


//
// enum を文字列にしたやつ orz
//

static const char *SLM2str_[] = {
	"Gdk",
	"Lib",
};

static const char *SRM2str_[] = {
	"ByLoad",
	"ByScaleSimple",
	"ByImageReductor",
};

/*static*/ const char *
SixelConverter::SOM2str(SixelOutputMode val)
{
	if (val == Normal)	return "Normal";
	if (val == Or)		return "Or";
	return "?";
}

/*static*/ const char *
SixelConverter::SLM2str(SixelLoaderMode val)
{
	return ::SLM2str_[(int)val];
}

/*static*/ const char *
SixelConverter::SRM2str(SixelResizeMode val)
{
	return ::SRM2str_[(int)val];
}

#if defined(SELFTEST)
#include "test.h"
static void
test_enum()
{
	std::vector<std::pair<SixelOutputMode, const std::string>> table_SOM = {
		{ SixelOutputMode::Normal,			"Normal" },
		{ SixelOutputMode::Or,				"Or" },
	};
	for (const auto& a : table_SOM) {
		const auto n = a.first;
		const auto& exp = a.second;
		std::string act(SixelConverter::SOM2str(n));
		xp_eq(exp, act, exp);
	}

	std::vector<std::pair<SixelLoaderMode, const std::string>> table_SLM = {
		{ SixelLoaderMode::Gdk,				"Gdk" },
		{ SixelLoaderMode::Lib,				"Lib" },
	};
	for (const auto& a : table_SLM) {
		const auto n = a.first;
		const auto& exp = a.second;
		std::string act(SixelConverter::SLM2str(n));
		xp_eq(exp, act, exp);
	}

	std::vector<std::pair<SixelResizeMode, const std::string>> table_SRM = {
		{ SixelResizeMode::ByLoad,			"ByLoad" },
		{ SixelResizeMode::ByScaleSimple,	"ByScaleSimple" },
		{ SixelResizeMode::ByImageReductor,	"ByImageReductor" },
	};
	for (const auto& a : table_SRM) {
		const auto n = a.first;
		const auto& exp = a.second;
		std::string act(SixelConverter::SRM2str(n));
		xp_eq(exp, act, exp);
	}
}

void
test_SixelConverter()
{
	test_enum();
}
#endif
