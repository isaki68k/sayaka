/* vi:set ts=4: */
/*
 * Copyright (C) 2026 Tetsuya Isaki
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
// ICO 読み込み
//

// .ICO ファイルは、大きく以下のような構造。
// - ファイルヘッダ (6バイト)
// - ディレクトリ (#0..#n)
// - データ (#0..#n)
//
// ファイルヘッダは以下の6バイト。
// +$00.w 予約(0)
// +$02.w 種類(1=ICO, 2=CUR)
// +$04.w 画像数
//
// ディレクトリは以下の16バイトがアイコン数分並ぶ。
// +$00.b: 画像の幅 (0 なら 256px)
// +$01.b: 画像の高さ (0 なら 256px)
// +$02.b: パレット数 (0 ならパレットなし)
// +$03.b: reserved
// +$04.w: ICO ならカラープレーン数(1)、CUR ならホットスポットX
// +$06.w: ICO ならピクセル毎のビット数、CUR ならホットスポットY
// +$08.l: データのバイト数
// +$0c.l: データ位置のファイル先頭からのオフセット
//
// データは BMP (風) と PNG の2種類がある。
// BMP 風は、BMP っぽいけど BMP ファイルとは異なり、微妙に都合のいいところ
// だけ使いまわした、下位互換でもない BMP モドキみたいな形式。
//
//   通常の BMP ファイル               ICO のデータ領域(BMP)
// +-------------------------------+
// | BITMAPFILEHEADER              |   (BITMAPFILEHEADERなし)
// +-------------------------------+  +--------------------------------+
// | DIB (以下のうちいずれか)      |  | BITMAPINFOHEADER 固定          |
// | o BITMAPCOREHEADER            |  | (圧縮方式は BI_RGB のみ。      |
// | o BITMAPINFOHEADER            |  | BI_BITFIELDS は来ないので      |
// |   :                           |  | RGBマスク情報ブロックも来ない) |
// +-------------------------------+  |                                |
// | BITMAPINFOHEADER で圧縮方式が |  |                                |
// | BI_BITFIELDSならRGBマスク情報 |  |                                |
// +-------------------------------+  +--------------------------------+
// | biClrUsed <= 256 ならパレット |  | biClrUsed <= 256 ならパレット  |
// +-------------------------------+  +--------------------------------+
// | ピクセルデータ                |  | ピクセルデータ                 |
// +-------------------------------+  +--------------------------------+
//                                    | マスクデータ (1bit/pixel)      |
//                                    +--------------------------------+
//
// PNG の場合は、データ領域がそのまま PNG 形式。
// この両者は先頭付近のマジックの違いで区別する。
// PNG なら先頭は $89, 'P', 'N', 'G', ... で始まる。
// BMP (風) なら先頭は BITMAPINFOHEADER で、その先頭フィールドは自身の
// ヘッダ長 (40バイト) なので $28, $00, $00, $00 となる。

#include "common.h"
#include "image_priv.h"
#include "image_bmp.h"
#include <err.h>
#include <string.h>

// ICO ファイルヘッダ
struct icohdr
{
	uint16 reserved;
	uint16 type;
	uint16 nfiles;
} __packed;

// ICO ファイルのディレクトリ構造。
// この構造体はファイルから読んでエンディアンを直したホスト側の構造。
struct icodir
{
	uint width;
	uint height;
	uint ncolors;
	uint colorbits;
	uint32 datalen;
	uint32 dataoff;
};

static bool ico_read_dir(FILE *, struct icodir *);
static struct image *ico_read_data(FILE *, const struct icodir *, uint,
	const struct diag *);
static struct image *ico_read_bmp(FILE *, const struct icodir *,
	const struct diag *);
static struct image *ico_read_png(FILE *, const struct icodir *,
	const struct diag *);
static int  raster_icomask1(struct bmpctx *, int);

bool
image_ico_match(FILE *fp, const struct diag *diag)
{
	struct icohdr hdr;

	size_t n = fread(&hdr, sizeof(hdr), 1, fp);
	if (n == 0) {
		Debug(diag, "%s: fread(magic) failed: %s", __func__, strerrno());
		return false;
	}

	uint32 resv = le16toh(hdr.reserved);
	uint32 type = le16toh(hdr.type);
	if (resv != 0) {
		return false;
	}
	if (type != 1/*ICO*/ && type != 2/*CUR*/) {
		return false;
	}

	return true;
}

