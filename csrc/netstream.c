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
// URL のファイルをシーク可能な FILE * にみせる
//

#include "common.h"

#if defined(HAVE_LIBCURL)

#include <errno.h>
#include <string.h>
#include <curl/curl.h>

// メモリセグメント
struct segment {
	char *ptr;
	uint pos;	// このセグメントの位置
	uint len;	// このセグメントの長さ
};

struct memstream_cookie {
	struct segment *segs;
	uint segcap;	// 確保してあるセグメント数
	uint seglen;	// 使用しているセグメント数
	uint curseg;	// 現在ポインタがいるセグメント番号
	uint pos;		// 先頭から数えた現在位置
};

static void netstream_global_init(void);
static size_t curl_write_cb(void *, size_t, size_t, void *);
static int memstream_read(void *, char *, int);
static int memstream_write(void *, const char *, int);
static off_t memstream_seek(void *, off_t, int);
static int memstream_close(void *);

static bool curl_initialized;

// こっちは初回の netstream_open() 内から呼ばれる。
static void
netstream_global_init(void)
{
	if (curl_initialized == false) {
		curl_global_init(CURL_GLOBAL_ALL);
		curl_initialized = true;
	}
}

// こっちはアプリケーション終了時に呼ぶこと。
void
netstream_global_cleanup(void)
{
	if (curl_initialized) {
		curl_global_cleanup();
	}
}

// url をダウンロードしてファイルストリームにして返す。
FILE *
netstream_open(const char *url, const struct diag *diag)
{
	struct memstream_cookie *cookie = NULL;
	FILE *fp = NULL;
	CURL *curl = NULL;

	netstream_global_init();

	cookie = calloc(1, sizeof(*cookie));
	if (cookie == NULL) {
		goto abort;
	}
	// segs が NULL かも知れないと気をつけるのは嫌なので先に確保。
	cookie->segs = malloc(sizeof(struct segment) * 16);
	if (cookie->segs == NULL) {
		goto abort;
	}

	fp = funopen(cookie,
		memstream_read,
		memstream_write,
		memstream_seek,
		memstream_close);
	if (fp == NULL) {
		goto abort;
	}

	curl = curl_easy_init();
	if (curl == NULL) {
		Debug(diag, "curl_easy_init() failed");
		goto abort;
	}

	// ダウンロード。
	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
	curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_cb);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
	CURLcode res = curl_easy_perform(curl);
	if (res != CURLE_OK) {
		Debug(diag, "%s: %s", __func__, curl_easy_strerror(res));
		goto abort;
	}

	// 読み出し用にポインタを先頭に戻す。
	fseek(fp, 0, SEEK_SET);

	return fp;

 abort:
	curl_easy_cleanup(curl);
	if (cookie) {
		free(cookie->segs);
		free(cookie);
	}
	return NULL;
}

// curl からのダウンロードコールバック関数
static size_t
curl_write_cb(void *buf, size_t size, size_t nmemb, void *user)
{
	FILE *fp = (FILE *)user;

	return fwrite(buf, size, nmemb, fp);
}

static int
memstream_read(void *arg, char *dst, int dstsize)
{
	struct memstream_cookie *cookie = (struct memstream_cookie *)arg;

	if (cookie->curseg >= cookie->seglen) {
		return 0;
	}
	struct segment *seg = &cookie->segs[cookie->curseg];
	uint segoff = cookie->pos - seg->pos;
	uint remain = seg->len - segoff;
	uint len = MIN(remain, dstsize);
	memcpy(dst, &seg->ptr[segoff], len);

	cookie->pos += len;
	if (segoff + len >= seg->len) {
		cookie->curseg++;
	}

	return len;
}

static int
memstream_write(void *arg, const char *src, int srclen)
{
	struct memstream_cookie *cookie = (struct memstream_cookie *)arg;

	// write はセグメントを末尾に追加。
	// 途中の write には対応していない。

	// 現在の最後のページから最終位置を取得。
	uint offset = 0;
	if (cookie->seglen > 0) {
		struct segment *last = &cookie->segs[cookie->seglen - 1];
		offset = last->pos + last->len;
	}

	// 目次が足りなければ伸ばす。
	if (cookie->seglen >= cookie->segcap) {
		uint newcap = cookie->segcap + 16;
		struct segment *newbuf;
		newbuf = realloc(cookie->segs, sizeof(struct segment) * newcap);
		if (newbuf == NULL) {
			errno = ENOMEM;
			return 0;
		}
		cookie->segs   = newbuf;
		cookie->segcap = newcap;
	}

	// 新しいセグメント。
	struct segment *seg = &cookie->segs[cookie->seglen];
	seg->pos = offset;
	seg->len = srclen;
	seg->ptr = malloc(srclen);
	if (seg->ptr == NULL) {
		return 0;
	}
	memcpy(seg->ptr, src, srclen);

	if (1) {
		printf("%s [%u/%u] offset=%u, len=%u\n", __func__,
			cookie->seglen, cookie->segcap, seg->pos, seg->len);
	}

	cookie->pos += srclen;
	cookie->seglen++;
	cookie->curseg = cookie->seglen;

	return srclen;
}

static off_t
memstream_seek(void *arg, off_t offset, int whence)
{
	struct memstream_cookie *cookie = (struct memstream_cookie *)arg;
	off_t newpos;

	switch (whence) {
	 case SEEK_SET:
		newpos = offset;
		break;
	 case SEEK_CUR:
		newpos = cookie->pos + offset;
		break;
	 case SEEK_END:
	 {
		off_t end;
		if (cookie->seglen == 0) {
			end = 0;
		} else {
			struct segment *last = &cookie->segs[cookie->seglen - 1];
			end = last->pos + last->len;
		}
		newpos = end + offset;
		break;
	 }
	 default:
		errno = EINVAL;
		return (off_t)-1;
	}

	// pos を含むセグメントに移動。
	uint curseg = 0;
	for (; curseg < cookie->seglen; curseg++) {
		struct segment *seg = &cookie->segs[curseg];
		if (newpos < seg->pos + seg->len) {
			break;
		}
	}
	cookie->curseg = curseg;
	cookie->pos = (int)newpos;

	return newpos;
}

static int
memstream_close(void *arg)
{
	struct memstream_cookie *cookie = (struct memstream_cookie *)arg;

	for (int i = 0, end = cookie->seglen; i < end; i++) {
		struct segment *seg = &cookie->segs[i];
		free(seg->ptr);
	}
	free(cookie->segs);
	free(cookie);

	return 0;
}

#else /* HAVE_LIBCURL */

// libcurl がない時でもクリーンアップだけは字面上必要。
void
netstream_global_cleanup(void)
{
}

#endif /* HAVE_LIBCURL */
