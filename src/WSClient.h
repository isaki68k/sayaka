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

#pragma once

#include "Diag.h"
#include "Random.h"
#include <memory>
#include <wslay/wslay.h>

class HttpClient;

using wsclient_onmsg_callback_t = void (*)(void *aux,
	wslay_event_context_ptr ctx,
	const wslay_event_on_msg_recv_arg *msg);

class WSClient
{
 public:
	WSClient(Random& rnd_, const Diag& diag_);
	~WSClient();

	bool Init(wsclient_onmsg_callback_t, void *);

	bool Open(const std::string& uri);
	bool Connect();
	void Close();

	ssize_t Write(const void *buf, size_t len);

	// 生ディスクリプタを取得。
	int GetFd() const;

	wslay_event_context_ptr GetContext() const { return wsctx; }

	// HTTP 応答コードを取得。なければ 0。
	int GetHTTPCode() const;

	// コールバック
	ssize_t RecvCallback(wslay_event_context_ptr ctx,
		uint8 *buf, size_t len, int flags);
	ssize_t SendCallback(wslay_event_context_ptr ctx,
		const uint8 *buf, size_t len, int flags);
	int GenmaskCallback(wslay_event_context_ptr ctx, uint8 *buf, size_t len);

	wsclient_onmsg_callback_t onmsg_callback {};
	void *onmsg_arg {};

 private:
	std::unique_ptr<HttpClient> http /*{}*/;

	wslay_event_context_ptr wsctx {};

	Random& rnd;

	Diag diag {};
};
