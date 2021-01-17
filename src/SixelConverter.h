#pragma once

#include "Diag.h"
#include "ImageReductor.h"
#include "StreamBase.h"
#include <vector>
#include <gdk-pixbuf/gdk-pixbuf.h>

// SIXEL 出力モード。
// SIXEL のカラーモード値と同じにする。
enum SixelOutputMode {
	Normal = 1,	// 通常の SIXEL を出力する。
	Or = 5,		// OR モード SIXEL を出力する。
};

// ローダモード。
// 画像のロードに使うライブラリを指定する。
enum SixelLoaderMode
{
	// gdk pixbuf を使用する。
	Gdk,

	// 個別ライブラリを使用する。
	// 現在は jpeg 画像で、libjpeg を使用する。
	// 将来的に libpng などをサポートしたときもこのフラグになる予定。
	// 個別ライブラリが対応していないフォーマットの場合は
	// gdk pixbuf にフォールバックする。
	Lib,
};

// リサイズモード
enum SixelResizeMode
{
	// リサイズ処理をロード時にライブラリで行う。
	ByLoad,

	// ロードは等倍で行い、その後にリサイズ処理を
	// Gdk.Pixbuf.scale_simple で行う。
	ByScaleSimple,

	// ロードは等倍で行い、その後にリサイズ処理を ImageReductor で行う。
	ByImageReductor,
};

class SixelConverter
{
 public:
	// 設定

	// Sixel の出力カラーモード値
	SixelOutputMode OutputMode = SixelOutputMode::Normal;

	// Sixel にパレットを出力するなら true
	bool OutputPalette = true;

	// カラーモード
	ReductorColorMode ColorMode = ReductorColorMode::Fixed256;

	// ファインダーモード
	ReductorFinderMode FinderMode = ReductorFinderMode::RFM_Default;

	// グレーカラーの時の色数。グレー以外の時は無視される。
	int GrayCount = 256;

	// 減色モード
	ReductorReduceMode ReduceMode = ReductorReduceMode::HighQuality;

	// リサイズモード
	SixelResizeMode ResizeMode = SixelResizeMode::ByLoad;

	// ローダモード
	SixelLoaderMode LoaderMode = SixelLoaderMode::Gdk;

	// ノイズ付加
	// ベタ塗り画像で少ない色数に減色する時、ノイズを付加すると画質改善出来る
	int AddNoiseLevel {};

	// リサイズ情報 (リサイズで希望する幅と高さ)。
	// 0 を指定するとその情報は使われない。
	int ResizeWidth {};
	int ResizeHeight {};

	// リサイズ処理で使用する軸
	ResizeAxisMode ResizeAxis = ResizeAxisMode::Both;

 public:
	// コンストラクタ
	SixelConverter();
	SixelConverter(int debuglv);

	bool Load(const std::string& filename);

	bool LoadFromStream(InputStream *stream);

	bool LoadJpeg(FILE *fp);

	// インデックスカラーに変換する
	void ConvertToIndexed();

	// Sixel を FILE * に出力する
	void SixelToFILE(FILE *fp);

	// ImageReductor を取得する
	ImageReductor& GetImageReductor() { return ir; }

 public:
	// 元画像
	GdkPixbuf *pix {};

	// インデックスカラー画像バッファ
	std::vector<uint8> Indexed {};

	// 画像の幅・高さを取得
	int GetWidth() const { return Width; }
	int GetHeight() const { return Height; }

 private:
	bool LoadJpeg(InputStream *stream);

	void LoadAfter();

	void CalcResizeGdkLoad(int *width, int *height);
	void CalcResize(int *width, int *height);

	// 画像の幅と高さ
	int Width;
	int Height;

	ImageReductor ir {};

	ImageReductor::Image *img {};

	Diag diag;
};