struct image *
image_ico_read(FILE *fp, const image_read_hint *hint, const struct diag *diag)
{
	struct icohdr hdr;
	struct icodir *dirs;
	struct image *img;
	size_t n;

	dirs = NULL;
	img = NULL;

	// ヘッダ
	n = fread(&hdr, sizeof(hdr), 1, fp);
	if (n == 0) {
		warn("%s: fread(header) failed", __func__);
		return NULL;
	}
	// このファイルの画像数。
	uint nfiles = le16toh(hdr.nfiles);
	Debug(diag, "%s: total icons = %u", __func__, nfiles);

	// 画像数分のディレクトリが続く。
	size_t dirlen = sizeof(struct icodir) * nfiles;
	dirs = malloc(dirlen);
	if (dirs == NULL) {
		warn("%s: malloc(%zu) failed", __func__, dirlen);
		return NULL;
	}
	for (uint i = 0; i < nfiles; i++) {
		struct icodir *dir = &dirs[i];
		if (ico_read_dir(fp, dir) == false) {
			warn("%s: fread(icon dir#%u) failed", __func__, i);
			goto abort;
		}
		char colbuf[32];
		if (dir->ncolors == 0) {
			strlcpy(colbuf, "true color", sizeof(colbuf));
		} else {
			snprintf(colbuf, sizeof(colbuf), "%u colors", dir->ncolors);
		}
		Debug(diag, "%s: #%u (%u, %u) %s, %u bits/pixel", __func__,
			i, dir->width, dir->height, colbuf, dir->colorbits);
	}

	uint page = hint->page;
	if (page >= nfiles) {
		warnx("%s: No page found: %u", __func__, page);
		goto abort;
	}

	img = ico_read_data(fp, &dirs[page], page, diag);
 abort:
	free(dirs);
	return img;
}

// fp からディレクトリブロックを読み込んで dir に返す。
static bool
ico_read_dir(FILE *fp, struct icodir *dir)
{
	uint8 buf[16];
	size_t n;

	n = fread(buf, sizeof(buf), 1, fp);
	if (n == 0) {
		return false;
	}

	dir->width  = buf[0] ?: 256;
	dir->height = buf[1] ?: 256;
	dir->ncolors = buf[2];
	dir->colorbits = le16toh(*(uint16 *)&buf[6]);
	dir->datalen = le32toh(*(uint32 *)&buf[8]);
	dir->dataoff = le32toh(*(uint32 *)&buf[12]);
	return true;
}

// dir で示される画像を読み込む。
static struct image *
ico_read_data(FILE *fp, const struct icodir *dir, uint page,
	const struct diag *diag)
{
	// データブロックは BMP 風か PNG。
	// PNG なら PNG ヘッダで始まるので $89 'P' 'N' 'G' ...、
	// BMP 風なら BITMAPINFOHEADER から始まり、これの先頭4バイトは
	// $28 $00 $00 $00 なので、最初の1バイトで判断が付く。

	fseek(fp, dir->dataoff, SEEK_SET);

	int firstbyte = fgetc(fp);
	if (firstbyte == EOF) {
		warnx("%s: EOF", __func__);
		return NULL;
	}
	// 次の読み込みに備えて、戻しておく。
	ungetc(firstbyte, fp);

	if (firstbyte == 0x28) {
		Debug(diag, "%s: #%u BMP %u bytes", __func__, page, dir->datalen);
		return ico_read_bmp(fp, dir, diag);
	} else {
		Debug(diag, "%s: #%u PNG %u bytes", __func__, page, dir->datalen);
		return ico_read_png(fp, dir, diag);
	}
}

