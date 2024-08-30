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
// HTTP クライアント
//

#include "common.h"
#include <stdio.h>
#include <string.h>

typedef struct httpclient_ {
	struct net *net;
	FILE *fp;

	// 接続中の URL
	struct urlinfo *url;

	// HTTP 応答行
	string *resline;
	uint rescode;
	const char *resmsg;

	// HTTP 受信ヘッダ (上限は適当)
	string *recvhdr[64];
	uint recvhdr_num;

	const diag *diag;
} httpclient;

static bool do_connect(httpclient *);
static void dump_sendhdr(httpclient *, const string *);
static int  recv_header(httpclient *);
static const char *find_recvhdr(const httpclient *, const char *);
static void clear_recvhdr(httpclient *);

httpclient *
httpclient_create(const diag *diag)
{
	httpclient *http;

	http = calloc(1, sizeof(*http));
	if (http == NULL) {
		return NULL;
	}

	http->net = net_create(diag);
	if (http->net == NULL) {
		free(http);
		return NULL;
	}

	http->diag = diag;

	return http;
}

void
httpclient_destroy(httpclient *http)
{
	if (http) {
		string_free(http->resline);
		clear_recvhdr(http);

		// http->fp は fclose 時に NULL 代入を保証しないと
		// ここでクローズできない。

		net_destroy(http->net);
		urlinfo_free(http->url);
		free(http);
	}
}

// url に接続する。
// 成功すれば 0 を返す。失敗すれば -1 を返す。
// 400 以上なら HTTP のエラーコード。
int
httpclient_connect(httpclient *http, const char *urlstr)
{
	const diag *diag = http->diag;
	int rv = -1;

	http->url = urlinfo_parse(urlstr);
	if (http->url == NULL) {
		Debug(diag, "%s: urlinfo_parse failed", __func__);
		return rv;
	}
	if (diag_get_level(diag) >= 2) {
		string *u = urlinfo_to_string(http->url);
		diag_print(diag, "%s: initial url |%s|", __func__, string_get(u));
		string_free(u);
	}

	for (;;) {
		// 接続。
		if (do_connect(http) == false) {
			Debug(diag, "%s: do_connect failed", __func__);
			break;
		}

		// ヘッダを送信。
		const char *host = string_get(http->url->host);
		const char *pqf  = string_get(http->url->pqf);
		string *hdr = string_init();
		string_append_printf(hdr, "GET %s HTTP/1.1\r\n", pqf);
		string_append_printf(hdr, "Host: %s\r\n", host);
		string_append_cstr(hdr,   "Connection: close\r\n");
		string_append_printf(hdr, "User-Agent: sayaka/c\r\n");
		string_append_cstr(hdr,   "\r\n");
		if (__predict_false(diag_get_level(diag) >= 2)) {
			dump_sendhdr(http, hdr);	// デバッグ表示
		}
		fputs(string_get(hdr), http->fp);
		fflush(http->fp);
		string_free(hdr);

		// 応答を受信。
		int code = recv_header(http);
		Debug(diag, "%s: rescode = %3u |%s|", __func__,
			http->rescode, http->resmsg);

		if (300 <= code && code < 400) {
			const char *location = find_recvhdr(http, "Location:");
			if (location) {
				struct urlinfo *newurl = urlinfo_parse(location);
				if (string_len(newurl->scheme) != 0) {
					// scheme があればフル URL とみなす。
					urlinfo_free(http->url);
					http->url = newurl;
				} else {
					// そうでなければ相対パスとみなす。
					urlinfo_update_path(http->url, newurl);
					urlinfo_free(newurl);
				}
				if (diag_get_level(diag) >= 1) {
					string *u = urlinfo_to_string(http->url);
					diag_print(diag, "new url |%s|", string_get(u));
					string_free(u);
				}
				// 内部状態をリセット。
				fclose(http->fp);
				net_close(http->net);
				clear_recvhdr(http);
				string_free(http->resline);
				http->resline = NULL;
				http->rescode = 0;
				http->resmsg = NULL;
				continue;
			}
		} else if (code >= 400) {
			return code;
		}

		rv = 0;
		Trace(diag, "%s: connected.", __func__);
		break;
	}

	net_shutdown(http->net);
	return rv;
}

// http->url に接続するところまで。
// 接続できれば true を返す。
static bool
do_connect(httpclient *http)
{
	const diag *diag = http->diag;

	const char *scheme = string_get(http->url->scheme);
	const char *host = string_get(http->url->host);
	const char *serv = string_get(http->url->port);

	if (strcmp(scheme, "http") != 0 && strcmp(scheme, "https") != 0) {
		Debug(diag, "%s: Unsupported protocol: %s", __func__, scheme);
		return false;
	}

	if (serv[0] == '\0') {
		serv = scheme;
	}

	Trace(diag, "%s: connecting %s://%s:%s", __func__, scheme, host, serv);
	if (net_connect(http->net, scheme, host, serv) == false) {
		Debug(diag, "%s: %s://%s:%s failed %s", __func__,
			scheme, host, serv, strerrno());
		return false;
	}

	http->fp = net_fopen(http->net);
	if (http->fp == NULL) {
		Debug(diag, "%s: net_fopen failed: %s", __func__, strerrno());
		return false;
	}

	return true;
}

