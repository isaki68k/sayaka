/*
 * Copyright (C) 2015 Y.Sugahara (moveccr)
 * Copyright (C) 2021 Tetsuya Isaki
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

#include "HttpClient.h"
#include "ChunkedInputStream.h"
#if defined(USE_MBEDTLS)
#include "TLSHandle_mbedtls.h"
#else
#include "TLSHandle_openssl.h"
#endif
#include "StringUtil.h"
#include <errno.h>
#include <sys/socket.h>

// コンストラクタ
HttpClient::HttpClient()
{
	family = AF_UNSPEC;
	user_agent = "sayaka";
}

// コンストラクタ (Diag)
HttpClient::HttpClient(const Diag& diag_)
	: HttpClient()
{
	SetDiag(diag_);
}

// デストラクタ
HttpClient::~HttpClient()
{
	Close();
}

// diag を設定する。
void
HttpClient::SetDiag(const Diag& diag_)
{
	diag = diag_;
}

// uri をターゲットにしてオープンする。
// オープンといってもまだ接続はしない。Close() の対比として。
bool
HttpClient::Open(const std::string& uri_)
{
#if defined(USE_MBEDTLS)
	mtls.reset(new TLSHandle_mbedtls());
#else
	mtls.reset(new TLSHandle_openssl());
#endif

	if (mtls->Init() == false) {
		Debug(diag, "%s: TLSHandle.Init failed", __method__);
		return false;
	}

	Uri = ParsedUri::Parse(uri_);
	Debug(diag, "Uri=|%s|", Uri.to_string().c_str());

	// メンバ変数を初期化。Location でもう一度来る可能性がある。
	SendHeaders.clear();
	RecvHeaders.clear();
	ResultLine.clear();
	ResultMsg.clear();
	ResultCode = 0;
	Ciphers.clear();

	tstream.reset();
	chunk_stream.reset();

	return true;
}

// 接続を閉じる。
void
HttpClient::Close()
{
	Trace(diag, "%s()", __func__);

	// 解放順序あり。
	chunk_stream.reset();
	tstream.reset();
	if ((bool)mtls) {
		mtls->Close();
	}
	mtls.reset();
}

// uri へ GET/POST して、ストリームを返す (GET と POST の共通部)。
// ストリームはこちらが所有するので、呼び出し側は解放しないこと。
Stream *
HttpClient::Act(const std::string& method)
{
	Trace(diag, "%s()", method.c_str());

	if ((bool)mtls == false) {
		Trace(diag, "%s: mtls not initialized", __method__);
		return NULL;
	}

	for (;;) {
		if (Connect() == false) {
			return NULL;
		}

		SendRequest(method);

		tstream.reset(new TLSStream(mtls.get(), diag));

		ReceiveHeader();

		if (300 <= ResultCode && ResultCode < 400) {
			Close();
			auto location = GetHeader(RecvHeaders, "Location");
			Debug(diag, "Redirect to %s", location.c_str());
			if (!location.empty()) {
				auto newUri = ParsedUri::Parse(location);
				if (!newUri.Scheme.empty()) {
					// Scheme があればフルURIとみなす
					Uri = ParsedUri::Parse(location);
				} else {
					// そうでなければ相対パスとみなす
					Uri.Path = newUri.Path;
					Uri.Query = newUri.Query;
					Uri.Fragment = newUri.Fragment;
				}
				Debug(diag, "New URI=|%s|", Uri.to_string().c_str());
				Open(Uri.to_string());
				continue;
			}
		} else if (ResultCode >= 400) {
			// メッセージは ResultMsg に入っている
			errno = ENOTCONN;
			return NULL;
		}
		break;
	}

	Stream *stream;
	auto transfer_encoding = GetHeader(RecvHeaders, "Transfer-Encoding");
	if (transfer_encoding == "chunked") {
		// チャンク
		Debug(diag, "use ChunkedInputStream");
		chunk_stream.reset(new ChunkedInputStream(tstream.get(), diag));
		stream = chunk_stream.get();
	} else {
		// そうでなければ元ストリームをポジションリセットして使う。
		// ここがコンテンツの先頭になるので。
		tstream.reset(new TLSStream(mtls.get(), diag));
		stream = tstream.get();
	}

	return stream;
}

// GET/POST リクエストを発行する
void
HttpClient::SendRequest(const std::string& method)
{
	std::string sb;

	std::string path = (method == "POST") ? Uri.Path : Uri.PQF();
	sb += string_format("%s %s HTTP/1.1\r\n", method.c_str(), path.c_str());

	for (const auto& h : SendHeaders) {
		sb += h.c_str();
		sb += "\r\n";
	}
	sb += "Connection: close\r\n";
	sb += string_format("Host: %s\r\n", Uri.Host.c_str());

	// User-Agent は SHOULD
	sb += "User-Agent: " + user_agent + "\r\n";

	if (method == "POST") {
		sb += "Content-Type: application/x-www-form-urlencoded\r\n";
		sb += string_format("Content-Length: %zd\r\n", Uri.Query.length());
		sb += "\r\n";
		sb += Uri.Query;
	} else {
		sb += "\r\n";
	}

	Debug(diag, "Request %s\n%s", method.c_str(), sb.c_str());

	mtls->Write(sb.c_str(), sb.length());
	mtls->Shutdown(SHUT_WR);

	Trace(diag, "%s() request sent", __func__);
}

// ヘッダを受信する
bool
HttpClient::ReceiveHeader()
{
	size_t r;

	Trace(diag, "%s()", __func__);

	RecvHeaders.clear();

	// 1行目は応答
	r = tstream->ReadLine(&ResultLine);
	if (r <= 0) {
		return false;
	}
	if (ResultLine.empty()) {
		return false;
	}
	Debug(diag, "HEADER |%s|", ResultLine.c_str());

	auto proto_arg = Split2(ResultLine, " ");
	auto protocol = proto_arg.first;
	ResultMsg = proto_arg.second;
	if (protocol == "HTTP/1.1" || protocol == "HTTP/1.0") {
		auto code_msg = Split2(ResultMsg, " ");
		auto code = code_msg.first;
		ResultCode = stou32def(code, -1);
		Debug(diag, "ResultCode=%d", ResultCode);
	}

	// 2行目以降のヘッダを読み込む
	// XXX 1000行で諦める
	for (int i = 0; i < 1000; i++) {
		std::string s;
		r = tstream->ReadLine(&s);
		if (r <= 0) {
			return false;
		}
		if (s.empty()) {
			return false;
		}
		Debug(diag, "HEADER |%s|", s.c_str());

		// まず行継続の処理
		if (s[0] == ' ') {
			auto& prev = RecvHeaders.back();
			prev += Chomp(s);
			continue;
		}
		// その後で改行等を削って、空行ならここで終了
		s = Chomp(s);
		if (s.empty()) {
			break;
		}
		RecvHeaders.emplace_back(s);
	}
	return false;
}

// 指定のヘッダ配列から指定のヘッダを検索してボディを返す。
// 指定されたヘッダが存在しない場合は "" を返す。
/*static*/ std::string
HttpClient::GetHeader(const std::vector<std::string>& header,
	const std::string& key_)
{
	auto key = StringToLower(key_);
	for (const auto& h : header) {
		auto [ k, v ] = Split2(h, ":");
		k = StringToLower(k);

		if (k == key) {
			return Chomp(v);
		}
	}
	return "";
}

