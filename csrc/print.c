/* vi:set ts=4: */
/*
 * Copyright (C) 2014-2024 Tetsuya Isaki
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

//
// 表示周り
//

#include "sayaka.h"
#include "image.h"
#include <limits.h>
#include <string.h>

// ヘッダの依存関係を減らすため。
extern image_opt imageopt;

static bool fetch_image(FILE *, const char *, uint);

uint indent_depth;				// 現在のインデント深さ
const char *output_codeset;		// 出力文字コード (NULL なら UTF-8)
bool opt_mathalpha;				// Mathematical AlphaNumeric を全角英数字に変換
bool opt_nocombine;				// Combining Enclosing Keycap を合成しない

// src をインデントをつけて出力する。
void
iprint(const ustring *src)
{
	// Stage1: Unicode 文字単位でいろいろフィルターかける。
	ustring *utext = ustring_init();

	const unichar *s = ustring_get(src);
	uint srclen = ustring_len(src);

	if (0) {
		char header[32];
		snprintf(header, sizeof(header), "%s src", __func__);
		ustring_dump(src, header);
	}

	for (uint i = 0; i < srclen; i++) {
		unichar uni = s[i];

		// Private Use Area (外字) をコードポイント形式(?)にする
		if (__predict_false((  0xe000 <= uni && uni <=   0xf8ff))	// BMP
		 || __predict_false(( 0xf0000 <= uni && uni <=  0xffffd))	// 第15面
		 || __predict_false((0x100000 <= uni && uni <= 0x10fffd))) 	// 第16面
		{
			char buf[16];
			snprintf(buf, sizeof(buf), "<U+%X>", uni);
			ustring_append_ascii(utext, buf);
			continue;
		}

		// Mathematical Alphanumeric Symbols を全角英数字に変換
		if (__predict_false(opt_mathalpha) &&
			__predict_false(0x1d400 <= uni && uni <= 0x1d7ff))
		{
#if 0
			// Mathematical Alphanumeric Symbols を全角英数字に変換
			utext.Append(ConvMathAlpha(uni));
#endif
			continue;
		}

		// --no-combine なら Combining Enclosing * (U+20DD-U+20E4) の前に
		// スペースを入れて、囲まれるはずだった文字とは独立させる。
		// 前の文字(たいていただの ASCII 数字)が潰されて読めなくなるのを
		// 防ぐため。
		// U+20E1 は「上に左右矢印を前の文字につける」で囲みではないが
		// 面倒なので混ぜておく。なぜ間に入れたのか…。
		if (opt_nocombine &&
			__predict_false(0x20dd <= uni && uni <= 0x20e4))
		{
			ustring_append_unichar(utext, 0x20);
		}

		if (__predict_false(output_codeset)) {
			// JIS/EUC-JP(/Shift-JIS) に変換する場合のマッピング
			// 本当は変換先がこれらの時だけのほうがいいだろうけど。
#if 0
#endif
		}

		ustring_append_unichar(utext, uni);
	}

	if (0) {
		char header[32];
		snprintf(header, sizeof(header), "%s utext", __func__);
		ustring_dump(utext, header);
	}

	// Stage2: インデントつけていく。
	ustring *utext2 = ustring_alloc(ustring_len(utext) + 32);

	// インデント階層
	uint left = indent_cols * (indent_depth + 1);
	char indent[12];
	{
		char *p = indent;
		*p++ = ESCchar;
		*p++ = '[';
		p += PUTD(p, left, sizeof(indent) - (p - indent));
		*p++ = 'C';
		*p = '\0';
	}
	ustring_append_ascii(utext2, indent);

	if (__predict_false(screen_cols == 0)) {
		// 桁数が分からない場合は何もしない
		ustring_append(utext2, utext);
	} else {
		// 1文字ずつ文字幅を数えながら出力用に整形していく
		uint in_escape = 0;
		uint x = left;
		const unichar *utextbuf = ustring_get(utext);
		unichar uni;
		for (int i = 0; (uni = utextbuf[i]) != 0; i++) {
			if (__predict_false(in_escape > 0)) {
				// 1: ESC直後
				// 2: ESC [
				// 3: ESC (
				ustring_append_unichar(utext2, uni);
				switch (in_escape) {
				 case 1:
					// ESC 直後の文字で二手に分かれる
					if (uni == '[') {
						in_escape = 2;
					} else {
						in_escape = 3;	// 手抜き
					}
					break;
				 case 2:
					// ESC [ 以降 'm' まで
					if (uni == 'm') {
						in_escape = 0;
					}
					break;
				 case 3:
					// ESC ( の次の1文字だけ
					in_escape = 0;
					break;
				}
			} else {
				if (uni == ESCchar) {
					ustring_append_unichar(utext2, uni);
					in_escape = 1;
				} else if (uni == '\n') {
					ustring_append_unichar(utext2, uni);
					ustring_append_ascii(utext2, indent);
					x = left;
				} else {
					// 文字幅を取得
#if 0
					uint width = get_eaw_width(uni);
#else
					uint width = (uni < 0x80) ? 1 : 2;
#endif
					if (width == 1) {
						ustring_append_unichar(utext2, uni);
						x++;
					} else {
						assert(width == 2);
						if (x > screen_cols - 2) {
							ustring_append_unichar(utext2, '\n');
							ustring_append_ascii(utext2, indent);
							x = left;
						}
						ustring_append_unichar(utext2, uni);
						x += 2;
					}
				}
				if (x > screen_cols - 1) {
					ustring_append_unichar(utext2, '\n');
					ustring_append_ascii(utext2, indent);
					x = left;
				}
			}

			// デバッグ用
			if (0) {
				printf("[%d] U+%04x, x = %d", i, uni, x);
				if (uni == ESCchar) {
					printf(" ESC");
				} else if (uni == '\n') {
					printf(" '\\n'");
				} else if (0x20 <= uni && uni < 0x7f) {
					printf(" '%c'", uni);
				}
				printf("\n");
			}
		}
	}

	// 出力文字コードに変換。
	string *outstr = ustring_to_utf8(utext2);
	fputs(string_get(outstr), stdout);

	ustring_free(utext);
	ustring_free(utext2);
}

// 画像をキャッシュして表示する。
// img_file はキャッシュディレクトリ内でのファイル名 (拡張子 .sixel なし)。
// img_url は画像の URL。
// resize_width は画像の表示幅。
// index は -1 ならアイコン、0 以上なら添付写真の何枚目かを表す。
// どちらも位置決めなどのために使用する。
// 表示できれば true を返す。
bool
show_image(const char *img_file, const char *img_url, uint width, int index)
{
	char cache_filename[PATH_MAX];
	FILE *fp;
	uint sx_width;
	uint sx_height;
	char buf[4096];
	char *next;
	uint i;
	uint n;
	bool rv = false;

	snprintf(cache_filename, sizeof(cache_filename),
		"%s/%s.sixel", cachedir, img_file);

	fp = fopen(cache_filename, "r");
	if (fp == NULL) {
		// キャッシュファイルがないので、画像を取得してキャッシュに保存。

		fp = fopen(cache_filename, "w+");
		if (fp == NULL) {
			Debug(diag_image, "%s: cache file '%s': %s", __func__,
				cache_filename, strerrno());
			return false;
		}

		if (fetch_image(fp, img_url, width) == false) {
			Debug(diag_image, "%s: fetch_image failed", __func__);
			goto abort;
		}

		fseek(fp, 0, SEEK_SET);
	}

	// SIXEL の先頭付近から幅と高さを取得。

	n = fread(buf, 1, sizeof(buf), fp);
	if (n < 32) {
		Debug(diag_image, "%s: %s: file too short(n=%u)", __func__,
			cache_filename, n);
		goto abort;
	}
	// 先頭から少しのところに '"' <Pan> ';' <Pad> ';' <Ph> ';' <Pv>
	// Search '"'
	for (i = 0; i < n && buf[i] != '\x22'; i++)
		;
	// Skip <Pan>
	for (i++; i < n && buf[i] != ';'; i++)
		;
	// Skip <Pad>
	for (i++; i < n && buf[i] != ';'; i++)
		;
	// Obtain <Ph>
	i++;
	sx_width = stou32def(&buf[i], -1, &next);
	// Obtain <Pv>
	sx_height = stou32def(next + 1, -1, NULL);
	if ((int)sx_width < 0 || (int)sx_height < 0) {
		Debug(diag_image, "%s: %s: could not read size in SIXEL",
			__func__, cache_filename);
		goto abort;
	}

	// この画像が占める文字数。
	//uint image_rows = (sx_height + fontheight - 1) / fontheight;
	//uint image_cols = (sx_width + fontwidth - 1) / fontwidth;

	if (index < 0) {
		// アイコンの場合は呼び出し側で実施。
	} else {
		// 添付画像の場合、表示位置などを計算。
	}

	// 最初の1回はすでに buf に入っているのでまず出力して、
	// 次からは順次読みながら最後まで出力。
	do {
		in_sixel = true;
		fwrite(buf, 1, n, stdout);
		fflush(stdout);
		in_sixel = false;

		n = fread(buf, 1, sizeof(buf), fp);
	} while (n > 0);

	if (index < 0) {
		// アイコンの場合は呼び出し側で実施。
	} else {
		// 添付画像の場合
	}

	rv = true;
 abort:
	fclose(fp);
	return rv;
}

// img_url から画像をダウンロードして、
// 長辺を size [pixel] にリサイズして、
// SIXEL 形式に変換して ofp に出力する。
// 出力できれば true を返す。
static bool
fetch_image(FILE *ofp, const char *img_url, uint size)
{
	struct netstream *net = NULL;
	pstream *pstream = NULL;
	FILE *ifp = NULL;
	image *srcimg = NULL;
	image *dstimg = NULL;
	uint dst_width;
	uint dst_height;
	bool rv = false;

	imageopt.width  = size;
	imageopt.height = size;

	if (strncmp(img_url, "http://",  7) == 0 ||
		strncmp(img_url, "https://", 8) == 0)
	{
#if defined(HAVE_LIBCURL)
		net = netstream_init(diag_net);
		if (net == NULL) {
			Debug(diag_net, "%s: netstream_init failed", __func__);
			return false;
		}
		int code = netstream_connect(net, img_url, &netopt);
		if (code < 0) {
			Debug(diag_net, "%s: %s: netstream_connect failed: %s", __func__,
				img_url, strerrno());
			goto abort;
		} else if (code == 1) {
			Debug(diag_net, "%s: %s: connection failed: %s", __func__,
				img_url, strerrno());
			goto abort;
		} else if (code >= 400) {
			Debug(diag_net, "%s: %s: connection failed: HTTP %u", __func__,
				img_url, code);
			goto abort;
		}
		ifp = netstream_fopen(net);
		if (ifp == NULL) {
			Debug(diag_net, "%s: netstream_fopen failed: %s", __func__,
				strerrno());
			goto abort;
		}
#else
		Debug(diag_net, "%s: Network support has not been compiled", __func__);
		return false;
#endif
	}

	// ifp からピークストリームを作成。
	pstream = pstream_init_fp(ifp);
	if (pstream == NULL) {
		Debug(diag_net, "%s: pstream_init_fp failed: %s", __func__, strerrno());
		goto abort;
	}

	// 画像読み込み。
	srcimg = image_read_pstream(pstream, &imageopt, diag_image);
	if (srcimg == NULL) {
		Debug(diag_image, "%s: image_read_pstream failed: %s", __func__,
			strerrno());
		goto abort;
	}

	// いい感じにサイズを決定。
	image_get_preferred_size(srcimg->width, srcimg->height,
		ResizeAxis_ScaleDownLong, imageopt.width, imageopt.height,
		&dst_width, &dst_height);

	// 減色 & リサイズ。
	dstimg = image_reduct(srcimg, dst_width, dst_height, &imageopt, diag_image);
	if (dstimg == NULL) {
		Debug(diag_image, "%s: image_reduct failed", __func__);
		goto abort;
	}

	// 出力。
	if (image_sixel_write(ofp, dstimg, &imageopt, diag_image) == false) {
		Debug(diag_image, "%s: image_sixel_write failed", __func__);
		goto abort;
	}
	fflush(ofp);

	rv = true;
 abort:
	image_free(dstimg);
	image_free(srcimg);
	pstream_cleanup(pstream);
	if (ifp) {
		fclose(ifp);
	}
	netstream_cleanup(net);
	return rv;
}