// アイコンデータ (BMP 風形式) を読み込む。
static struct image *
ico_read_bmp(FILE *fp, const struct icodir *dir, const struct diag *diag)
{
	struct bmpctx bmp0;
	struct bmpctx *bmp;
	BITMAPINFOHEADER info;
	size_t n;
	bool ok = false;

	bmp = &bmp0;
	memset(bmp, 0, sizeof(*bmp));
	bmp->fp = fp;

	n = fread(&info, sizeof(info), 1, fp);
	if (n == 0) {
		warn("%s: fread(INFO) failed", __func__);
		return NULL;
	}

	// ヘッダの内容を読み込む。
	bmp_read_info_header(bmp, &info);

	// ここのヘッダの高さは2倍で記録されている。
	// 16x16 アイコンなら、ディレクトリブロックのほうは 16x16、
	// BITMAPINFOHEADER のほうは 16x32 になっている。
	// ところが縦 32 ラインといっても、
	// 前半 32 ラインのピクセルデータ部は biBitCount [bit/pixel] だが、
	// 後半 32 ラインのマスクデータ部は 1 [bit/pixel] であり、全然別物。
	// どうしてこうなった…。
	bmp->height /= 2;

	// デバッグ表示。
	if (diag_get_level(diag) >= 1) {
		bmp_print_debuginfo(bmp, diag, __func__, 0);
	}

	// 圧縮方式は事実上 BI_RGB のみらしい。
	if (bmp->compression != BI_RGB) {
		warnx("%s: Unsupported compression mode %d", __func__,
			bmp->compression);
		return NULL;
	}
	if (bmp_select_raster_rgb(bmp) == false) {
		warnx("%s: BI_RGB but BitCount=%u not supported", __func__,
			bmp->bitcount);
		return NULL;
	}

	// パレットは BiBitCount が 8 以下の時にある。
	if (bmp->bitcount <= 8) {
		if (bmp_read_palette4(bmp) == false) {
			warnx("%s: fread(palette) failed", __func__);
			return NULL;
		}
	}

	bmp->img = image_create(bmp->width, bmp->height, IMAGE_FMT_ARGB16);
	if (bmp->img == NULL) {
		return NULL;
	}

	// 全ラスターを展開。
	if (bmp_extract(bmp) == false) {
		goto abort;
	}

	// 続いてマスクデータ(アルファチャンネル)を展開。
	// ラスター処理関数を差し替えてもう一度走るだけでいい。
	bmp->rasterop = raster_icomask1;
	if (bmp_extract(bmp) == false) {
		goto abort;
	}

	ok = true;
 abort:
	if (ok == false) {
		image_free(bmp->img);
		bmp->img = NULL;
	}
	return bmp->img;
}

// アイコンのマスクデータ (AND ビットマップ) を展開する。
// ピクセルデータ(XOR ビットマップ) のビット深度とは関係なく、
// こちらは常に 1bit/pixel。
static int
raster_icomask1(struct bmpctx *bmp, int y)
{
	struct image *img = bmp->img;
	// 1バイトに8ピクセル収まっているのを4バイト単位に切り上げる。
	uint bytewidth = howmany(img->width, 8);
	uint bmpstride = roundup(bytewidth, 4);
	uint8 srcbuf[bmpstride];

	size_t n = fread(srcbuf, 1, bmpstride, bmp->fp);
	uint width = n * 8;
	if (width > img->width) {
		width = img->width;
	}

	const uint8 *s = srcbuf;
	uint16 *d = (uint16 *)img->buf + img->width * y;
	uint16 bits = 0;
	for (uint x = 0; x < width; x++) {
		// 左のピクセルが MSB 側。
		if (__predict_false((x % 8) == 0)) {
			bits = (*s++) << 8;
		}
		// %1 なら透明。
		uint alpha = (bits & 0x8000);
		if (alpha) {
			*d |= alpha;
		}
		d++;
		bits <<= 1;
	}

	return RASTER_OK;
}

// アイコンデータ (PNG 形式) を読み込む。
static struct image *
ico_read_png(FILE *fp, const struct icodir *dir, const struct diag *diag)
{
	const image_read_hint *hint = NULL;

#if defined(USE_LIBPNG)
	return image_png_read(fp, hint, diag);
#elif defined(USE_STB_IMAGE)
	return image_stb_read(fp, hint, diag);
#else
	return NULL;
#endif
}
