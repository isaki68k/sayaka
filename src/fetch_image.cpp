#include "FileStream.h"
#include "HttpClient.h"
#include "SixelConverter.h"
#include "StringUtil.h"
#include "fetch_image.h"
#include "main.h"

// 画像をダウンロードして SIXEL に変換してキャッシュする。
// 成功すれば、書き出したキャッシュファイルの FILE* (位置は先頭) を返す。
// 失敗すれば NULL を返す。
// cache_filename はキャッシュするファイルのファイル名
// img_url は画像 URL
// resize_width はリサイズすべき幅を指定、0 ならリサイズしない
FILE *
fetch_image(const std::string& cache_filename, const std::string& img_url,
	int resize_width)
{
	SixelConverter sx(opt_debug_sixel);

	// 共通の設定
	// 一番高速になる設定
	sx.LoaderMode = SixelLoaderMode::Lib;
	sx.ResizeMode = SixelResizeMode::ByLoad;
	// 縮小するので X68k でも画質 High でいける
	sx.ReduceMode = ReductorReduceMode::HighQuality;
	// 縮小のみの長辺指定変形。
	// height にも resize_width を渡すことで長辺を resize_width に
	// 制限できる。この関数の呼び出し意図がそれを想定している。
	// もともと幅しか指定できなかった経緯があり、
	// 本当は width/height をうまく分離すること。
	sx.ResizeWidth = resize_width;
	sx.ResizeHeight = resize_width;
	sx.ResizeAxis = ResizeAxisMode::ScaleDownLong;

	if (color_mode == ColorFixedX68k) {
		// とりあえず固定 16 色
		// システム取得する?
		sx.ColorMode = ReductorColorMode::FixedX68k;
	} else {
		if (color_mode <= 2) {
			sx.ColorMode = ReductorColorMode::Mono;
		} else if (color_mode < 8) {
			sx.ColorMode = ReductorColorMode::Gray;
			// グレーの場合の色数として colormode を渡す
			sx.GrayCount = color_mode;
		} else if (color_mode < 16) {
			sx.ColorMode = ReductorColorMode::Fixed8;
		} else if (color_mode < 256) {
			sx.ColorMode = ReductorColorMode::FixedANSI16;
		} else {
			sx.ColorMode = ReductorColorMode::Fixed256;
		}
	}
	if (opt_ormode) {
		sx.OutputMode = SixelOutputMode::Or;
	} else {
		sx.OutputMode = SixelOutputMode::Normal;
	}
	sx.OutputPalette = opt_output_palette;

	HttpClient fg;
	if (fg.Init(diagHttp, img_url) == false) {
		return NULL;
	}
	fg.family = address_family;
	fg.SetTimeout(opt_timeout_image);
	InputStream *stream = fg.GET();
	if (stream == NULL) {
		diag.Debug("Warning: fetch_image GET failed");
		return NULL;
	}

	// URL の末尾が .jpg とか .png なのに Content-Type が image/* でない
	// (= HTML とか) を返すやつは画像ではないので無視。
	const auto& content_type = fg.GetHeader(fg.RecvHeaders, "Content-Type");
	if (StartWith(content_type, "image/") == false) {
		return NULL;
	}
	if (sx.LoadFromStream(stream) == false) {
		diag.Debug("Warning: fetch_image LoadFromStream failed");
		return NULL;
	}

	// インデックスカラー変換
	sx.ConvertToIndexed();

	FILE *fp = fopen(cache_filename.c_str(), "w+");
	FileOutputStream outstream(fp, false);
	sx.SixelToStream(&outstream);
	fseek(fp, 0, SEEK_SET);
	return fp;
}
