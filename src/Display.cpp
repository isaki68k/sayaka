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
#include "JsonInc.h"
#include "StringUtil.h"
#include "autofd.h"
#include "fetch_image.h"
#include "subr.h"
#include "term.h"
#include <ctime>

#if !defined(PATH_SEPARATOR)
#define PATH_SEPARATOR "/"
#endif

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

// 添付画像を出力する。
// index は画像の番号 (位置決めに使用する)
bool
ShowPhoto(const std::string& img_url, int resize_width, int index)
{
	auto img_file = img_url;

	for (auto p = 0;
		(p = img_file.find_first_of(":/()? ", p)) != std::string::npos;
		p++)
	{
		img_file[p] = '_';
	}

	return ShowImage(img_file, img_url, resize_width, index);
}

// 画像をキャッシュして表示する。
//  img_file はキャッシュディレクトリ内でのファイル名。
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

	std::string img_path = cachedir + PATH_SEPARATOR + img_file;

	Debug(diagImage, "%s: img_url=%s", __func__, img_url.c_str());
	Debug(diagImage, "%s: img_path=%s", __func__, img_path.c_str());
	auto cache_filename = img_path + ".sixel";
	AutoFILE cache_file = fopen(cache_filename.c_str(), "r");
	if (!cache_file.Valid()) {
		// キャッシュファイルがないので、画像を取得
		Debug(diagImage, "sixel cache is not found; fetch the image.");
		cache_file = fetch_image(cache_filename, img_url, resize_width);
		if (!cache_file.Valid()) {
			return false;
		}
	}

	// SIXEL の先頭付近から幅と高さを取得
	auto sx_width = 0;
	auto sx_height = 0;
	char buf[4096];
	char *ep;
	auto n = fread(buf, 1, sizeof(buf), cache_file);
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

		n = fread(buf, 1, sizeof(buf), cache_file);
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

// UNIX 時刻から表示用の文字列を返す。
std::string
formattime(time_t unixtime)
{
	char buf[64];
	struct tm dtm;

	localtime_r(&unixtime, &dtm);

	// 現在時刻
	time_t now = GetUnixTime();
	struct tm ntm;
	localtime_r(&now, &ntm);

	const char *fmt;
	if (dtm.tm_year == ntm.tm_year && dtm.tm_yday == ntm.tm_yday) {
		// 今日なら時刻のみ
		fmt = "%T";
	} else if (dtm.tm_year == ntm.tm_year) {
		// 昨日以前で今年中なら年を省略 (mm/dd HH:MM:SS)
		// XXX 半年以内ならくらいのほうがいいのか?
		fmt = "%m/%d %T";
	} else {
		// 去年以前なら yyyy/mm/dd HH:MM (秒はもういいだろう…)
		fmt = "%Y/%m/%d %R";
	}
	strftime(buf, sizeof(buf), fmt, &dtm);
	return std::string(buf);
}