// 送信ヘッダをデバッグ表示する。
static void
dump_sendhdr(httpclient *http, const string *hdr)
{
	char buf[1024];

	// 改行を忘れたりすると事故なので、改行をエスケープして表示する。
	const char *s = string_get(hdr);
	char *d = buf;
	for (; *s; s++) {
		if (*s == '\r') {
			*d++ = '\\';
			*d++ = 'r';
		} else if (*s == '\n') {
			*d++ = '\\';
			*d++ = 'n';
			*d = '\0';
			Trace(http->diag, "<-- |%s|", buf);
			d = buf;
		} else {
			*d++ = *s;
		}
	}
	if (d != buf) {
		*d = '\0';
		Trace(http->diag, "<-! |%s|", buf);
	}
}

// 応答を受信する。
// 戻り値は HTTP 応答コード。
// エラーなら -1 を返す。
static int
recv_header(httpclient *http)
{
	const diag *diag = http->diag;

	// 応答の1行目を受信。
	http->resline = string_fgets(http->fp);
	if (http->resline == NULL) {
		Debug(diag, "%s: No HTTP response?", __func__);
		return -1;
	}
	string_rtrim_inplace(http->resline);
	Trace(diag, "--> |%s|", string_get(http->resline));

	// 残りのヘッダを受信。
	string *recv;
	while ((recv = string_fgets(http->fp)) != NULL) {
		string_rtrim_inplace(recv);
		Trace(diag, "--> |%s|", string_get(recv));
		if (string_len(recv) != 0) {
			// XXX 足りなくなったら無視…
			if (http->recvhdr_num < countof(http->recvhdr)) {
				http->recvhdr[http->recvhdr_num++] = recv;
			}
		} else {
			string_free(recv);
			break;
		}
	}

	// 1行目を雑にチェックする。
	// "HTTP/1.1 200 OK\r\n"。
	const char *p = string_get(http->resline);
	const char *e = strchr(p, ' ');
	if (e == NULL) {
		Debug(diag, "%s: Invaild HTTP response: %s", __func__,
			string_get(http->resline));
		return -1;
	}
	if (strncmp(p, "HTTP/1.0", e - p) != 0 &&
		strncmp(p, "HTTP/1.1", e - p) != 0)
	{
		Debug(diag, "%s: Unsupported HTTP version?", __func__);
		return -1;
	}

	p = e;
	while (*p != '\0' && *p == ' ')
		p++;

	// 応答コードをチェック。
	http->rescode = stou32def(p, 0, UNCONST(&p));

	// メッセージを取得。
	while (*p != '\0' && *p == ' ')
		p++;
	http->resmsg = p;

	return http->rescode;
}

// 受信ヘッダからヘッダ名 key (":" を含むこと) に対応する値を返す。
// 戻り値は http->recvhdr 内を指しているので解放不要。
// 見付からなければ NULL を返す。
static const char *
find_recvhdr(const httpclient *http, const char *key)
{
	uint keylen = strlen(key);

	for (uint i = 0; i < http->recvhdr_num; i++) {
		const char *h = string_get(http->recvhdr[i]);
		if (strncasecmp(h, key, keylen) == 0) {
			const char *p = h + keylen;
			while (*p != '\0' && *p == ' ')
				p++;
			return p;
		}
	}
	return NULL;
}

// 受信ヘッダをクリアする。
static void
clear_recvhdr(httpclient *http)
{
	for (uint i = 0; i < http->recvhdr_num; i++) {
		string_free(http->recvhdr[i]);
	}
	memset(&http->recvhdr, 0, sizeof(http->recvhdr));
	http->recvhdr_num = 0;
}

// HTTP 応答のメッセージ部分を返す。
// 接続していないなどでメッセージがなければ NULL を返す。
const char *
httpclient_get_resmsg(const httpclient *http)
{
	return http->resmsg;
}

// ストリームを返す。
// httpclient_connect() が成功した場合のみ有効。
FILE *
httpclient_fopen(const httpclient *http)
{
	return http->fp;
}


#if defined(TEST)

#include <err.h>
#include <signal.h>

static int
testhttp(const diag *diag, int ac, char *av[])
{
	httpclient *http;
	const char *url;

	url = av[2];

	http = httpclient_create(diag);
	if (http == NULL) {
		err(1, "%s: http_create failed", __func__);
	}

	int code = httpclient_connect(http, url);
	if (code < 0) {
		err(1, "%s: http_connect failed", __func__);
	}
	if (code >= 400) {
		errx(1, "%s: http_connect failed: %u %s", __func__,
			http->rescode, http->resmsg);
	}

	net_close(http->net);
	httpclient_destroy(http);
	return 0;
}

int
main(int ac, char *av[])
{
	diag *diag = diag_alloc();
	diag_set_level(diag, 2);

	signal(SIGPIPE, SIG_IGN);

	if (ac == 3 && strcmp(av[1], "http") == 0) {
		return testhttp(diag, ac, av);
	}

	printf("usage: %s http <url> ... HTTP/HTTPS client\n",
		getprogname());
	return 0;
}
#endif // TEST
