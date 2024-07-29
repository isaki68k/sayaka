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
#include "StringUtil.h"
#include <cstdarg>
#include <string>

// ここではログにタイムスタンプを付けたい。
#define TRACE(fmt...)	do {	\
	if (diag >= 2) {	\
		PrintTime(NULL);	\
		auto msg = string_format(fmt);	\
		diag.Print("%s %s", __func__, msg.c_str());	\
	}	\
} while (0)
#define VERBOSE(fmt...)	do {	\
	if (diag >= 3) {	\
		PrintTime(NULL);	\
		auto msg = string_format(fmt);	\
		diag.Print("%s %s", __func__, msg.c_str());	\
	}	\
} while (0)

// ここではエラーメッセージにもタイムスタンプを付けたい。
#define ERROR(fmt...)	do {	\
	PrintTime(NULL);	\
	fprintf(stderr, fmt);	\
	fprintf(stderr, "\n");	\
} while (0)

class TLSHandleBase
{
 public:
	TLSHandleBase();
	virtual ~TLSHandleBase();

	// ディスクリプタを持っているのでコピーコンストラクタを禁止する。
	TLSHandleBase(const TLSHandleBase&) = delete;
	TLSHandleBase& operator=(const TLSHandleBase&) = delete;

	// 初期化
	virtual bool Init();

	// HTTPS を使うかどうかを設定する。
	// Init() 後、Connect() および UseRSA() より前に設定すること。
	virtual void UseSSL(bool value);

	// 接続に使用する CipherSuites を RSA_WITH_AES_128_CBC_SHA に限定する。
	// UseSSL(true) 後で Connect() より前に設定すること。
	// UseSSL(false) 時に呼ぶと false を返す。
	// XXX どういう API にすべきか
	virtual bool UseRSA() = 0;

	// アドレスファミリを指定する。デフォルトは AF_UNSPEC。
	// Connect() より先に設定しておくこと。
	void SetFamily(int family_) { family = family_; }

	// タイムアウトを設定する。
	// 0 ならポーリングモード。
	// -1 ならタイムアウトしない。
	// デフォルトは -1。
	virtual void SetTimeout(int timeout_);

	// 接続する
	bool Connect(const std::string& hostname, const std::string& servname) {
		return Connect(hostname.c_str(), servname.c_str());
	}
	virtual bool Connect(const char *hostname, const char *servname) = 0;

	// クローズする
	virtual void Close() = 0;

	// 読み書き
	virtual ssize_t Read(void *buf, size_t len) = 0;
	virtual ssize_t Write(const void *buf, size_t len) = 0;

	// ブロッキングモード/ノンブロッキングモードに設定する。
	virtual bool SetBlock() = 0;
	virtual bool SetNonBlock() = 0;

	// 生ディスクリプタ取得。
	virtual int GetFd() const = 0;

	// ログレベルを設定。
	static void SetLevel(int val);

 protected:
	static void PrintTime(const struct timeval *);

 public:
	bool usessl {};
	int family {};
	int timeout {};		// [msec]
	int ssl_timeout {};

 protected:
	static Diag diag;
};