// uri へ接続する。
bool
HttpClient::Connect()
{
	// デフォルトポートの処理
	// ParsedUri はポート番号がない URL だと Port = "" になる。
	if (Uri.Port == "") {
		if (Uri.Scheme == "https" || Uri.Scheme == "wss") {
			Uri.Port = "443";
		} else {
			Uri.Port = "80";
		}
	}

	// 接続
	if (Uri.Scheme == "https" || Uri.Scheme == "wss") {
		mtls->UseSSL(true);
	}
	if (Ciphers == "RSA") {
		// XXX RSA 専用
		mtls->UseRSA();
	}
	Trace(diag, "%s: %s", __func__, Uri.to_string().c_str());
	if (mtls->Connect(Uri.Host, Uri.Port) == false) {
		Debug(diag, "TLSHandle.Connect failed");
		return false;
	}

	return true;
}

// 生ディスクリプタを取得
int
HttpClient::GetFd() const
{
	if ((bool)mtls) {
		return mtls->GetFd();
	} else {
		return -1;
	}
}

// 読み出し。
ssize_t
HttpClient::Read(void *buf, size_t len)
{
	return mtls->Read(buf, len);
}

// 書き込み。
ssize_t
HttpClient::Write(const void *buf, size_t len)
{
	return mtls->Write(buf, len);
}


//
// TLS Stream
//

// コンストラクタ
TLSStream::TLSStream(TLSHandleBase *mtls_, const Diag& diag_)
	: diag(diag_)
{
	mtls = mtls_;
}

// デストラクタ
TLSStream::~TLSStream()
{
}

// 読み出し
ssize_t
TLSStream::Read(void *dst, size_t dstlen)
{
	return (ssize_t)mtls->Read(dst, dstlen);
}
