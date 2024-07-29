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

#include "TLSHandle.h"
#include <memory>
#include <vector>

class TLSHandle_openssl_inner;

class TLSHandle_openssl : public TLSHandleBase
{
	using inherited = TLSHandleBase;
 public:
	TLSHandle_openssl();
	virtual ~TLSHandle_openssl() override;

	// ディスクリプタを持っているのでコピーコンストラクタを禁止する。
	TLSHandle_openssl(const TLSHandle_openssl&) = delete;
	TLSHandle_openssl& operator=(const TLSHandle_openssl&) = delete;

	// 初期化
	bool Init() override;

	// 接続に使用する CipherSuites を RSA_WITH_AES_128_CBC_SHA に限定する。
	bool UseRSA() override;

	// 接続する
	bool Connect(const char *hostname, const char *servname) override;

	// クローズする
	void Close() override;

	// 読み書き
	ssize_t Read(void *buf, size_t len) override;
	ssize_t Write(const void *buf, size_t len) override;

	// ブロッキングモード/ノンブロッキングモードに設定する。
	bool SetBlock() override;
	bool SetNonBlock() override;

	// 生ディスクリプタ取得
	int GetFd() const override { return fd; }

	// HMAC-SHA1 したバイナリを返す (OAuth 用)
	static std::vector<uint8> HMAC_SHA1(const std::string& key,
		const std::string& msg);

 private:
	bool ConnectSocket(const char *hostname, const char *servname);
	bool SetBlocking(bool block);

	int fd {};
	std::unique_ptr<TLSHandle_openssl_inner> inner /*{}*/;
};
