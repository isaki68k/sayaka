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
#include <cstring>
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
		// WSClient のためにストリームを返すが、ここでは tstream を使えばいい。
		if (Connect() == NULL) {
			return NULL;
		}

		// ヘッダを送信。
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

		SendRequest(sb);
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
		Debug(diag, "use tstream as-is");
		tstream.reset(new TLSStream(mtls.get(), diag));
		stream = tstream.get();
	}

	return stream;
}

// ヘッダ(とかの)文字列を送信する。
bool
HttpClient::SendRequest(const std::string& header)
{
	if ((int)diag) {
		// デバッグ表示
		std::string buf(header);
		while (buf.empty() == false) {
			const char *e = strchr(buf.c_str(), '\n');
			std::string line;
			if (__predict_true(e != NULL)) {
				size_t len = e - &buf[0] + 1;
				line = buf.substr(0, len);
				buf = buf.substr(len);
			} else {
				line = buf;
				buf.clear();
			}
			line = string_replace(line, "\r\n", "\\r\\n");
			line = string_replace(line, "\n", "\\n");
			diag.Print("Send %s", line.c_str());
		}
	}

	auto r = tstream->Write(header.c_str(), header.length());
	if (r < 0) {
		return false;
	}
	if (r < header.length()) {
		return false;
	}
	// 本当はいるのかも知れないが
	//tstream->Flush();

	Trace(diag, "%s() request sent %zd", __func__, r);
	return true;
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
	Debug(diag, "Recv %s", ResultLine.c_str());

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
		Debug(diag, "Recv %s", s.c_str());

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
Stream *
HttpClient::Connect()
{
	// デフォルトポートの処理
	// ParsedUri はポート番号がない URL だと Port = "" になる。
	if (Uri.Port.empty()) {
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
		return NULL;
	}

	tstream.reset(new TLSStream(mtls.get(), diag));
	return tstream.get();
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
	return mtls->Read(dst, dstlen);
}

// 書き込み
ssize_t
TLSStream::Write(const void *src, size_t srclen)
{
	return mtls->Write(src, srclen);
}
