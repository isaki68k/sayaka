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
#include <mbedtls/net_sockets.h>
#include <mbedtls/ssl.h>

class mTLSHandle : public TLSHandleBase
{
 public:
	mTLSHandle();
	virtual ~mTLSHandle() override;

	// ディスクリプタを持っているのでコピーコンストラクタを禁止する。
	mTLSHandle(const mTLSHandle&) = delete;
	mTLSHandle& operator=(const mTLSHandle&) = delete;

	// 初期化
	bool Init() override;

	// 接続に使用する CipherSuites を RSA_WITH_AES_128_CBC_SHA に限定する。
	// Connect() より先に設定しておくこと。
	// XXX どういう API にすべきか
	void UseRSA() override;

	// タイムアウトを設定する。デフォルトは 0 (タイムアウトしない)
	void SetTimeout(int timeout_) override;

	// 接続する
	bool Connect(const char *hostname, const char *servname) override;

	// クローズする
	void Close() override;

	// shutdown する
	int Shutdown(int how) override;

	// 読み書き
	size_t Read(void *buf, size_t len) override;
	size_t Write(const void *buf, size_t len) override;

 private:
	// 内部コンテキスト
	mbedtls_net_context net {};
	mbedtls_ssl_context ssl {};
	mbedtls_ssl_config conf {};

	// mbedTLS のエラーコードを文字列にして返す
	// (static バッファを使っていることに注意)
	char errbuf[128] {};
	const char *errmsg(int code);
};
