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
#include "Dictionary.h"
#include <string>

class OAuth
{
 public:
	OAuth();
	OAuth(const Diag& diag_);

	void SetDiag(const Diag& diag_);

	// access token, secret をファイルから読み込む。
	bool LoadTokenFromFile(const std::string& filename);
	bool SaveTokenToFile(const std::string& filename) const;

	// Nonce のための文字列を返す。
	// (呼び出すたびに異なる文字列を生成する)
	// テスト側で差し替えるため weak 指定。
	static std::string GetNonce(int len = 16) __attribute__((__weak__));

	// HMAC-SHA1 したバイナリを返す。
	static std::vector<uint8> HMAC_SHA1(const std::string& key,
		const std::string& msg);

	// HMAC-SHA1 してバイナリを Base64 した文字列を返す。
	static std::string HMAC_SHA1_Base64(const std::string& key,
		const std::string& msg);

	// パラメータを作ってアクセス URI を返す
	std::string CreateParams(const std::string& method,
		const std::string& uri);

	// paramdict を "key1=value1&key2=value2&..." 形式にエンコードする
	static std::string MakeQuery(const StringDictionary& paramdict);

	// "key1=value1&key2=value2&..." 形式の s をパースして dict に代入する
	static void ParseQuery(StringDictionary& dict, const std::string& s);

	// OAuthParams から OAuth ヘッダを作成する。
	// OAuthParams には authorization: OAuth ヘッダに載せるすべての
	// パラメータを代入しておくこと。
	std::string MakeOAuthHeader();

	// uri_request_token に接続しトークンを取得する。
	// 取得したトークンとシークレットは AccessToken, AccessSecret に格納する。
	void RequestToken(const std::string& uri_request_token);

 public:
	std::string ConsumerKey {};
	std::string ConsumerSecret {};

	// OAuth ヘッダに書き出すパラメータ
	StringDictionary OAuthParams {};

	// リクエストのパラメータ
	// (URI の Query 句がまだ使えないので)
	StringDictionary AdditionalParams {};

	// OAuth ヘッダモードでは true を設定のこと。
	bool UseOAuthHeader {};

	// アクセストークンとアクセスシークレット
	std::string AccessToken {};
	std::string AccessSecret {};

 private:
	Diag diag {};
};
