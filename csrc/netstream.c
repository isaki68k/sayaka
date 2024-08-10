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
#if defined(LIBCURL_HAS_MBEDTLS_BACKEND)
#include <mbedtls/ssl.h>
#endif
#if defined(LIBCURL_HAS_OPENSSL_BACKEND)
#include <openssl/ssl.h>
#endif

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
	const struct diag *diag;
};

static void netstream_global_init(void);
static bool netstream_get_sessioninfo(CURL *, const struct diag *);
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
netstream_open(const char *url, const struct netstream_opt *opt,
	const struct diag *diag)
{
	struct memstream_cookie *cookie = NULL;
	FILE *fp = NULL;
	CURLM *mhandle = NULL;
	CURL *curl = NULL;
	CURLMcode mcode;
	CURLcode res;
	bool done;
	bool info_shown;

	netstream_global_init();

	cookie = calloc(1, sizeof(*cookie));
	if (cookie == NULL) {
		goto abort;
	}
	cookie->diag = diag;
	// segs が NULL かも知れないと気をつけるのは嫌なので先に確保。
	cookie->segcap = 16;
	cookie->segs = malloc(sizeof(struct segment) * cookie->segcap);
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

	mhandle = curl_multi_init();
	if (mhandle == NULL) {
		Debug(diag, "curl_multi_init() failed");
		goto abort;
	}

	curl = curl_easy_init();
	if (curl == NULL) {
		Debug(diag, "curl_easy_init() failed");
		goto abort;
	}

	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, (long)1);
	curl_easy_setopt(curl, CURLOPT_NOPROGRESS, (long)1);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_cb);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, (long)0);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, (long)0);
	if (opt->use_rsa_only) {
		// 通称 RSA が使えるのは TLSv1.2 以下のみ。
		curl_easy_setopt(curl, CURLOPT_SSLVERSION,
			(long)CURL_SSLVERSION_MAX_TLSv1_2);

		// cipher_list はバックエンドに垂れ流しているだけなので、
		// バックエンドごとに指定方法が違う。うーんこの…。
		// そして現在のバックエンドは文字列判定しか出来ないっぽい。
		curl_version_info_data *info = curl_version_info(CURLVERSION_NOW);
		const char *ciphers = NULL;
		if (strncmp(info->ssl_version, "OpenSSL", 7) == 0) {
			ciphers = "AES128-SHA";
		} else {
			Debug(diag, "Not supported backend ssl_version \"%s\"",
				info->ssl_version);
			goto abort;
		}
		curl_easy_setopt(curl, CURLOPT_SSL_CIPHER_LIST, ciphers);
	}

	curl_multi_add_handle(mhandle, curl);

	// curl_easy_perform() 相当。curl/lib/easy.c の easy_transfer() あたり。
	done = false;
	mcode = CURLM_OK;
	res = CURLE_OK;
	info_shown = false;
	while (done == false && mcode == 0) {
		int still_running = 0;
		mcode = curl_multi_poll(mhandle, NULL, 0, 1000, NULL);
		if (mcode == 0) {
			mcode = curl_multi_perform(mhandle, &still_running);
		}

		// セッション情報をデバッグ表示。
		if (diag_get_level(diag) >= 1 && info_shown == false) {
			info_shown = netstream_get_sessioninfo(curl, diag);
		}

		if (mcode == 0 && still_running == 0) {
			int rc;
			CURLMsg *msg = curl_multi_info_read(mhandle, &rc);
			if (msg) {
				res = msg->data.result;
				done = true;
			}
		}
	}

	// 所要時間を表示。
	if (diag_get_level(diag) >= 1) {
		int r;
		curl_off_t queue_time;
		curl_off_t name_time;
		curl_off_t connect_time;
		curl_off_t appconn_time;
		curl_off_t prexfer_time;
		curl_off_t start_time;
		curl_off_t total_time;

		r = curl_easy_getinfo(curl, CURLINFO_QUEUE_TIME_T, &queue_time);
		if (r != CURLE_OK) {
			printf("err\n");
		}
		r = curl_easy_getinfo(curl, CURLINFO_NAMELOOKUP_TIME_T, &name_time);
		if (r != CURLE_OK) {
			printf("err\n");
		}
		r = curl_easy_getinfo(curl, CURLINFO_CONNECT_TIME_T, &connect_time);
		if (r != CURLE_OK) {
			printf("err\n");
		}
		r = curl_easy_getinfo(curl, CURLINFO_APPCONNECT_TIME_T, &appconn_time);
		if (r != CURLE_OK) {
			printf("err\n");
		}
		r = curl_easy_getinfo(curl, CURLINFO_PRETRANSFER_TIME_T, &prexfer_time);
		if (r != CURLE_OK) {
			printf("err\n");
		}
		r = curl_easy_getinfo(curl, CURLINFO_STARTTRANSFER_TIME_T, &start_time);
		if (r != CURLE_OK) {
			printf("err\n");
		}
		r = curl_easy_getinfo(curl, CURLINFO_TOTAL_TIME_T, &total_time);
		if (r != CURLE_OK) {
			printf("err\n");
		}

		// いずれも開始時点(どこ?)からの積算[usec]なので、個別の時間に分ける。
		// queue_time はほぼ固定費なので無視。amd64 で 130usec 程度。
		// pretransfer も無視していい?。amd64 で 40usec 程度。
		// APPCONNECT は HTTP 時は 0 が返ってくる。

		uint32 name_ms = (uint32)(name_time - queue_time) / 1000;
		uint32 conn_ms = (uint32)(connect_time - name_time) / 1000;
		char appbuf[32];
		appbuf[0] = '\0';
		if (appconn_time != 0) {
			uint32 app_ms = (uint32)(appconn_time - connect_time) / 1000;
			snprintf(appbuf, sizeof(appbuf), " appconn=%u", app_ms);
		}
		uint32 start_ms = (uint32)(start_time - prexfer_time) / 1000;
		uint32 total_ms = (uint32)total_time / 1000;

		diag_print(diag,
			"Profile: namelookup=%u connect=%u%s start=%u: total %u msec",
			name_ms,
			conn_ms,
			appbuf,
			start_ms,
			total_ms);
	}

	if (res != CURLE_OK) {
		Debug(diag, "%s: %s", __func__, curl_easy_strerror(res));
		goto abort;
	}

	curl_multi_remove_handle(mhandle, curl);
	curl_easy_cleanup(curl);
	curl_multi_cleanup(mhandle);

	// 読み出し用にポインタを先頭に戻す。
	fseek(fp, 0, SEEK_SET);

	return fp;

 abort:
	curl_easy_cleanup(curl);
	curl_multi_cleanup(mhandle);
	if (cookie) {
		free(cookie->segs);
		free(cookie);
	}
	return NULL;
}

