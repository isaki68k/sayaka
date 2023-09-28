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

#include "WSClient.h"
#include "OAuth.h"
#include "StringUtil.h"
#include <cstring>
#include <random>

static ssize_t wsclient_recv_callback(wslay_event_context_ptr ctx,
	uint8 *buf, size_t len, int flags, void *aux);
static ssize_t wsclient_send_callback(wslay_event_context_ptr ctx,
	const uint8 *buf, size_t len, int flags, void *aux);
static int wsclient_genmask_callback(wslay_event_context_ptr ctx,
	uint8 *buf, size_t len, void *aux);
static void wsclient_on_msg_recv_callback(wslay_event_context_ptr ctx,
	const wslay_event_on_msg_recv_arg *arg, void *aux);


// コンストラクタ
WSClient::WSClient()
{
}

// デストラクタ
WSClient::~WSClient()
{
	Close();
}

// 初期化
bool
WSClient::Init(const Diag& diag_,
	wsclient_onmsg_callback_t onmsg_callback_, void *onmsg_arg_)
{
	diag = diag_;

	// メッセージ受信コールバック。
	onmsg_callback = onmsg_callback_;
	onmsg_arg = onmsg_arg_;

	// コンテキストを用意。
	wslay_event_callbacks callbacks = {
		.recv_callback			= wsclient_recv_callback,
		.send_callback			= wsclient_send_callback,
		.genmask_callback		= wsclient_genmask_callback,
		.on_msg_recv_callback	= wsclient_on_msg_recv_callback,
	};
	if (wslay_event_context_client_init(&wsctx, &callbacks, this) != 0) {
		Debug(diag, "Init: wslay_event_context_client_init failed\n");
		return false;
	}

	// 乱数のシードを用意。
	std::random_device rdev;
	std::mt19937 mt(rdev());
	std::uniform_int_distribution<> rand(0);
	maskseed = rand(mt);

	// HTTP オブジェクト。
	http.reset(new HttpClient());
	if ((bool)http == false) {
		return false;
	}

	return true;
}

// 接続先を指定。
bool
WSClient::SetURI(const std::string& uri_)
{
	// XXX Init 後、Connect 前でないといけない。
	if ((bool)http == false) {
		return false;
	}

	if (http->Init(diag, uri_) == false) {
		return false;
	}

	return true;
}

// 接続してハンドシェイクフェーズを通過するまで。
// 成功すれば true を返す。
bool
WSClient::Connect()
{
	if (http->Connect() == false) {
		Debug(diag, "Connect: http->Connect failed\n");
		return false;
	}

	// キーのための乱数を用意。
	std::vector<uint8> nonce(16);
	Random(nonce.data(), nonce.size());
	std::string key = OAuth::Base64Encode(nonce);

	// ヘッダ送信。
	std::string header;
	header = string_format("GET %s HTTP/1.1\r\n", http->Uri.PQF().c_str());
	header += string_format("Host: %s\r\n", http->Uri.Host.c_str());
	// User-Agent は SHOULD だが、ないとわりとけられる。
	header += "User-Agent: " + http->user_agent + "\r\n";

	header += "Upgrade: websocket\r\n"
			  "Connection: Upgrade\r\n"
			  "Sec-WebSocket-Version: 13\r\n";
	header += string_format("Sec-WebSocket-Key: %s\r\n", key.c_str());
	header += "\r\n";
	Debug(diag, "Connect: header=|%s|\n", header.c_str());
	if (http->Write(header.c_str(), header.size()) < 0) {
		return false;
	}

	// ヘッダを受信。
	// XXX どうしたもんか。
	http->mstream.reset(new mTLSInputStream(http->mtls.get(), diag));
	http->ReceiveHeader();
	http->mstream.reset();

	if (http->ResultCode != 101) {
		// メッセージは ResultMsg に入っている
		errno = ENOTCONN;
		return false;
	}

	// XXX Sec-WebSocket-Accept のチェック。

	return true;
}

void
WSClient::Close()
{
	// 本当は CLOSE を送るとかだが。

	if ((bool)http) {
		http->Close();
	}
	http.reset();
}

