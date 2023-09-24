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

#include "HttpClient.h"
#include <queue>
#include <wslay/wslay.h>

class WSClient
{
 public:
	WSClient();
	~WSClient();

	bool Init(const Diag& diag, const std::string& uri);
	bool Connect();
	void Close();

	ssize_t Read(void *buf, size_t len);
	ssize_t Write(const void *buf, size_t len);

	bool CanRead() const;

	// 生ディスクリプタを取得。
	int GetFd() const;

	wslay_event_context_ptr GetContext() const { return wsctx; }

	// コールバック
	ssize_t RecvCallback(wslay_event_context_ptr ctx,
		uint8 *buf, size_t len, int flags);
	ssize_t SendCallback(wslay_event_context_ptr ctx,
		const uint8 *buf, size_t len, int flags);
	void OnMsgRecvCallback(wslay_event_context_ptr ctx,
		const wslay_event_on_msg_recv_arg *arg);
	int GenmaskCallback(wslay_event_context_ptr ctx, uint8 *buf, size_t len);

 private:
	void Random(uint8 *buf, size_t len);
	uint32 xorshift32();

	std::unique_ptr<HttpClient> http {};

	wslay_event_context_ptr wsctx {};

	std::queue<std::string> recvq {};

	uint32 maskseed {};

	Diag diag {};
};
