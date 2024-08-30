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

	// HTTP 応答行
	string *resline;
	uint rescode;
	const char *resmsg;

	// HTTP 受信ヘッダ (上限は適当)
	string *recvhdr[64];
	uint recvhdr_num;

	const diag *diag;
} httpclient;

httpclient *
httpclient_create(const diag *diag)
{
	httpclient *http;

	http = calloc(1, sizeof(*http));
	if (http == NULL) {
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
		for (uint i = 0; i < http->recvhdr_num; i++) {
			string_free(http->recvhdr[i]);
		}

		// http->fp は fclose 時に NULL 代入を保証しないと
		// ここでクローズできない。

		net_destroy(http->net);
		free(http);
	}
}

// url に接続する。
// 成功すれば 0、失敗すれば -1 を返す。
int
httpclient_connect(httpclient *http, const char *url)
{
	const diag *diag = http->diag;
	int rv = -1;

	struct urlinfo *info = urlinfo_parse(url);
	if (info == NULL) {
		Debug(diag, "%s: urlinfo_parse: %s", __func__, strerrno());
		goto done;
	}
	const char *scheme = string_get(info->scheme);
	const char *host = string_get(info->host);
	const char *serv = string_get(info->port);
	const char *pqf  = string_get(info->pqf);

	if (strcmp(scheme, "http") != 0 && strcmp(scheme, "https") != 0) {
		Debug(diag, "%s: Unsupported protocol: %s", __func__, scheme);
		goto done;
	}

	if (serv[0] == '\0') {
		serv = scheme;
	}

	http->net = net_create(diag);
	if (http->net == NULL) {
		Debug(diag, "%s: net_create failed: %s", __func__, strerrno());
		goto done;
	}

	if (net_connect(http->net, scheme, host, serv) == false) {
		Debug(diag, "%s: %s://%s:%s failed %s", __func__,
			scheme, host, serv, strerrno());
		goto done;
	}

	http->fp = net_fopen(http->net);
	if (http->fp == NULL) {
		Debug(diag, "%s: net_fopen failed: %s", __func__, strerrno());
		goto done;
	}

	// HTTP ヘッダを送信。
	string *hdr = string_init();
	string_append_printf(hdr, "GET %s HTTP/1.1\r\n", pqf);
	string_append_printf(hdr, "Host: %s\r\n", host);
	string_append_printf(hdr, "User-Agent: sayaka/c\r\n");
	string_append_cstr(hdr,   "\r\n");
	Trace(diag, "<<< %s", string_get(hdr));
	fputs(string_get(hdr), http->fp);
	fflush(http->fp);
	string_free(hdr);

	// 応答の1行目を受信。
	http->resline = string_fgets(http->fp);
	if (http->resline == NULL) {
		Debug(diag, "%s: No HTTP response?", __func__);
		goto done;
	}
	string_rtrim_inplace(http->resline);

	// 残りのヘッダを受信。
	string *recv;
	while ((recv = string_fgets(http->fp)) != NULL) {
		string_rtrim_inplace(recv);
		Trace(diag, ">>> |%s|", string_get(recv));
		if (string_len(recv) != 0) {
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
	if (strncmp(p, "HTTP/", 5) != 0) {
		Debug(diag, "%s: Invalid HTTP response?", __func__);
		goto done;
	}
	p += 5;
	while (*p != '\0' && *p != ' ')
		p++;
	while (*p != '\0' && *p == ' ')
		p++;

	// 応答コードをチェック。
	http->rescode = stou32def(p, 0, UNCONST(&p));
	if (http->rescode >= 400) {
		rv = http->rescode;
	} else {
		rv = 0;
	}

	// メッセージを取得。
	while (*p != '\0' && *p == ' ')
		p++;
	http->resmsg = p;

	Debug(diag, "rescode = %3u |%s|", http->rescode, http->resmsg);

	net_shutdown(http->net);

 done:
	urlinfo_free(info);
	return rv;
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