// 上位からの書き込み。
// といっても送信キューに置くだけ。実際にはイベントループで送信される。
// 成功すればキューに置いたバイト数を返す。
// 失敗すれば errno をセットし -1 を返す。
ssize_t
WSClient::Write(const void *buf, size_t len)
{
	wslay_event_msg msg;
	msg.opcode = WSLAY_TEXT_FRAME;
	msg.msg = (const uint8 *)buf;
	msg.msg_length = len;

	int r = wslay_event_queue_msg(GetContext(), &msg);
	if (r != 0) {
		Debug(diag, "wslay_event_queue_msg failed: %d\n", r);
		// エラーメッセージを読み替える。
		switch (r) {
		 case WSLAY_ERR_NO_MORE_MSG:		errno = ESHUTDOWN;	break;
		 case WSLAY_ERR_INVALID_ARGUMENT:	errno = EINVAL;	break;
		 case WSLAY_ERR_NOMEM:				errno = ENOMEM;	break;
		 default:
			errno = EIO;
			break;
		}
		return -1;
	}

	return msg.msg_length;
}

// 生ディスクリプタを取得。
int
WSClient::GetFd() const
{
	if ((bool)http == false) {
		return -1;
	}
	return http->GetFd();
}

// 下位からの受信要求コールバック。
ssize_t
WSClient::RecvCallback(wslay_event_context_ptr ctx,
	uint8 *buf, size_t len, int flags)
{
	ssize_t r;
	for (;;) {
		r = http->Read(buf, len);
		if (r < 0) {
			if (errno == EINTR) {
				continue;
			}
			if (errno == EAGAIN || errno == EWOULDBLOCK) {
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
WSClient::SendCallback(wslay_event_context_ptr ctx,
	const uint8 *buf, size_t len, int flags)
{
	ssize_t r;

	for (;;) {
		r = http->Write(buf, len);
		if (r < 0) {
			if (errno == EINTR) {
				continue;
			}
			if (errno == EAGAIN || errno == EWOULDBLOCK) {
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
WSClient::GenmaskCallback(wslay_event_context_ptr ctx, uint8 *buf, size_t len)
{
	Random(buf, len);
	return 0;
}

// buf から len バイトを乱数で埋める。
void
WSClient::Random(uint8 *buf, size_t len)
{
	uint8 *p = buf;
	uint32 r = 0;	// shut up warning
	uint i = 0;

	if (__predict_true(((uintmax_t)p & 3) == 0)) {
		uint len4 = (len / 4) * 4;
		for (; i < len4; i += 4) {
			*(uint32 *)p = xorshift32();
			p += 4;
		}
	}

	for (; i < len; i++) {
		if (__predict_false((i % 4) == 0)) {
			r = xorshift32();
		}
		*p++ = r;
		r >>= 8;
	}
}

uint32
WSClient::xorshift32()
{
	uint32 y = maskseed;
	y ^= y << 13;
	y ^= y >> 17;
	y ^= y << 15;
	maskseed = y;
	return y;
}


// C のコールバック関数

static ssize_t
wsclient_recv_callback(wslay_event_context_ptr ctx,
	uint8 *buf, size_t len, int flags, void *aux)
{
	WSClient *client = (WSClient *)aux;
	return client->RecvCallback(ctx, buf, len, flags);
}

static ssize_t
wsclient_send_callback(wslay_event_context_ptr ctx,
	const uint8 *buf, size_t len, int flags, void *aux)
{
	WSClient *client = (WSClient *)aux;
	return client->SendCallback(ctx, buf, len, flags);
}

static int
wsclient_genmask_callback(wslay_event_context_ptr ctx,
	uint8 *buf, size_t len, void *aux)
{
	WSClient *client = (WSClient *)aux;
	return client->GenmaskCallback(ctx, buf, len);
}

static void
wsclient_on_msg_recv_callback(wslay_event_context_ptr ctx,
	const wslay_event_on_msg_recv_arg *msg, void *aux)
{
	// 呼び出し側が Init() にセットしたコールバックを呼ぶ。
	WSClient *client = (WSClient *)aux;
	auto callback = client->onmsg_callback;

	(*callback)(client->onmsg_arg, ctx, msg);
}
