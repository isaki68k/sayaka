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
// ネットワーク
//

#include "common.h"
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <openssl/err.h>
#include <openssl/ssl.h>

#ifndef UNCONST
#define UNCONST(p)	((void *)(uintptr_t)(const void *)(p))
#endif

struct net;
struct net {
	bool (*f_connect)(struct net *, const char *, const char *);
	ssize_t (*f_read)(struct net *, void *, size_t);
	ssize_t (*f_write)(struct net *, const void *, size_t);
	void (*f_close)(struct net *);
	// f_connect() が確保したリソースを解放する。
	// net 自体はここではなく呼び出し元の net_destroy() が解放する。
	void (*f_cleanup)(struct net *);

	int sock;
	SSL_CTX *ctx;
	SSL *ssl;

	const struct diag *diag;
};

static void sock_cleanup(struct net *);
static bool sock_connect(struct net *, const char *, const char *);
static ssize_t sock_read(struct net *, void *, size_t);
static ssize_t sock_write(struct net *, const void *, size_t);
static void sock_close(struct net *);
static void tls_cleanup(struct net *);
static bool tls_connect(struct net *, const char *, const char *);
static ssize_t tls_read(struct net *, void *, size_t);
static ssize_t tls_write(struct net *, const void *, size_t);
static void tls_close(struct net *);
static int  socket_connect(const char *, const char *);
static int  socket_setblock(int, bool);

// net コンテキストを作成する。
struct net *
net_create(const struct diag *diag)
{
	struct net *net = calloc(1, sizeof(*net));

	net->sock = -1;
	net->diag = diag;

	return net;
}

// net コンテキストを解放する。
void
net_destroy(struct net *net)
{
	if (net) {
		if (net->f_close) {
			net->f_close(net);
		}
		if (net->f_cleanup) {
			net->f_cleanup(net);
		}
		free(net);
	}
}

// scheme://host:serv/ に接続する。
// 失敗すれば errno をセットして false を返す。
bool
net_connect(struct net *net,
	const char *scheme, const char *host, const char *serv)
{
	assert(net);

	// ここでプロトコル選択。
	if (strcmp(scheme, "https") == 0 ||
		strcmp(scheme, "wss") == 0)
	{
		net->f_connect = tls_connect;
		net->f_read    = tls_read;
		net->f_write   = tls_write;
		net->f_close   = tls_close;
		net->f_cleanup = tls_cleanup;
	} else {
		net->f_connect = sock_connect;
		net->f_read    = sock_read;
		net->f_write   = sock_write;
		net->f_close   = sock_close;
		net->f_cleanup = sock_cleanup;
	}

	return net->f_connect(net, host, serv);
}

ssize_t
net_read(struct net *net, void *dst, size_t dstsize)
{
	assert(net);
	return net->f_read(net, dst, dstsize);
}

ssize_t
net_write(struct net *net, const void *src, size_t srcsize)
{
	assert(net);
	return net->f_write(net, src, srcsize);
}

void
net_close(struct net *net)
{
	assert(net);
	net->f_close(net);
}

// 生ソケットを取得する。
int
net_get_fd(const struct net *net)
{
	assert(net);
	return net->sock;
}


//
// 生ソケット
//

static bool
sock_connect(struct net *net, const char *host, const char *serv)
{
	net->sock = socket_connect(host, serv);
	if (net->sock < 0) {
		return false;
	}
	return true;
}

static ssize_t
sock_read(struct net *net, void *dst, size_t dstsize)
{
	return read(net->sock, dst, dstsize);
}

static ssize_t
sock_write(struct net *net, const void *src, size_t srcsize)
{
	return write(net->sock, src, srcsize);
}

static void
sock_close(struct net *net)
{
	if (net->sock >= 3) {
		close(net->sock);
	}
	net->sock = -1;
}

static void
sock_cleanup(struct net *net)
{
}


//
// TLS
//

static bool
tls_connect(struct net *net, const char *host, const char *serv)
{
	const struct diag *diag = net->diag;
	int r;

	static bool initialized = false;
	if (initialized == false) {
		SSL_load_error_strings();
		SSL_library_init();
		initialized = true;
	}

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

	net->sock = socket_connect(host, serv);
	if (net->sock == -1) {
		Debug(diag, "%s: %s:%s failed: %s", __func__, host, serv, strerrno());
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
	sock_close(net);
}

static void
tls_cleanup(struct net *net)
{
	if (net->ssl) {
		SSL_free(net->ssl);
		net->ssl = NULL;
	}
	if (net->ctx) {
		SSL_CTX_free(net->ctx);
		net->ctx = NULL;
	}
}


// 下請け。
// hostname:servname に TCP で接続しそのソケットを返す。
// 失敗すれば errno をセットして -1 を返す。
static int
socket_connect(const char *hostname, const char *servname)
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
		if (socket_setblock(fd, false) < 0) {
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
	if (socket_setblock(fd, true) < 0) {
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

// 下請け。
// ソケット fd のブロッキングモードを変更する。
// blocking = true ならブロッキングモード、
// blocking = false ならノンブロッキングモード。
// 成功すれば 0、失敗すれば errno をセットして -1 を返す。
static int
socket_setblock(int fd, bool blocking)
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