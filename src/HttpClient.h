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

#pragma once

#include "Diag.h"
#include "ParsedUri.h"
#include "Stream.h"
#include "TLSHandle.h"
#include <memory>
#include <string>
#include <vector>
#include <sys/socket.h>

class ChunkedInputStream;

class TLSStream : public Stream
{
 public:
	TLSStream(TLSHandleBase *mtls_, const Diag& diag_);
	virtual ~TLSStream() override;

	ssize_t Read(void *dst, size_t dstlen) override;

 private:
	TLSHandleBase *mtls {};

	Diag diag {};
};

class HttpClient
{
	friend class WSClient;

 public:
	HttpClient();
	HttpClient(const Diag& diag);
	~HttpClient();

	// Diag を設定。
	void SetDiag(const Diag& diag);

	// uri をターゲットにしてオープンする。
	bool Open(const std::string& uri);
	// 接続を閉じる
	void Close();

	// uri から GET して、ストリームを返す
	Stream *GET() {
		return Act("GET");
	}

	// uri へ POST して、ストリームを返す
	Stream *POST() {
		return Act("POST");
	}

	// uri へ GET/POST して、ストリームを返す
	// GET と POST の共通部。
	Stream *Act(const std::string& method);

	// 送信ヘッダを追加する。
	// s は改行を含まない HTTP ヘッダ1行の形式。
	void AddHeader(const std::string& s) {
		SendHeaders.push_back(s);
	}

	// ヘッダ配列から指定のヘッダを検索してボディを返す。
	// 指定されたヘッダが存在しない場合は "" を返す。
	static std::string GetHeader(const std::vector<std::string>& header,
		const std::string& key);

	// Ciphers を設定する
	void SetCiphers(const std::string& ciphers_) {
		Ciphers = ciphers_;
	}

	// タイムアウトを設定する
	void SetTimeout(int timeout) {
		mtls->SetTimeout(timeout);
	}

	// パース後の URI
	ParsedUri Uri {};

	// リクエスト時にサーバに送る追加のヘッダ
	// Host: はこちらで生成するので呼び出し側は指定しないこと。
	std::vector<std::string> SendHeaders {};

	// 受け取ったヘッダ
	std::vector<std::string> RecvHeaders {};

	// 受け取った応答行 (例えば "HTTP/1.1 404 NotFound")
	std::string ResultLine {};

	// 応答コードとメッセージ部分 (例えば "404 NotFound")
	std::string ResultMsg {};

	// 受け取った応答コード (例えば 404)
	int ResultCode {};

	// コネクションに使用するプロトコルファミリ
	// XXX ただし mbedTLS 版は API が指定に対応していないので、未対応
	int family {};

	// 使用する CipherSuites
	// ただし ""(デフォルト) と "RSA" しか対応していない
	std::string Ciphers {};

	// User-Agent
	std::string user_agent {};

 private:
	// GET/POST リクエストを発行する
	void SendRequest(const std::string& method);

	// ヘッダを受信する
	bool ReceiveHeader();

	// 接続する
	bool Connect();

	// 生ディスクリプタを取得
	int GetFd() const;

	// 読み書き
	ssize_t Read(void *buf, size_t len);
	ssize_t Write(const void *buf, size_t len);

	// mTLS ハンドル
	std::unique_ptr<TLSHandleBase> mtls {};

	// mTLS ストリーム
	std::unique_ptr<TLSStream> tstream {};

	// チャンク用
	std::unique_ptr<ChunkedInputStream> chunk_stream /*{}*/;

	Diag diag {};
};
