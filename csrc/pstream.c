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

// ここは diag 遠いので…
#if 0
#define DEBUG(fmt...)	do {	\
	printf("%s: ", __func__);	\
	printf(fmt);	\
	printf("\n");	\
} while (0)
#else
#define DEBUG(fmt...)	/**/
#endif

struct pstream {
	// 入力となるストリームかディスクリプタ。
	// ifp != NULL なら ifp が有効。ifp == NULL なら ifd が有効。
	FILE *ifp;
	int ifd;

	uint pos;			// 上位レイヤから見た現在位置

	char *peekbuf;		// ピーク用バッファ
	uint bufsize;		// 確保してあるバッファサイズ
	uint peeklen;		// ピークバッファに読み込んである長さ
	bool done;			// EOF に到達した
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
// 失敗すれば errno をセットして NULL を返す。
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
// 失敗すれば errno をセットして NULL を返す。
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
	if (ps->pos != 0) {
		fseek(fp, ps->pos, SEEK_SET);
	}

	return fp;
}

// 現在位置から最大 dstsize バイトを読み込んでバッファする。
static int
pstream_peek_cb(void *cookie, char *dst, int dstsize)
{
	struct pstream *ps = (struct pstream *)cookie;

	DEBUG("called(dstsize=%u)", dstsize);
	while (ps->pos == ps->peeklen) {
		// 内部バッファを末尾まで読んでいたら、次の読み込みを試行。

		// 終了フラグが立っていれば EOF。
		if (ps->done) {
			DEBUG("return EOF");
			return 0;
		}

		// バッファに空きがなければ拡大。
		if (ps->peeklen == ps->bufsize) {
			uint newsize = ps->bufsize + 1024;
			char *newbuf = realloc(ps->peekbuf, newsize);
			if (newbuf == NULL) {
				return -1;
			}
			ps->peekbuf = newbuf;
			ps->bufsize = newsize;
			DEBUG("realloc %u", newsize);
		}

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
		DEBUG("n = %d", (int)n);
		ps->peeklen += n;

		// この読み込みで EOF に到達した。
		if (n == 0) {
			ps->done = true;
		}
	}

	// 内部バッファにある限りは使う。
	size_t len = MIN(ps->peeklen - ps->pos, dstsize);
	DEBUG("len = %u from pos=%u", (uint)len, ps->pos);
	memcpy(dst, ps->peekbuf + ps->pos, len);
	ps->pos += len;
	return len;
}

// 現在位置から dstsize バイトを読み込む。
static int
pstream_read_cb(void *cookie, char *dst, int dstsize)
{
	struct pstream *ps = (struct pstream *)cookie;

	DEBUG("called(dstsize=%d)", dstsize);

	ssize_t len;
	if (ps->pos < ps->peeklen) {
		// ピークバッファ内ならピークバッファから読み出す。
		len = MIN(ps->peeklen - ps->pos, dstsize);
		DEBUG("from buf: pos=%u len=%d", ps->pos, (int)len);
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
		DEBUG("out buf : pos=%u len=%d", ps->pos, (int)len);
	}
	ps->pos += len;
	return len;
}

// 現在位置を設定する。
static off_t
pstream_seek_cb(void *cookie, off_t offset, int whence)
{
	struct pstream *ps = (struct pstream *)cookie;
	uint newpos;

	DEBUG("called(offset=%jd, %s)", (intmax_t)offset,
		(whence == SEEK_SET ? "SEEK_SET" :
		(whence == SEEK_CUR ? "SEEK_CUR" :
		(whence == SEEK_END ? "SEEK_END" : "?"))));

	switch (whence) {
	 case SEEK_SET:
		newpos = offset;
		break;
	 case SEEK_CUR:
		newpos = ps->pos + offset;
		break;
	 case SEEK_END:
	 default:
		DEBUG("whence=%d", whence);
		errno = EINVAL;
		return (off_t)-1;
	}

	if (newpos == ps->pos) {
		DEBUG("newpos=%u (unchanged)", newpos);
		return newpos;
	}

	// バッファ外への移動は、下位ストリームがサポートしていれば可能?
	// 実際には、バッファを超えて読み進めた後でピークバッファ内に seek で
	// 戻ってそこから再び読み進めてバッファを超えると話が合わなくなる。
	if (newpos > ps->peeklen) {
		off_t r;
		if (ps->ifp) {
			if (fseek(ps->ifp, (long)newpos, SEEK_SET) < 0) {
				DEBUG("fseek(%u): %s", (uint)newpos, strerrno());
				return -1;
			}
		} else {
			r = lseek(ps->ifd, (off_t)newpos, SEEK_SET);
			if (r < 0) {
				DEBUG("lseek(%u): %s", (uint)newpos, strerrno());
				return -1;
			}
			// 同じはずだが一応。
			newpos = r;
		}
	}

	ps->pos = newpos;
	DEBUG("pos=%u", ps->pos);
	return newpos;
}

// ストリームを閉じる。
static int
pstream_close_cb(void *cookie)
{
	struct pstream *ps = (struct pstream *)cookie;

	DEBUG("called");
	pstream_close(ps);
	return 0;
}
