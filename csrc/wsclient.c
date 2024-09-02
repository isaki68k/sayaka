/* vi:set ts=4: */
/*
 * Copyright (C) 2023-2024 Tetsuya Isaki
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
// WebSocket クライアント
//

#include "sayaka.h"
#include <errno.h>
#include <string.h>

enum {
	// フレームの +0バイト目 (の下位4ビット)
	WS_OPCODE_CONT		= 0x0,	// 継続
	WS_OPCODE_TEXT		= 0x1,	// テキスト
	WS_OPCODE_BINARY	= 0x2,	// バイナリ
	WS_OPCODE_CLOSE		= 0x8,	// 終了
	WS_OPCODE_PING		= 0x9,	// ping
	WS_OPCODE_PONG		= 0xa,	// pong

	// フレームの +0バイト目の最上位ビットは最終フレームビット。
	WS_OPFLAG_FIN		= 0x80,

	// フレームの +1バイト目の最上位ビットはマスクビット。
	// クライアントからサーバへのフレームには立てる。
	WS_MASK_BIT			= 0x80,	// Frame[1]
};

#define BUFSIZE	(1024)

typedef struct wsclient_ {
	struct net *net;

	uint8 *buf;			// 受信バッファ
	uint bufsize;		// 確保してある buf のバイト数
	uint buflen;		// buf の有効バイト数
	uint bufpos;		// 現在の処理開始位置

	uint8 opcode;		// opcode
	string *text;		// テキストメッセージ

	// テキスト受信コールバック。
	// テキストが 1フレーム受信できた時に呼ばれる。
	void (*callback)(const string *);

	const diag *diag;
} wsclient;

static void wsclient_send_pong(wsclient *);
static int  wsclient_send(wsclient *, uint8, const void *, uint);
static uint ws_encode_len(uint8 *, uint);
static uint ws_decode_len(const uint8 *, uint *);

// wsclient コンテキストを生成する。
// 失敗すれば errno をセットし NULL を返す。
wsclient *
wsclient_create(const diag *diag)
{
	wsclient *ws;

	ws = calloc(1, sizeof(*ws));
	if (ws == NULL) {
		return NULL;
	}

	ws->buf = malloc(BUFSIZE);
	if (ws->buf == NULL) {
		goto abort;
	}

	ws->text = string_alloc(BUFSIZE);
	if (ws->text == NULL) {
		goto abort;
	}

	ws->diag = diag;

	return ws;

 abort:
	wsclient_destroy(ws);
	return NULL;
}

// ws を解放する。
void
wsclient_destroy(wsclient *ws)
{
	if (ws) {
		net_destroy(ws->net);
		free(ws->buf);
		string_free(ws->text);
		free(ws);
	}
}

// ws を初期化する。
void
wsclient_init(wsclient *ws, void (*callback)(const string *))
{
	assert(ws);

	ws->callback = callback;
}

// url に接続する。
// 失敗すれば errno をセットして -1 を返す。
// 0 なら EOF?。
// 接続して HTTP(WebSocket) 応答まで受け取れれば応答コードを返す
// (101 なら成功)。
int
wsclient_connect(wsclient *ws, const char *url, const struct net_opt *opt)
{
	const diag *diag = ws->diag;
	string *key = NULL;
	string *hdr = NULL;
	string *response = NULL;
	int rv = -1;

	struct urlinfo *info = urlinfo_parse(url);
	if (info == NULL) {
		Debug(diag, "%s: urlinfo_parse: %s", __func__, strerrno());
		goto abort;
	}
	const char *scheme = string_get(info->scheme);
	const char *host = string_get(info->host);
	const char *serv = string_get(info->port);
	const char *pqf  = string_get(info->pqf);

	if (serv[0] == '\0') {
		if (strcmp(scheme, "ws") == 0) {
			serv = "http";
		} else if (strcmp(scheme, "wss") == 0) {
			serv = "https";
		} else {
			errno = EPROTONOSUPPORT;
			Debug(diag, "%s: %s: %s", __func__, url, strerrno());
			goto abort;
		}
	}

	ws->net = net_create(diag);
	if (ws->net == NULL) {
		Debug(diag, "%s: net_create failed: %s", __func__, strerrno());
		goto abort;
	}

	if (net_connect(ws->net, scheme, host, serv, opt) == false) {
		Debug(diag, "%s: %s://%s:%s failed %s", __func__,
			scheme, host, serv, strerrno());
		goto abort;
	}

	// キー(乱数)を作成。
	char nonce[16];
	rnd_fill(nonce, sizeof(nonce));
	key = base64_encode(nonce, sizeof(nonce));

	// WebSocket ヘッダを送信。
	hdr = string_init();
	string_append_printf(hdr, "GET %s HTTP/1.1\r\n", pqf);
	string_append_printf(hdr, "Host: %s\r\n", host);
	string_append_printf(hdr, "User-Agent: sayaka/c\r\n");
	string_append_cstr(hdr,
		"Upgrade: websocket\r\n"
		"Connection: Upgrade\r\n"
		"Sec-WebSocket-Version: 13\r\n");
	string_append_printf(hdr, "Sec-WebSocket-Key: %s\r\n", string_get(key));
	string_append_cstr(hdr,   "\r\n");
	if (__predict_false(diag_get_level(diag) >= 2)) {
		diag_http_header(diag, hdr);
	}
	int sent = net_write(ws->net, string_get(hdr), string_len(hdr));
	if (sent < 0) {
		Debug(diag, "%s: net_write: %s", __func__, strerrno());
		goto abort;
	}

	// 応答の1行目を受信。
	response = net_gets(ws->net);
	if (response == NULL) {
		Debug(diag, "%s: Unexpected EOF while reading response header",
			__func__);
		rv = 0;
		goto abort;
	}

	// 残りの行は今のところ使ってないので読み捨てる。
	string *recvhdr;
	while ((recvhdr = net_gets(ws->net)) != NULL) {
		string_rtrim_inplace(recvhdr);
		bool newline = (string_len(recvhdr) == 0);
		Trace(diag, "--> |%s|", string_get(recvhdr));
		string_free(recvhdr);
		if (newline) {
			break;
		}
	}

	// 1行目を雑にチェックする。
	// "HTTP/1.1 101 Switching Protocols\r\n" みたいなのが来るはず。

	// 先頭が "HTTP/1.1"。
	const char *recvbuf = string_get(response);
	if (strncmp(recvbuf, "HTTP/1.1", 8) != 0) {
		Debug(diag, "%s: No HTTP/1.1 response?", __func__);
		errno = EPROTO;
		goto abort;
	}

	// 空白をスキップ。
	const char *p = &recvbuf[8];
	while (*p == ' ')
		p++;

	// 応答コードをチェック。
	int rescode = atoi(p);
	if (rescode != 101) {
		Debug(diag, "%s: Upgrading failed by %u", __func__, rescode);
		rv = rescode;
		goto abort;
	}

	// XXX Sec-WebSocket-Accept のチェックとか。

	rv = rescode;
 abort:
	string_free(response);
	string_free(hdr);
	string_free(key);
	urlinfo_free(info);
	return rv;
}

// net に着信したフレームの処理をする (受信までブロックする)。
// 戻り値は -1 ならエラー。0 なら EOF。
// 1 なら何かしら処理をしたが、上位には関係がない。
// 2 なら ws->text に上位に通知するデータが用意できた。
int
wsclient_process(wsclient *ws)
{
	const diag *diag = ws->diag;
	int rv = 1;
	int r;

	// 受信バッファに BUFSIZE 分の空きがあるか。
	if (ws->bufsize - ws->buflen < BUFSIZE) {
		uint newsize = ws->bufsize + BUFSIZE;
		uint8 *newbuf = realloc(ws->buf, newsize);
		if (newbuf == NULL) {
			Debug(diag, "%s: realloc(%u): %s", __func__, newsize, strerrno());
			return -1;
		}
		ws->buf = newbuf;
		ws->bufsize = newsize;
	}

	// ブロッキング。
	r = net_read(ws->net, ws->buf + ws->buflen, ws->bufsize - ws->buflen);
	if (r == 0) {
		Debug(diag, "%s: EOF", __func__);
		return 0;
	}
	if (0) {
		int j;
		for (j = 0; j < r; j++) {
			printf(" %02x", ws->buf[ws->buflen + j]);
			if ((j % 16) == 15) {
				printf("\n");
			}
		}
		if ((j % 16) != 15) {
			printf("\n");
		}
	}
	ws->buflen += r;

	// 読めたので処理する。
	uint pos = ws->bufpos;
	uint8 opbyte = ws->buf[pos++];
	uint8 opcode = opbyte & 0x0f;
	bool fin     = opbyte & WS_OPFLAG_FIN;
	uint datalen;
	pos += ws_decode_len(&ws->buf[pos], &datalen);

	// ペイロードを全部読み込めているか。
	if (ws->buflen - pos < datalen) {
		// 足りなければ次のフレームを待つ。
		Debug(diag, "%s: wait more data: filled=%u < datalen=%u", __func__,
			ws->buflen - pos, datalen);
		return 1;
	}

	// このペイロードは全部受信出来ているので現在位置は進めてよい。
	ws->bufpos = pos;

	// opcode ごとの処理。
	// バイナリフレームは未対応。
	if (opcode == WS_OPCODE_PING) {
		Debug(diag, "%s: PING len=%u", __func__, datalen);
		wsclient_send_pong(ws);
		rv = 1;
	} else if (opcode == WS_OPCODE_CLOSE) {
		Debug(diag, "%s: CLOSE", __func__);
		return 0;
	} else if (opcode == WS_OPCODE_TEXT || opcode == WS_OPCODE_CONT) {
		// テキストフレーム。
		if (ws->opcode == WS_OPCODE_TEXT) {
			Debug(diag, "%s: TEXT len=%u", __func__, datalen);
			ws->opcode = opcode;
			string_clear(ws->text);
		} else {
			Debug(diag, "%s: CONT len=%u", __func__, datalen);
		}
		string_append_mem(ws->text, &ws->buf[ws->bufpos], datalen);
		if (fin) {
			rv = 2;
		}
	} else {
		// 知らないフレーム。
		Debug(diag, "%s: Unsupported frame code=0x%x, len=0x%x", __func__,
			opcode, datalen);
	}

	ws->bufpos += datalen;

	// 受信バッファを読み終えていれば先頭に巻き戻す。
	if (ws->bufpos == ws->buflen) {
		ws->bufpos = 0;
		ws->buflen = 0;
	}

	if (rv == 2) {
		if (ws->callback) {
			(ws->callback)(ws->text);
		}
		string_clear(ws->text);
	}

	return rv;
}

// テキストフレームを送信する。
ssize_t
wsclient_send_text(wsclient *ws, const char *buf)
{
	return wsclient_send(ws, WS_OPCODE_TEXT, buf, strlen(buf));
}

// PONG 応答を送信する。
static void
wsclient_send_pong(wsclient *ws)
{
	wsclient_send(ws, WS_OPCODE_PONG, NULL, 0);
}

// WebSocket フレームを送信する。
// datalen が 0 なら data は NULL でも可。
static int
wsclient_send(wsclient *ws, uint8 opcode, const void *data, uint datalen)
{
	uint8 buf[1 + (1+2) + 4 + datalen];
	uint hdrlen;
	int r;
	union {
		uint8 buf[4];
		uint32 u32;
	} key;

	// ヘッダを作成。継続はとりあえず無視。
	// OP & len
	buf[0] = opcode | WS_OPFLAG_FIN;
	hdrlen = 1;
	hdrlen += ws_encode_len(&buf[1], datalen);
	// Mask
	buf[1] |= WS_MASK_BIT;
	key.u32 = rnd_get32();
	memcpy(&buf[hdrlen], &key.buf, sizeof(key.buf));
	hdrlen += sizeof(key.buf);

	// データをマスクする。
	const uint8 *src = data;
	uint8 *masked = &buf[hdrlen];
	for (uint i = 0; i < datalen; i++) {
		masked[i] = src[i] ^ key.buf[i % 4];
	}

	// 送信。
	uint framelen = hdrlen + datalen;
	r = net_write(ws->net, buf, framelen);
	if (r < 0) {
		Debug(ws->diag, "%s: net_write(%u): %s", __func__,
			framelen, strerrno());
		return -1;
	}

	return datalen;
}

// WebSocket フレームの長さフィールドを作成する。
// 戻り値は書き込んだバイト数。
static uint
ws_encode_len(uint8 *dst, uint len)
{
	uint8 *d = dst;

	if (len < 126) {
		*d++ = len;
	} else if (len < 65536) {
		*d++ = 126;
		*d++ = len >> 8;
		*d++ = len & 0xff;
	} else {
		*d++ = 127;
		*d++ = 0;
		*d++ = 0;
		*d++ = 0;
		*d++ = 0;
		*d++ = (len >> 24) & 0xff;
		*d++ = (len >> 16) & 0xff;
		*d++ = (len >>  8) & 0xff;
		*d++ =  len        & 0xff;
	}

	return d - dst;
}

// WebSocket フレームの長さフィールドからデータ長を読み出して *lenp に格納する。
// 長さは 32ビットまでしか対応していない。
// 戻り値は読み進めたバイト数。
static uint
ws_decode_len(const uint8 *src, uint *lenp)
{
	const uint8 *s = src;
	uint8 s0;
	uint len;

	s0 = *s++;
	if (s0 < 126) {
		len = s0;
	} else if (s0 == 126) {
		len  = (*s++) << 8;
		len |= (*s++);
	} else {
		// 32 ビット以上は無視。
		s += 4;
		len  = (*s++) << 24;
		len |= (*s++) << 16;
		len |= (*s++) <<  8;
		len |= (*s++);
	}

	*lenp = len;
	return s - src;
}

#if defined(TEST)
//
// % cc -o wsclient wsclient.c libcommon.a
//

#include <err.h>
#include <stdio.h>
#include <signal.h>

static struct net_opt opt;

static int
testhttp(const diag *diag, int ac, char *av[])
{
	struct net *net;
	const char *host;
	const char *serv;
	const char *path;

	host = av[2];
	serv = av[3];
	path = av[4];

	if (strcmp(serv, "http") != 0 && strcmp(serv, "https") != 0) {
		errx(1, "<serv> argument must be \"http\" or \"https\"");
	}
	net = net_create(diag);
	if (net == NULL) {
		err(1, "%s: net_create failed", __func__);
	}

	if (net_connect(net, serv, host, serv, &opt) == false) {
		err(1, "%s:%s: connect failed", host, serv);
	}

	// HTTP ヘッダを送信。
	string *hdr = string_init();
	string_append_printf(hdr, "GET %s HTTP/1.1\r\n", path);
	string_append_printf(hdr, "Host: %s\r\n", host);
	string_append_cstr(hdr, "\r\n");
	int n = net_write(net, string_get(hdr), string_len(hdr));
	printf("net_write=%d\n", n);

	// HTTP 応答を受信して表示。
	string *buf;
	while ((buf = net_gets(net)) != NULL) {
		string_rtrim_inplace(buf);
		bool end = (string_len(buf) == 0);
		printf("%s\n", string_get(buf));
		string_free(buf);
		if (end)
			break;
	}
	// 本文は無視。

	net_close(net);
	net_destroy(net);
	return 0;
}

// 表示するだけのコールバック。
static void
cat_callback(const string *s)
{
	printf("%s (%u bytes)\n", string_get(s), string_len(s));
}

// WebSocket エコークライアント…にしたいが、今のところ1往復のみ。
static int
testwsecho(const diag *diag, int ac, char *av[])
{
	wsclient *ws = wsclient_create(diag);
	wsclient_init(ws, cat_callback);

	int sock = wsclient_connect(ws, av[2], &opt);
	if (sock < 0) {
		err(1, "wsclient_connect failed");
	}

	// 1回だけ標準入力を受け付ける。
	char sendbuf[100];
	if (fgets(sendbuf, sizeof(sendbuf), stdin) == NULL) {
		err(1, "fgets failed");
	}
	wsclient_send_text(ws, sendbuf);

	for (;;) {
		int r;

		r = wsclient_process(ws);
		if (r < 0) {
			warn("read");
			break;
		}
		if (r == 0) {
			printf("EOF\n");
			break;
		}
	}

	wsclient_destroy(ws);

	return 0;
}

static int
testmisskey(const diag *diag, int ac, char *av[])
{
	wsclient *ws = wsclient_create(diag);
	wsclient_init(ws, cat_callback);

	int sock = wsclient_connect(ws, av[2], &opt);
	if (sock < 0) {
		err(1, "wsclient_connect failed");
	}

	char cmd[128];
	snprintf(cmd, sizeof(cmd), "{\"type\":\"connect\",\"body\":{"
		"\"channel\":\"localTimeline\",\"id\":\"sayaka-%08x\"}}",
		rnd_get32());
	if (wsclient_send_text(ws, cmd) < 0) {
		warn("%s: Sending command failed", __func__);
		return -1;
	}

	for (;;) {
		int r = wsclient_process(ws);
		if (r < 1) {
			break;
		}
		if (r == 1) {
			continue;
		}
	}

	wsclient_destroy(ws);

	return 0;
}

int
main(int ac, char *av[])
{
	diag *diag = diag_alloc();
	diag_set_level(diag, 2);

	net_opt_init(&opt);
	signal(SIGPIPE, SIG_IGN);

	if (ac == 5 && strcmp(av[1], "http") == 0) {
		return testhttp(diag, ac, av);
	}
	if (ac == 3 && strcmp(av[1], "ws") == 0) {
		return testwsecho(diag, ac, av);
	}
	if (ac == 3 && strcmp(av[1], "misskey") == 0) {
		return testmisskey(diag, ac, av);
	}

	printf("usage: %s http <host> <serv> <path> ... HTTP/HTTPS client\n",
		getprogname());
	printf("       %s wsecho  <url> ... WebSocket echo client\n",
		getprogname());
	printf("       %s misskey <url> ... Misskey WebSocket test\n",
		getprogname());
	return 0;
}

#endif // TEST
