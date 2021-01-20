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
#include "Json.h"
#include "OAuth.h"
#include <string>

class TwitterToken
{
 public:
	std::string Token {};
	std::string Secret {};

	bool LoadFromFile(const std::string& filename);
	bool SaveToFile(const std::string& filename);
};

class Twitter
{
 public:
	Twitter();
	Twitter(const Diag& diag_);

	void SetDiag(const Diag& diag_);

	// Ciphers を設定する。
	void SetCiphers(const std::string& ciphers);

	// Access Token を取得するところまで
	void GetAccessToken();

	InputStream *GetAPI(const std::string& apiRoot, const std::string& api,
		const StringDictionary& options);
	InputStream *PostAPI(const std::string& apiRoot, const std::string& api,
		const StringDictionary& options);

	// API に接続し、結果の JSON を返す。
	Json API2Json(const std::string& method,
		const std::string& apiRoot, const std::string& api,
		const StringDictionary& options);

	TwitterToken AccessToken {};

 private:
	InputStream *API(const std::string& method,
		const std::string& apiRoot, const std::string& api,
		const StringDictionary& options);

	OAuth oauth {};

	Diag diag {};

 public:
	static const char accessTokenURL[];
	static const char authorizeURL[];
	static const char requestTokenURL[];

	static const char APIRoot[];
	static const char StreamAPIRoot[];
 private:
	static const char ConsumerKey[];
	static const char ConsumerSecret[];
};

#if defined(SELFTEST)
extern void test_Twitter();
#endif
