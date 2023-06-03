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

#include "sayaka.h"
#include "TLSHandle_openssl.h"
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <openssl/err.h>

// コンストラクタ
TLSHandle_openssl::TLSHandle_openssl()
{
}

// デストラクタ
TLSHandle_openssl::~TLSHandle_openssl()
{
	ERR_free_strings();
}

// 初期化
bool
TLSHandle_openssl::Init()
{
	if (inherited::Init() == false) {
		return false;
	}

	SSL_load_error_strings();
	SSL_library_init();

	return true;
}

// 接続に使用する CipherSuites を RSA_WITH_AES_128_CBC_SHA に限定する。
bool
TLSHandle_openssl::UseRSA()
{
	// OpenSSL での指定方法が分からない。
	warn("--cipher RSA: not supported in openssl mode");
	return false;
}

// 接続
bool
TLSHandle_openssl::Connect(const char *hostname, const char *servname)
{
	int r;

	if (ConnectSocket(hostname, servname) == false) {
		return false;
	}

	ctx = SSL_CTX_new(SSLv23_client_method());
	ssl = SSL_new(ctx);

	r = SSL_set_fd(ssl, fd);
	if (r == 0) {
		ERR_print_errors_fp(stderr);
		return false;
	}

	r = SSL_connect(ssl);
	if (r != 1) {
		ERR_print_errors_fp(stderr);
		return false;
	}

	return true;
}

// ソケットに接続するところまで。
bool
TLSHandle_openssl::ConnectSocket(const char *hostname, const char *servname)
{
	struct addrinfo hints;
	struct addrinfo *ai;
	struct addrinfo *ailist;
	fd_set wfds;
	struct timeval tv;
	bool inprogress;
	int val;
	int r;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = family;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	if (getaddrinfo(hostname, servname, &hints, &ailist) != 0) {
		return false;
	}

	inprogress = false;
	for (ai = ailist; ai != NULL; ai = ai->ai_next) {
		fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
		if (fd < 0) {
			continue;
		}

		// ここでノンブロックに設定
		val = fcntl(fd, F_GETFL);
		if (val < 0) {
			goto abort_continue;
		}
		if (fcntl(fd, F_SETFL, val | O_NONBLOCK) < 0) {
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
		return false;
	}

	// ここでブロッキングに戻す。
	val = fcntl(fd, F_GETFL);
	if (val < 0) {
		close(fd);
		fd = -1;
		return false;
	}
	if (fcntl(fd, F_SETFL, val & ~O_NONBLOCK) < 0) {
		close(fd);
		fd = -1;
		return false;
	}

	// 接続待ちなら
	if (inprogress) {
		FD_ZERO(&wfds);
		FD_SET(fd, &wfds);
		memset(&tv, 0, sizeof(tv));
		tv.tv_sec = timeout / 1000;
		tv.tv_usec = (timeout % 1000) * 1000;
		r = select(fd + 1, NULL, &wfds, NULL, (timeout < 0) ? NULL : &tv);
		if (r <= 0) {
			close(fd);
			fd = -1;
			return false;
		}
	}
	return true;
}

void
TLSHandle_openssl::Close()
{
	SSL_shutdown(ssl);
	SSL_free(ssl);
	SSL_CTX_free(ctx);
	close(fd);
	fd = -1;
}

int
TLSHandle_openssl::Shutdown(int how)
{
	// XXX
	return 0;
}

size_t
TLSHandle_openssl::Read(void *buf, size_t len)
{
	return SSL_read(ssl, buf, len);
}

size_t
TLSHandle_openssl::Write(const void *buf, size_t len)
{
	return SSL_write(ssl, buf, len);
}
