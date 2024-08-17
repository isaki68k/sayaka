/* vi:set ts=4: */
/*
 * Copyright (C) 2023 Tetsuya Isaki
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
// WebSocket
//

#include "sayaka.h"
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <curl/curl.h>
#include <openssl/err.h>
#include <openssl/ssl.h>

#ifndef UNCONST
#define UNCONST(p)	((void *)(uintptr_t)(const void *)(p))
#endif

struct net;
struct net {
	// *_create() と対になるもので net の中身をすべて解放する。
	// net 自体はここでは解放せず呼び出し元が free() すること。
	void (*f_cleanup)(struct net *);

	bool (*f_connect)(struct net *, const char *, const char *);
	ssize_t (*f_read)(struct net *, void *, size_t);
	ssize_t (*f_write)(struct net *, const void *, size_t);
	void (*f_close)(struct net *);

	int sock;
	SSL_CTX *ctx;
	SSL *ssl;

	const struct diag *diag;
};

int  sock_connect(const char *, const char *);
int  sock_setblock(int, bool);
struct net *net_create(const struct diag *);
static void net_cleanup(struct net *);
static bool net_connect(struct net *, const char *, const char *);
static ssize_t net_read(struct net *, void *, size_t);
static ssize_t net_write(struct net *, const void *, size_t);
static void net_close(struct net *);
struct net *tls_create(const struct diag *diag);
static void tls_cleanup(struct net *);
static bool tls_connect(struct net *, const char *, const char *);
static ssize_t tls_read(struct net *, void *, size_t);
static ssize_t tls_write(struct net *, const void *, size_t);
static void tls_close(struct net *);

// hostname:servname に TCP で接続しそのソケットを返す。
// 失敗すれば errno をセットして -1 を返す。
int
sock_connect(const char *hostname, const char *servname)
{
	struct addrinfo hints;
	struct addrinfo *ai;
	struct addrinfo *ailist;
	fd_set wfds;
	struct timeval tv;
	bool inprogress;
	int fd;
	int r;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	if (getaddrinfo(hostname, servname, &hints, &ailist) != 0) {
		return -1;
	}

	inprogress = false;
	for (ai = ailist; ai != NULL; ai = ai->ai_next) {
		fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
		if (fd < 0) {
			continue;
		}

		// ここでノンブロックに設定
		if (sock_setblock(fd, false) < 0) {
			goto abort_continue;
		}

		if (connect(fd, ai->ai_addr, ai->ai_addrlen) == 0) {
			break;
		}
		// ノンブロッキングなので connect() は EINPROGRESS を返す
		if (errno == EINPROGRESS) {
			inprogress = true;
			break;
		}

 abort_continue:
		close(fd);
		fd = -1;
	}
	freeaddrinfo(ailist);

	// 接続出来なかった
	if (fd < 0) {
		return -1;
	}

	// ここでブロッキングに戻す。
	if (sock_setblock(fd, true) < 0) {
		close(fd);
		return -1;
	}

	// 接続待ちなら
	if (inprogress) {
		FD_ZERO(&wfds);
		FD_SET(fd, &wfds);
		memset(&tv, 0, sizeof(tv));
		int timeout = 3000;	// XXX option
		tv.tv_sec = timeout / 1000;
		tv.tv_usec = (timeout % 1000) * 1000;
		r = select(fd + 1, NULL, &wfds, NULL, (timeout < 0) ? NULL : &tv);
		if (r <= 0) {
			close(fd);
			return -1;
		}
	}
	return fd;
}

// ソケット fd のブロッキングモードを変更する。
// blocking = true ならブロッキングモード、
// blocking = false ならノンブロッキングモード。
// 成功すれば 0、失敗すれば errno をセットして -1 を返す。
int
sock_setblock(int fd, bool blocking)
{
	int val;

	val = fcntl(fd, F_GETFL);
	if (val < 0) {
		return -1;
	}

	if (blocking) {
		val &= ~O_NONBLOCK;
	} else {
		val |= O_NONBLOCK;
	}

	if (fcntl(fd, F_SETFL, val) < 0) {
		return -1;
	}

	return 0;
}


struct net *
net_create(const struct diag *diag)
{
	struct net *net = calloc(1, sizeof(*net));

	net->f_connect = net_connect;
	net->f_read    = net_read;
	net->f_write   = net_write;
	net->f_close   = net_close;
	net->f_cleanup = net_cleanup;

	net->sock = -1;
	net->diag = diag;

	return net;
}

static void
net_cleanup(struct net *net)
{
	assert(net);

	net_close(net);
}

static bool
net_connect(struct net *net, const char *host, const char *serv)
{
	assert(net);

	net->sock = sock_connect(host, serv);
	if (net->sock < 0) {
		return false;
	}
	return true;
}

static ssize_t
net_read(struct net *net, void *dst, size_t dstsize)
{
	return read(net->sock, dst, dstsize);
}

static ssize_t
net_write(struct net *net, const void *src, size_t srcsize)
{
	return write(net->sock, src, srcsize);
}

static void
net_close(struct net *net)
{
	if (net->sock >= 3) {
		close(net->sock);
		net->sock = -1;
	}
}


struct net *
tls_create(const struct diag *diag)
{
	static bool initialized = false;
	if (initialized == false) {
		SSL_load_error_strings();
		SSL_library_init();
		initialized = true;
	}

	struct net *net = calloc(1, sizeof(*net));

	net->f_connect = tls_connect;
	net->f_read    = tls_read;
	net->f_write   = tls_write;
	net->f_close   = tls_close;
	net->f_cleanup = tls_cleanup;

	net->sock = -1;
	net->diag = diag;

	return net;
}

static void
tls_cleanup(struct net *net)
{
	assert(net);

	tls_close(net);
	if (net->ssl) {
		SSL_free(net->ssl);
		net->ssl = NULL;
	}
	if (net->ctx) {
		SSL_CTX_free(net->ctx);
		net->ctx = NULL;
	}
}

static bool
tls_connect(struct net *net, const char *host, const char *serv)
{
	const struct diag *diag = net->diag;
	int r;

	assert(net);

	net->ctx = SSL_CTX_new(TLS_client_method());
	if (net->ctx == NULL) {
		Debug(diag, "%s: SSL_CTX_new failed", __func__);
		return false;
	}
	net->ssl = SSL_new(net->ctx);
	if (net->ssl == NULL) {
		Debug(diag, "%s: SSL_new failed", __func__);
		return false;
	}

	net->sock = sock_connect(host, serv);
	if (net->sock == -1) {
		Debug(diag, "%s: sock_connect: %s:%s failed", __func__, host, serv);
		return false;
	}

	r = SSL_set_fd(net->ssl, net->sock);
	if (r == 0) {
		ERR_print_errors_fp(stderr);
		return false;
	}

	r = SSL_set_tlsext_host_name(net->ssl, UNCONST(host));
	if (r != 1) {
		ERR_print_errors_fp(stderr);
		return false;
	}

	if (SSL_connect(net->ssl) < 1) {
		Debug(diag, "%s: SSL_connect failed", __func__);
		return false;
	}

	// 接続できたらログ?
	Debug(net->diag, "%s done", __func__);

	return true;
}

static ssize_t
tls_read(struct net *net, void *dst, size_t dstsize)
{
	const struct diag *diag = net->diag;
	ssize_t r;

	Trace(diag, "%s (dstsize=%zu)", __func__, dstsize);
	r = SSL_read(net->ssl, dst, dstsize);
	if (r < 0) {
		if (SSL_get_error(net->ssl, r) != SSL_ERROR_SYSCALL) {
			// とりあえず何かにしておく。
			errno = EIO;
		}
		Trace(diag, "%s r=%zd, errno=%d", __func__, r, errno);
	} else {
		Trace(diag, "%s r=%zd", __func__, r);
	}
	return r;
}

static ssize_t
tls_write(struct net *net, const void *src, size_t srcsize)
{
	const struct diag *diag = net->diag;
	ssize_t r;

	Trace(diag, "%s (srcsize=%zu)", __func__, srcsize);
	r = SSL_write(net->ssl, src, srcsize);
	if (r < 0) {
		if (SSL_get_error(net->ssl, r) != SSL_ERROR_SYSCALL) {
			// とりあえず何かにしておく。
			errno = EIO;
		}
		Trace(diag, "%s r=%zd, errno=%d", __func__, r, errno);
	} else {
		Trace(diag, "%s r=%zd", __func__, r);
	}
	return r;
}

static void
tls_close(struct net *net)
{
	if (net->ssl) {
		SSL_shutdown(net->ssl);
	}
	// 元ソケットも閉じる。
	net_close(net);
}


struct wsstream {
	struct net *net;
	wslay_event_context_ptr ctx;

	// メッセージ受信コールバック。
	// (wslay で受信が完了したので上位に渡すほう)
	wslay_event_on_msg_recv_callback onmsg_callback;

	const struct diag *diag;
};

// wsstream コンテキストを生成する。
struct wsstream *
wsstream_create(const struct diag *diag)
{
	struct wsstream *ws;

	ws = calloc(1, sizeof(*ws));
	if (ws == NULL) {
		return NULL;
	}

	ws->diag = diag;

	return ws;
}

// ws を解放する。
void
wsstream_destroy(struct wsstream *ws)
{
	if (ws) {
		if (ws->ctx) {
		}
		if (ws->net) {
			ws->net->f_cleanup(ws->net);
			free(ws->net);
		}
		free(ws);
	}
}

// ws を初期化する。
void
wsstream_init(struct wsstream *ws, wslay_event_on_msg_recv_callback callback)
{
	assert(ws->ctx == NULL);

	// wslay コンテキストを用意。
	struct wslay_event_callbacks callbacks = {
		.recv_callback			= wsstream_recv_cb,
		.send_callback			= wsstream_send_cb,
		.genmask_callback		= wsstream_genmask_cb,
		.on_sg_recv_callback	= wsstream_on_msg_recv_cb,
	};
	int r = wslay_event_context_client_init(&ws->ctx, &callbacks, ws);
	if (r != 0) {
		Debug(ws->diag, "%s: wslay_event_context_client_init failed: %d",
			__func__, r);
		switch (r) {
		 case WSLAY_ERR_NOMEM:
			errno = ENOMEM;
			break;
		 default:
			// XXX 他のエラーは来ないはず。
			errno = EINVAL;
			break;
		}
		return false;
	}

	// 呼び出し元(上位)向けの受信コールバック。
	ws->onmsg_callback = callback;
	ws->onmsg_arg = ws;

	return true;
}

// ws からソケットを取得する。
// まだなければ -1 が返る。
int
wsstream_get_fd(const struct wsstream *ws)
{
	assert(ws);

	if (ws->net) {
		return ws->net->sock;
	}
	return -1;
}

// url に接続する。
// 成功すれば 0、失敗すれば -1 を返す。
int
wsstream_connect(struct wsstream *ws, const char *url)
{
	const struct diag *diag = ws->diag;
	string *key = NULL;
	string *hdr = NULL;

	CURLU *cu = curl_url();
	curl_url_set(cu, CURLUPART_URL, url, CURLU_NON_SUPPORT_SCHEME);
	char *scheme = NULL;
	char *host = NULL;
	char *path = NULL;
	curl_url_get(cu, CURLUPART_SCHEME, &scheme, 0);
	curl_url_get(cu, CURLUPART_HOST, &host, 0);
	curl_url_get(cu, CURLUPART_PATH, &path, 0);

	const char *serv;
	if (strcmp(scheme, "ws") == 0) {
		ws->net = net_create(diag);
		serv = "http";
	} else if (strcmp(scheme, "wss") == 0) {
		ws->net = tls_create(diag);
		serv = "https";
	} else {
		Debug(diag, "%s: %s: Unsupported protocol", __func__, url);
		goto abort;
	}

	if (ws->net->f_connect(ws->net, host, serv) == false) {
		Debug(diag, "%s: %s:%s: %s", __func__, host, serv, strerrno());
		goto abort;
	}

	// キー(乱数)を作成。
	char nonce[16];
	rnd_fill(nonce, sizeof(nonce));
	key = base64_encode(nonce, sizeof(nonce));

	// WebSocket ヘッダを送信。
	hdr = string_init();
	// XXX path は PQF にしたほうがいい
	string_append_printf(hdr, "GET %s HTTP/1.1\r\n", path);
	string_append_printf(hdr, "Host: %s\r\n", host);
	string_append_printf(hdr, "User-Agent: sayaka/c\r\n");
	string_append_cstr(hdr,
		"Upgrade: websocket\r\n"
		"Connection: Upgrade\r\n"
		"Sec-WebSocket-Version: 13\r\n");
	string_append_printf(hdr, "Sec-WebSocket-Key: %s\r\n", string_get(key));
	string_append_cstr(hdr,   "\r\n");
	Trace(diag, "<<< %s", string_get(hdr));
	ssize_t sent = ws->net->f_write(ws->net, string_get(hdr), string_len(hdr));
	if (sent < 0) {
		Debug(diag, "%s: f_write: %s", __func__, strerrno());
		goto abort;
	}

	// ヘッダを受信。
	char recvbuf[1024];
	size_t len = 0;
	for (;;) {
		ssize_t n = ws->net->f_read(ws->net,
			recvbuf + len, sizeof(recvbuf) - len);
		if (n < 0) {
			Debug(diag, "%s: f_read: %s", __func__, strerrno());
			goto abort;
		}
		if (n == 0) {
			break;
		}

		// ヘッダを全部受信したか。
		len += n;
		recvbuf[len] = '\0';
		if (len >= 4 && strcmp(&recvbuf[len - 4], "\r\n\r\n") == 0) {
			break;
		}
	}
	Trace(diag, ">>> |%s|", recvbuf);

	// 1行目を雑にチェックする。
	// "HTTP/1.1 101 Switching Protocols\r\n" みたいなのが来るはず。

	// 先頭が "HTTP/1.1"。
	if (strncmp(recvbuf, "HTTP/1.1", 8) != 0) {
		Debug(diag, "%s: No HTTP/1.1 response?", __func__);
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
		goto abort;
	}

	// XXX Sec-WebSocket-Accept のチェックとか。

	return 0;

 abort:
	string_free(hdr);
	string_free(key);
	curl_free(scheme);
	curl_free(host);
	curl_free(path);
	curl_url_cleanup(cu);
	return -1;
}

// 上位からの書き込み。
// といっても送信キューに置くだけ。実際にはイベントループで送信される。
// 成功すればキューに置いたバイト数を返す。
// 失敗すれば errno をセットし -1 を返す。
ssize_t
wsstream_write(struct wsstream *ws, const void *buf, size_t len)
{
	struct wslay_event_msg msg;
	int r;

	memset(&msg, 0, sizeof(msg));
	msg.opcode = WSLAY_TEXT_FRAME;
	msg.msg = (const uint8 *)buf;
	msg.msg_length = len;

	r = wslay_event_queue_msg(ws->ctx, &msg);
	if (r != 0) {
		Debug(ws->diag, "%s: wslay_event_queue_msg failed: %d", __func__, r);
		// エラーメッセージを読み替える。
		switch (r) {
		 case WSLAY_ERR_NO_MORE_MSG:		errno = ESHUTDOWN;	break;
		 case WSLAY_ERR_INVALID_ARGUMENT:	errno = EINVAL;		break;
		 case WSLAY_ERR_NOMEM:				errno = ENOMEM;		break;
		 default:
			errno = EIO;
			break;
		}
		return -1;
	}

	return msg.msg_length;
}

// 下位からの受信要求コールバック。
ssize_t
wsstream_recv_cb(wslay_event_context_ptr ctx,
	uint8 *buf, size_t len, int flags, void *user_data)
{
	struct wsstream *ws = user_data;
	ssize_t r;

	for (;;) {
		r = ws->net->f_read(ws->net, buf, len);
		if (r < 0) {
printf("%s: f_read r=%d\n", __func__, r);
			if (errno == EINTR) {
				continue;
			}
			if (errno == EWOULDBLOCK) {
				wslay_event_set_error(ctx, WSLAY_ERR_WOULDBLOCK);
			} else {
				wslay_event_set_error(ctx, WSLAY_ERR_CALLBACK_FAILURE);
			}
		} else if (r == 0) {
			// Unexpected EOF is also treated as an error.
			wslay_event_set_error(ctx, WSLAY_ERR_CALLBACK_FAILURE);
			r = -1;
		}

		return r;
	}
}

// 下位層への送信要求コールバック。
ssize_t
wsstream_send_cb(wslay_event_context_ptr ctx,
	const uint8 *buf, size_t len, int flags, void *user_data)
{
	struct wsstream *ws = user_data;
	ssize_t r;

	for (;;) {
		r = ws->net->f_write(ws->net, buf, len);
		if (r < 0) {
printf("%s: f_write r=%d\n", __func__, r);
			if (errno == EINTR) {
				continue;
			}
			if (errno == EWOULDBLOCK) {
				wslay_event_set_error(ctx, WSLAY_ERR_WOULDBLOCK);
			} else {
				wslay_event_set_error(ctx, WSLAY_ERR_CALLBACK_FAILURE);
			}
		}
		return r;
	}
}

// 送信マスク作成要求コールバック。
int
wsstream_genmask_cb(wslay_event_context_ptr ctx,
	void *buf, size_t len, void *user_data)
{
	rnd_fill(buf, len);
	return 0;
}

// 下位層からのメッセージ受信完了コールバック。
void
wsstream_on_msg_recv_cb(wslay_event_context_ptr ctx,
	const wslay_event_on_msg_recv_arg *msg, void *user_data)
{
	struct wsstream *ws = user_data;

	if (__predict_false(diag_get_level(ws->diag) >= 1)) {
		const char *opstr;
		char buf[8];
		switch (msg->opcode) {
		 case WSLAY_TEXT_FRAME:			opstr = "TEXT";		break;
		 case WSLAY_BINARY_FRAME:		opstr = "BINARY";	break;
		 case WSLAY_CONNECTION_CLOSE:	opstr = "CLOSE";	break;
		 case WSLAY_PING:				opstr = "PING";		break;
		 case WSLAY_PONG:				opstr = "PONG";		break;
		 default:
			snprintf(buf, sizeof(buf), "0x%x", msg->opcode);
			opstr = buf;
			break;
		}
		diag_print(ws->diag, "%s: opcode=%s len=%d", __func__,
			opstr, (int)msg->msg_length);
	}

	if (!wslay_is_ctrl_frame(msg->opcode)) {
		// クライアント指定のコールバックを呼ぶ。
		(*ws->onmsg_callback)(ws, ctx, msg);
	}
}


#if defined(TEST)

#include <err.h>
#include <stdio.h>

static int
testhttp(const struct diag *diag, int ac, char *av[])
{
	struct net *net;
	const char *host;
	const char *serv;
	const char *path;

	host = av[2];
	serv = av[3];
	path = av[4];

	if (strcmp(serv, "http") == 0) {
		net = net_create(diag);
	} else if (strcmp(serv, "https") == 0) {
		net = tls_create(diag);
	} else {
		errx(1, "%s: invalid service name", serv);
	}

	if (net->f_connect(net, host, serv) == false) {
		err(1, "%s:%s: connect failed", host, serv);
	}

	string *hdr = string_init();
	string_append_printf(hdr, "GET %s HTTP/1.1\r\n", path);
	string_append_printf(hdr, "Host: %s\r\n", host);
	string_append_cstr(hdr, "\r\n");
	ssize_t n = net->f_write(net, string_get(hdr), string_len(hdr));
	printf("write=%zd\n", n);

	char buf[1024];
	ssize_t r = net->f_read(net, buf, sizeof(buf));
	printf("read=%zd\n", r);
	buf[r] = 0;
	printf("buf=|%s|\n", buf);

	net->f_close(net);

	net->f_cleanup(net);
	free(net);

	return 0;
}

static int
testws(const struct diag *diag, int ac, char *av[])
{
	struct wsstream *ws = wsstream_create(diag);

	int r = wsstream_connect(ws, av[2]);
	if (r != 0) {
		err(1, "wsstream_connect failed %d", r);
	}

	for (;;) {
		uint8 buf[100];
		int i;

		r = ws->net->f_read(ws->net, buf, sizeof(buf));
		if (r < 0) {
			warn("f_read");
			break;
		}
		if (r == 0) {
			printf("EOF\n");
			break;
		}

		printf("recv %d bytes:\n", r);
		for (i = 0; i < r; i++) {
			if ((i % 16) == 0) {
				printf("%04x:", i);
			}
			if ((i % 16) == 8) {
				printf(" ");
			}
			printf(" %02x", buf[i]);
			if ((i % 16) == 15) {
				printf("\n");
			}
		}
		if ((i % 16) != 0) {
			printf("\n");
		}
	}

	wsstream_destroy(ws);

	return 0;
}

int
main(int ac, char *av[])
{
	struct diag *diag = diag_alloc();
	diag_set_level(diag, 2);

	if (ac == 5 && strcmp(av[1], "http") == 0) {
		return testhttp(diag, ac, av);
	}
	if (ac == 3 && strcmp(av[1], "ws") == 0) {
		return testws(diag, ac, av);
	}

	printf("usage: %s http <host> <serv> <path> ... HTTP/HTTPS client\n",
		getprogname());
	printf("       %s ws   <url> ... WebSocket test\n",
		getprogname());
	return 0;
}

#endif
