/*
 * Copyright (C) 2014-2023 Tetsuya Isaki
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
#include "Display.h"
#include "FileStream.h"
#include "HttpClient.h"
#include "JsonFwd.h"
#include "MemoryStream.h"
#include "SixelConverter.h"
#include "subr.h"
#include "term.h"
#include <ctime>

#if !defined(PATH_SEPARATOR)
#define PATH_SEPARATOR "/"
#endif

UseSixel use_sixel;				// SIXEL 画像を表示するかどうか
int  image_count;				// この列に表示している画像の数
int  image_next_cols;			// この列で次に表示する画像の位置(桁数)
int  image_max_rows;			// この列で最大の画像の高さ(行数)
int  max_image_count;			// この列に表示する画像の最大数
bool in_sixel;					// SIXEL 出力中なら true

static bool fetch_image(FileStream& outstream,
	const std::string& img_url, int resize_width);

// 現在行にアイコンを表示。
// 呼び出し時点でカーソルは行頭にあるため、必要なインデントを行う。
// アイコン表示後にカーソル位置を表示前の位置に戻す。
// 実際のアイコン表示そのものはサービスごとに callback(user, userid) で行う。
// userid はキャッシュファイルに使うユーザ名(アカウント名)文字列。
// 呼び出し元ではすでに持ってるはずなので。
// callback() はアイコンを表示できれば true を返すこと。
void
ShowIcon(bool (*callback)(const Json&, const std::string&),
	const Json& user, const std::string& userid)
{
	if ((int)diagImage == 0) {
		// 改行x3 + カーソル上移動x3 を行ってあらかじめスクロールを
		// 発生させ、アイコン表示時にスクロールしないようにしてから
		// カーソル位置を保存する
		// (スクロールするとカーソル位置復元時に位置が合わない)
		printf("\n\n\n" CSI "3A" ESC "7");

		// インデント。
		// CSI."0C" は0文字でなく1文字になってしまうので、必要な時だけ。
		if (indent_depth > 0) {
			int left = indent_cols * indent_depth;
			printf(CSI "%dC", left);
		}
	}

	bool shown = false;
	if (__predict_true(use_sixel != UseSixel::No)) {
		// ここがサービスごとに違う部分。
		// user から実際に画像を表示する。
		shown = callback(user, userid);
	}

	if (__predict_true(shown)) {
		if ((int)diagImage == 0) {
			// アイコン表示後、カーソル位置を復帰
			printf("\r");
			// カーソル位置保存/復元に対応していない端末でも動作するように
			// カーソル位置復元前にカーソル上移動x3を行う
			printf(CSI "3A" ESC "8");
		}
	} else {
		// アイコンを表示してない場合はここで代替アイコンを表示。
		printf(" *");
		// これだけで復帰できるはず
		printf("\r");
	}
}

// 画像 URL からキャッシュファイル名を作成して返す。
std::string
GetCacheFilename(const std::string& img_url)
{
	auto img_file = img_url;

	for (auto p = 0;
		(p = img_file.find_first_of(":/()? ", p)) != std::string::npos;
		p++)
	{
		img_file[p] = '_';
	}

	return img_file;
}

// 画像をキャッシュして表示する。
//  img_file はキャッシュディレクトリ内でのファイル名 (拡張子 .sixel なし)。
//  img_url は画像の URL。
//  resize_width はリサイズ後の画像の幅。ピクセルで指定。0 を指定すると
//  リサイズせずオリジナルのサイズ。
//  index は -1 ならアイコン、0 以上なら添付写真の何枚目かを表す。
//  どちらも位置決めなどのために使用する。
// 表示できれば true を返す。
bool
ShowImage(const std::string& img_file, const std::string& img_url,
	int resize_width, int index)
{
	if (use_sixel == UseSixel::No)
		return false;

	auto cache_filename = cachedir + PATH_SEPARATOR + img_file + ".sixel";
	Debug(diagImage, "%s: img_url=%s", __func__, img_url.c_str());
	Debug(diagImage, "%s: cache_filename=%s", __func__, cache_filename.c_str());

	FileStream cache_file;
	if (cache_file.Open(cache_filename, "r") == false) {
		// キャッシュファイルがないので、画像を取得してキャッシュに保存。
		Debug(diagImage, "%s: sixel cache is not found; fetch the image.",
			__func__);
		if (cache_file.Open(cache_filename, "w+") == false) {
			Debug(diagImage, "%s: cache file '%s': %s", __method__,
				cache_filename.c_str(), strerrno());
			return false;
		}
		if (fetch_image(cache_file, img_url, resize_width) == false) {
			Debug(diagImage, "%s: fetch_image failed\n", __func__);
			return false;
		}
	}

	// SIXEL の先頭付近から幅と高さを取得
	auto sx_width = 0;
	auto sx_height = 0;
	char buf[4096];
	char *ep;
	auto n = cache_file.Read(buf, sizeof(buf));
	if (n < 32) {
		return false;
	}
	// " <Pan>; <Pad>; <Ph>; <Pv>
	int i;
	// Search "
	for (i = 0; i < n && buf[i] != '\x22'; i++)
		;
	// Skip Pan;
	for (i++; i < n && buf[i] != ';'; i++)
		;
	// Skip Pad
	for (i++; i < n && buf[i] != ';'; i++)
		;
	// Ph
	i++;
	sx_width = stou32def(buf + i, -1, &ep);
	if (sx_width < 0) {
		return false;
	}
	// Pv
	i = ep - buf;
	i++;
	sx_height = stou32def(buf + i, -1);
	if (sx_height < 0) {
		return false;
	}

	// この画像が占める文字数
	auto image_rows = (sx_height + fontheight - 1) / fontheight;
	auto image_cols = (sx_width + fontwidth - 1) / fontwidth;

	if (index < 0) {
		// アイコンの場合は呼び出し側で実施。
	} else {
		// 添付画像の場合、表示位置などを計算。
		auto indent = (indent_depth + 1) * indent_cols;
		if ((max_image_count > 0 && image_count >= max_image_count) ||
		    (indent + image_next_cols + image_cols >= screen_cols))
		{
			// 指定された枚数を超えるか、画像が入らない場合は折り返す
			printf("\r");
			printf(CSI "%dC", indent);
			image_count = 0;
			image_max_rows = 0;
			image_next_cols = 0;
		} else {
			// 前の画像の横に並べる
			if (image_count > 0) {
				if (image_max_rows > 0) {
					printf(CSI "%dA", image_max_rows);
				}
				if (image_next_cols > 0) {
					printf(CSI "%dC", image_next_cols);
				}
			}
		}
	}

	// 最初の1回はすでに buf に入っているのでまず出力して、
	// 次からは順次読みながら最後まで出力。
	do {
		in_sixel = true;
		fwrite(buf, 1, n, stdout);
		fflush(stdout);
		in_sixel = false;

		n = cache_file.Read(buf, sizeof(buf));
	} while (n > 0);

	if (index < 0) {
		// アイコンの場合は呼び出し側で実施。
	} else {
		// 添付画像の場合
		image_count++;
		image_next_cols += image_cols;

		// カーソル位置は同じ列に表示した画像の中で最長のものの下端に揃える
		if (image_max_rows > image_rows) {
			printf(CSI "%dB", image_max_rows - image_rows);
		} else {
			image_max_rows = image_rows;
		}
	}

	return true;
}

// 画像をダウンロードして SIXEL に変換して out に書き出す。
// 成功すれば true を、失敗すれば false を返す。
// 成功した場合 out はファイル先頭を指している。
// img_url は画像 URL。
// ただし Blurhash なら独自の blurhash://<encoded> 形式の文字列を渡すこと。
// <encoded> 部分は URL エンコードではなく独自文字列。内容は
// <encoded> := <width> "&" <height> "&" <生blurhash>
// <width> と <height> で入力画像のあるべきサイズを指定する。
//
// resize_width はリサイズすべき幅を指定、0 ならリサイズしない。
static bool
fetch_image(FileStream& outstream, const std::string& img_url, int resize_width)
{
	SixelConverter sx(opt_debug_sixel);

	// 共通の設定
	// 一番高速になる設定
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

	// mem と http が stream を提供するので生存期間に注意。stream は解放不要。
	MemoryStream mem;
	HttpClient http;
	Stream *stream = NULL;
	if (StartWith(img_url, "blurhash://")) {
		// Blurhash は自分で自分のサイズを(アスペクト比すら)持っておらず、
		// 代わりに呼び出し側が独自形式で提供してくれているのでそれを
		// 取り出して、サイズ固定モードで SIXEL にする。うーんこの…。
		char *end;
		const char *p = &img_url[11];
		errno = 0;
		int w = strtol(p, &end, 10);
		if (__predict_false(end == p || *end != '&' || errno != 0)) {
			return false;
		}
		p = end + 1;
		int h = strtol(p, &end, 10);
		if (__predict_false(end == p || *end != '&' || errno != 0)) {
			return false;
		}
		p = end + 1;
		mem.Append(p, img_url.size() - (p - &img_url[0]));
		mem.Rewind();
		stream = &mem;
		// サイズはここで sx にセットする。
		sx.ResizeAxis = ResizeAxisMode::Both;
		sx.ResizeWidth  = w;
		sx.ResizeHeight = h;
	} else {
		http.SetDiag(diagHttp);
		if (http.Open(img_url) == false) {
			return false;
		}
		http.family = address_family;
		http.SetTimeout(opt_timeout_image);
		if (!opt_ciphers.empty()) {
			http.SetCiphers(opt_ciphers);
		}
		stream = http.GET();
		if (stream == NULL) {
			Debug(diagImage, "%s: GET failed", __method__);
			return false;
		}

		// URL の末尾が .jpg とか .png なのに Content-Type が image/* でない
		// (= HTML とか) を返すやつは画像ではないので無視。
		const auto& content_type = http.GetHeader(http.RecvHeaders,
			"Content-Type");
		if (StartWith(content_type, "image/") == false) {
			Debug(diagImage, "%s: Content-type is not an image: %s",
				__method__, content_type.c_str());
			return false;
		}
	}
	if (sx.LoadFromStream(stream) == false) {
		Debug(diagImage, "%s LoadFromStream failed", __func__);
		return false;
	}

	// インデックスカラー変換
	sx.ConvertToIndexed();

	if (sx.SixelToStream(&outstream) == false) {
		Debug(diagImage, "%s: SixelToStream failed", __func__);
		return false;
	}
	outstream.Flush();
	outstream.Rewind();
	return true;
}
