/*
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

#include "TLSHandle.h"
#include <memory>
#include <vector>

class TLSHandle_mbedtls_inner;

class TLSHandle_mbedtls : public TLSHandleBase
{
	using inherited = TLSHandleBase;
 public:
	TLSHandle_mbedtls();
	virtual ~TLSHandle_mbedtls() override;

	// ディスクリプタを持っているのでコピーコンストラクタを禁止する。
	TLSHandle_mbedtls(const TLSHandle_mbedtls&) = delete;
	TLSHandle_mbedtls& operator=(const TLSHandle_mbedtls&) = delete;

	bool Init() override;
	bool UseRSA() override;

	// タイムアウトを設定する。デフォルトは -1 (タイムアウトしない)。
	void SetTimeout(int timeout_) override;

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
	int GetFd() const override;

	// HMAC-SHA1 したバイナリを返す (OAuth 用)
	static std::vector<uint8> HMAC_SHA1(const std::string& key,
		const std::string& msg);

	// mbedTLS のデバッグレベルを設定する。
	static void SetLevel(int lv);

 private:
	std::unique_ptr<TLSHandle_mbedtls_inner> inner /*{}*/;

	// mbedTLS のエラーコードを文字列にして返す
	// (static バッファを使っていることに注意)
	char errbuf[128] {};
	const char *errmsg(int code);
};
