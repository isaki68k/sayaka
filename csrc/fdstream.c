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
// ファイルディスクリプタをシーク可能な FILE * にみせる
//

#include "common.h"
#include <errno.h>
#include <string.h>
#include <unistd.h>
#if defined(HAVE_BSD_BSD_H)
#include <bsd/stdio.h>
#endif

struct fdstream_cookie {
	int fd;			// 入力ディスクリプタ
	char *buf;		// バッファ (realloc する)
	size_t bufsize;	// 確保してあるバッファ長
	size_t len;		// 現在の有効長
	off_t pos;		// 読み出し位置
};

static int fdstream_read(void *, char *, int);
static int fdstream_write(void *, const char *, int);
static off_t fdstream_seek(void *, off_t, int);
static int fdstream_close(void *);

// ファイルディスクリプタをシーク可能な FILE * としてオープンする。
FILE *
fdstream_open(int fd)
{
	struct fdstream_cookie *cookie;
	FILE *fp;

	cookie = calloc(1, sizeof(struct fdstream_cookie));
	if (cookie == NULL) {
		return NULL;
	}

	cookie->fd = fd;

	fp = funopen(cookie,
		fdstream_read,
		fdstream_write,
		fdstream_seek,
		fdstream_close);
	if (fp == NULL) {
		free(cookie);
		return NULL;
	}

	return fp;
}

static int
fdstream_read(void *arg, char *dst, int dstsize)
{
	struct fdstream_cookie *cookie = (struct fdstream_cookie *)arg;

	if (cookie->pos >= cookie->len) {
		// バッファの末尾にいるなら追加読み込み。
		int avail = cookie->bufsize - cookie->len;
		if (avail == 0) {
			// 空き容量がなければ再確保して..
			int newsize = cookie->bufsize + 65536;
			char *newbuf = realloc(cookie->buf, newsize);
			if (newbuf == NULL) {
				return -1;
			}
			cookie->buf = newbuf;
			cookie->bufsize = newsize;

			// 空き容量を更新
			avail = cookie->bufsize - cookie->len;
		}

		size_t n = read(cookie->fd, cookie->buf + cookie->len, avail);
		if (n < 0) {
			return -1;
		}
		cookie->len += n;
	}

	int len = MIN(cookie->len - cookie->pos, dstsize);
	memcpy(dst, cookie->buf + cookie->pos, len);
	cookie->pos += len;
	return len;
}

static int
fdstream_write(void *arg, const char *src, int srclen)
{
	errno = ENOTSUP;
	return -1;
}

static off_t
fdstream_seek(void *arg, off_t offset, int whence)
{
	struct fdstream_cookie *cookie = (struct fdstream_cookie *)arg;
	off_t newpos;

	switch (whence) {
	 case SEEK_SET:
		newpos = offset;
		break;
	 case SEEK_CUR:
		newpos = cookie->pos + offset;
		break;
	 case SEEK_END:
		newpos = cookie->len + offset;
		break;
	 default:
		errno = EINVAL;
		return -1;
	}

	// 現在読み込んでいるファイル末尾を超えたらどうなる?
	if (newpos > cookie->len) {
		errno = EINVAL;
		return -1;
	}

	cookie->pos = newpos;
	return newpos;
}

static int
fdstream_close(void *arg)
{
	struct fdstream_cookie *cookie = (struct fdstream_cookie *)arg;

	if (cookie->fd >= 3) {
		close(cookie->fd);
	}

	free(cookie->buf);
	free(cookie);
	return 0;
}
