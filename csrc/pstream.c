/* vi:set ts=4: */
/*
 * Copyright (C) 2024 Tetsuya Isaki
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
// ピーク可能なストリーム (ほぼ画像ローダ専用)
//

// o 画像の種類判定をするのに先頭のいくらかを読んでは巻き戻すという動作が必要。
// o fd がファイルでもなければ基本的に seek は出来ない。
// o フルスペックで seek 可能にしてしまうと、入力ストリームをすべてバッファ
//   する必要があり、さすがに無駄。
// o さらに、おそらく画像ローダ側でも多かれ少なかれバッファはしてあり
//   (極端な話 WebP は結局ファイル全体を一度に要求する)、
//   なおさらこちらで自前バッファリングするのは無駄。
// o 画像のロードがターゲットなので、フルスペックの seek はこれまでのところ
//   求められておらずせいぜい rewind で十分。
// o 画像ローダには FILE* 形式で渡せると嬉しい。
// といった事情から、
// 前半の判定フェーズで使う、seek 可能な内部バッファを持つ FILE* と、
// 後半の読み込みフェーズで使う、内部バッファに置かず seek 不可能な FILE*
// という 2段階という変態ストリームを用意する。

#include "common.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

struct pstream {
	// 入力となるストリームかディスクリプタ。
	// ifp != NULL なら ifp が有効。ifp == NULL なら ifd が有効。
	FILE *ifp;
	int ifd;

	uint pos;			// 上位レイヤから見た現在位置
	bool seekable;		// シーク可能な期間は true

	char *peekbuf;		// ピーク用バッファ
	uint bufsize;		// 確保してあるバッファサイズ
	uint peeklen;		// ピークバッファに読み込んである長さ
};

static struct pstream *pstream_init_common(void);
static void pstream_close(struct pstream *);
static int pstream_peek_cb(void *, char *, int);
static int pstream_read_cb(void *, char *, int);
static off_t pstream_seek_cb(void *, off_t, int);
static int pstream_close_cb(void *);

static struct pstream *
pstream_init_common()
{
	struct pstream *ps = calloc(1, sizeof(struct pstream));
	if (ps == NULL) {
		return NULL;
	}

	ps->seekable = true;
	return ps;
}

// fd からストリームコンテキストを作成する。
struct pstream *
pstream_init_fd(int fd)
{
	struct pstream *ps = pstream_init_common();
	if (ps == NULL) {
		return NULL;
	}

	ps->ifp = NULL;
	ps->ifd = fd;
	return ps;
}

// fp からストリームコンテキストを作成する。
struct pstream *
pstream_init_fp(FILE *ifp)
{
	struct pstream *ps = pstream_init_common();
	if (ps == NULL) {
		return NULL;
	}

	ps->ifp = ifp;
	ps->ifd = -1;
	return ps;
}

// ストリームの入力を閉じる。
static void
pstream_close(struct pstream *ps)
{
	assert(ps);

	if (ps->ifp) {
		fclose(ps->ifp);
	} else {
		if (ps->ifd >= 3) {
			close(ps->ifd);
		}
	}
	ps->ifp = NULL;
	ps->ifd = -1;
}

// ストリームコンテキストを解放する。
// ps が NULL なら何もしない。
void
pstream_cleanup(struct pstream *ps)
{
	if (ps) {
		pstream_close(ps);
		free(ps->peekbuf);
		ps->peekbuf = NULL;

		free(ps);
	}
}

// ストリームコンテキストから、判定フェーズで使うシーク可能な FILE* を返す。
// read、seek、close が可能。close は何も閉じない。
FILE *
pstream_open_for_peek(struct pstream *ps)
{
	FILE *fp = funopen(ps,
		pstream_peek_cb,
		NULL,	// write
		pstream_seek_cb,
		NULL);	// close
	return fp;
}

// ストリームコンテキストから、直接読み込む FILE* を返す。
// read、close のみ可能。
// クローズでディスクリプタをクローズする。(ps は解放しない)
FILE *
pstream_open_for_read(struct pstream *ps)
{
	FILE *fp = funopen(ps,
		pstream_read_cb,
		NULL,	// write
		pstream_seek_cb,
		pstream_close_cb);
	if (fp == NULL) {
		return NULL;
	}

	// 巻き戻さずに呼び出されたら辻褄を合わせるために seek する。
	// (このためにこの時点までは seekable にしてある)
	if (ps->pos != 0) {
		fseek(fp, ps->pos, SEEK_SET);
	}

	ps->seekable = false;

	return fp;
}

// 現在位置から dstsize バイトを読み込んでバッファする。
static int
pstream_peek_cb(void *cookie, char *dst, int dstsize)
{
	struct pstream *ps = (struct pstream *)cookie;

	// 必要になる総バイト数。
	uint newsize = ps->pos + dstsize;

	if (newsize > ps->peeklen) {
		// 現在のピークバッファでは足りない場合。

		char *newbuf = realloc(ps->peekbuf, newsize);
		if (newbuf == NULL) {
			return -1;
		}
		ps->peekbuf = newbuf;
		ps->bufsize = newsize;

		// 前回は peeklen まで読み込んでいるので、続きを読み込む。
		char *buf = ps->peekbuf + ps->peeklen;
		size_t len = ps->bufsize - ps->peeklen;
		ssize_t n;
		if (ps->ifp) {
			n = fread(buf, 1, len, ps->ifp);
		} else {
			n = read(ps->ifd, buf, len);
			if (n < 0) {
				return -1;
			}
		}
		ps->peeklen += n;
	}

	size_t len = MIN(ps->peeklen - ps->pos, dstsize);
	memcpy(dst, ps->peekbuf + ps->pos, len);
	ps->pos += len;
	return len;
}

// 現在位置から dstsize バイトを読み込む。
static int
pstream_read_cb(void *cookie, char *dst, int dstsize)
{
	struct pstream *ps = (struct pstream *)cookie;

	ssize_t len;
	if (ps->pos < ps->peeklen) {
		// ピークバッファ内ならピークバッファから読み出す。
		len = MIN(ps->peeklen - ps->pos, dstsize);
		memcpy(dst, ps->peekbuf + ps->pos, len);
	} else {
		// ピークバッファ外なら直接リード。
		if (ps->ifp) {
			len = fread(dst, 1, dstsize, ps->ifp);
		} else {
			len = read(ps->ifd, dst, dstsize);
			if (len < 0) {
				return -1;
			}
		}
	}
	ps->pos += len;
	return len;
}

// 現在位置を設定する。
static off_t
pstream_seek_cb(void *cookie, off_t offset, int whence)
{
	struct pstream *ps = (struct pstream *)cookie;
	off_t newpos;

	if (ps->seekable == false) {
		errno = EPIPE;
		return (off_t)-1;
	}

	switch (whence) {
	 case SEEK_SET:
		newpos = offset;
		break;
	 case SEEK_CUR:
		newpos = ps->pos + offset;
		break;
	 case SEEK_END:
	 default:
		errno = EINVAL;
		return (off_t)-1;
	}

	// 読み込んであるバッファを通り過ぎることは面倒なので認めない。
	if (newpos > ps->peeklen) {
		errno = EINVAL;
		return (off_t)-1;
	}

	ps->pos = (uint)newpos;
	return newpos;
}

// ストリームを閉じる。
static int
pstream_close_cb(void *cookie)
{
	struct pstream *ps = (struct pstream *)cookie;

	pstream_close(ps);
	return 0;
}