// 接続中の TLS バージョン等をデバッグ表示する。
// 処理できれば true を返す。
static bool
netstream_get_sessioninfo(CURL *curl, const struct diag *diag)
{
	struct curl_tlssessioninfo *csession;

	// internals がいつからいつまで有効かは分からないっぽいので、
	// 当たるまで調べる。うーん。
	curl_easy_getinfo(curl, CURLINFO_TLS_SSL_PTR, &csession);
	if (csession->internals == NULL) {
		return false;
	}

	// バックエンドごとの処理。
#if defined(LIBCURL_HAS_MBEDTLS_BACKEND)
	if (csession->backend == CURLSSLBACKEND_MBEDTLS) {
		mbedtls_ssl_context *ssl = (mbedtls_ssl_context *)csession->internals;
		diag_print(diag, "Connected %s %s",
			mbedtls_ssl_get_version(ssl),
			mbedtls_ssl_get_ciphersuite(ssl));
	} else
#endif
#if defined(LIBCURL_HAS_OPENSSL_BACKEND)
	if (csession->backend == CURLSSLBACKEND_OPENSSL) {
		SSL_SESSION *sess = SSL_get_session(csession->internals);
		int ssl_version = SSL_SESSION_get_protocol_version(sess);
		char verbuf[16];
		const char *ver;
		switch (ssl_version) {
		 case SSL3_VERSION:		ver = "SSLv3";		break;
		 case TLS1_VERSION:		ver = "TLSv1.0";	break;
		 case TLS1_1_VERSION:	ver = "TLSv1.1";	break;
		 case TLS1_2_VERSION:	ver = "TLSv1.2";	break;
		 case TLS1_3_VERSION:	ver = "TLSv1.3";	break;
		 default:
			snprintf(verbuf, sizeof(verbuf), "0x%04x", ssl_version);
			ver = verbuf;
			break;
		}

		const SSL_CIPHER *ssl_cipher = SSL_SESSION_get0_cipher(sess);
		const char *cipher_name = SSL_CIPHER_get_name(ssl_cipher);

		diag_print(diag, "Connected %s %s", ver, cipher_name);
	} else
#endif
	{
		// 仕方ないので何か表示しておく。
		diag_print(diag, "Connected");
	}

	return true;
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

	Trace(cookie->diag, "%s: [%u/%u] offset=%u, len=%u", __func__,
		cookie->seglen, cookie->segcap, seg->pos, seg->len);

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

// opt を初期化する。
void
netstream_opt_init(struct netstream_opt *opt)
{
	opt->use_rsa_only = false;
}