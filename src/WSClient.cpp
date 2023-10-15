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
#include "Base64.h"
#include "HttpClient.h"
#include "StringUtil.h"
#include <cstring>

static ssize_t wsclient_recv_callback(wslay_event_context_ptr ctx,
	uint8 *buf, size_t len, int flags, void *aux);
static ssize_t wsclient_send_callback(wslay_event_context_ptr ctx,
	const uint8 *buf, size_t len, int flags, void *aux);
static int wsclient_genmask_callback(wslay_event_context_ptr ctx,
	uint8 *buf, size_t len, void *aux);
static void wsclient_on_msg_recv_callback(wslay_event_context_ptr ctx,
	const wslay_event_on_msg_recv_arg *arg, void *aux);


// コンストラクタ
WSClient::WSClient(Random& rnd_, const Diag& diag_)
	: rnd(rnd_), diag(diag_)
{
}

// デストラクタ
WSClient::~WSClient()
{
	Close();

	wslay_event_context_free(wsctx);
}

// 初期化。
// Init() はコンストラクト後は1回のみ呼び出し可能。
// 成功すれば true を、失敗すれば errno をセットして false を返す。
bool
WSClient::Init(wsclient_onmsg_callback_t onmsg_callback_, void *onmsg_arg_)
{
	if (wsctx) {
		errno = EBUSY;
		return false;
	}

	// コンテキストを用意。
	wslay_event_callbacks callbacks = {
		.recv_callback			= wsclient_recv_callback,
		.send_callback			= wsclient_send_callback,
		.genmask_callback		= wsclient_genmask_callback,
		.on_msg_recv_callback	= wsclient_on_msg_recv_callback,
	};
	int r = wslay_event_context_client_init(&wsctx, &callbacks, this);
	if (r != 0) {
		Debug(diag, "%s: wslay_event_context_client_init failed: %d\n",
			__method__, r);
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

	// メッセージ受信コールバックを設定。
	onmsg_callback = onmsg_callback_;
	onmsg_arg = onmsg_arg_;

	return true;
}

// 接続先を指定してオープン。まだ接続はしない。
// 成功すれば true、失敗すれば false を返す。
bool
WSClient::Open(const std::string& uri_)
{
	http.reset(new HttpClient(diag));
	if ((bool)http == false) {
		Debug(diag, "%s: HttpClient failed", __method__);
		return false;
	}

	if (http->Open(uri_) == false) {
		Debug(diag, "%s: HttpClient.Open failed", __method__);
		return false;
	}

	return true;
}

// 接続してハンドシェイクフェーズを通過するまで。
// 成功すれば true を返す。
bool
WSClient::Connect()
{
	tstream = http->Connect();
	if (tstream == NULL) {
		Debug(diag, "Connect: http->Connect failed\n");
		return false;
	}

	// キーのための乱数を用意。
	std::vector<uint8> nonce(16);
	rnd.Fill(nonce.data(), nonce.size());
	std::string key = Base64Encode(nonce);

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
	if (http->SendRequest(header) == false) {
		return false;
	}

	// ヘッダを受信。
	http->ReceiveHeader();
	if (http->ResultCode != 101) {
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

// HTTP 応答コードを取得。なければ 0。
int
WSClient::GetHTTPCode() const
{
	if ((bool)http == false) {
		return 0;
	}
	return http->ResultCode;
}

// 下位からの受信要求コールバック。
ssize_t
WSClient::RecvCallback(wslay_event_context_ptr ctx,
	uint8 *buf, size_t len, int flags)
{
	ssize_t r;
	for (;;) {
		r = tstream->Read(buf, len);
		if (r < 0) {
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
WSClient::SendCallback(wslay_event_context_ptr ctx,
	const uint8 *buf, size_t len, int flags)
{
	ssize_t r;

	for (;;) {
		r = tstream->Write(buf, len);
		if (r < 0) {
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
WSClient::GenmaskCallback(wslay_event_context_ptr ctx, uint8 *buf, size_t len)
{
	rnd.Fill(buf, len);
	return 0;
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
