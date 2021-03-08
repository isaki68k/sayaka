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
#include "OAuth.h"
#include "StringUtil.h"
#include "subr.h"
#include <array>
#include <random>
#include <mbedtls/md.h>

// コンストラクタ
OAuth::OAuth()
{
	UseOAuthHeader = true;
}

// コンストラクタ
OAuth::OAuth(const Diag& diag_)
	: OAuth()
{
	SetDiag(diag_);
}

void
OAuth::SetDiag(const Diag& diag_)
{
	diag = diag_;
}

// Nonce のための文字列を返す。
/*static*/ std::string
OAuth::GetNonce()
{
	// twitter のドキュメントには alphanumeric と書いてあるので
	// 0x30-39, 41-5a, 61-7a、個数 = 10+26+26 = 62
	// 0 .. 61 の乱数を求める

	// http://vivi.dyndns.org/tech/cpp/random.html
	std::random_device rdev;
	std::mt19937 mt(rdev());
	std::uniform_int_distribution<> rand(0, 61);

	std::string str;
	for (int i = 0; i < 16; i++) {
		char c = rand(mt);
		if (c < 10) {
			c += '0';
		} else if (c < 10 + 26) {
			c += 'A' - 10;
		} else {
			c += 'a' - 36;
		}
		str += c;
	}
	return str;
}

/*static*/ std::string
OAuth::Base64Encode(const std::vector<uint8>& src)
{
	static const char enc[] =
		"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
		"abcdefghijklmnopqrstuvwxyz"
		"0123456789+/";
	std::vector<uint8> tmp;
	std::string base64;
	int i;

	for (i = 0; src.size() - i >= 3; ) {
		// 0000'0011  1111'2222  2233'3333
		uint8 a0 = src[i++];
		uint8 a1 = src[i++];
		uint8 a2 = src[i++];

		tmp.push_back(a0 >> 2);
		tmp.push_back(((a0 & 0x03) << 4) | (a1 >> 4));
		tmp.push_back(((a1 & 0x0f) << 2) | (a2 >> 6));
		tmp.push_back(a2 & 0x3f);
	}

	// 残りは 0,1,2バイト
	if (src.size() - i == 1) {
		uint8 a0 = src[i++];

		tmp.push_back(a0 >> 2);
		tmp.push_back((a0 & 0x03) << 4);
	} else if (src.size() - i == 2) {
		uint8 a0 = src[i++];
		uint8 a1 = src[i++];

		tmp.push_back(a0 >> 2);
		tmp.push_back(((a0 & 0x03) << 4) | (a1 >> 4));
		tmp.push_back(((a1 & 0x0f) << 2));
	}

	for (const auto& c : tmp) {
		base64 += enc[c];
	}
	// 4文字になるようパディング
	while (base64.size() % 4 != 0) {
		base64 += '=';
	}
	return base64;
}

// HMAC-SHA1 したバイナリを返す。
/*static*/ std::vector<uint8>
OAuth::HMAC_SHA1(const std::string& key, const std::string& msg)
{
	mbedtls_md_context_t ctx;
	std::vector<uint8> result(20);	// SHA-1 は 20バイト (決め打ち…)

	mbedtls_md_init(&ctx);
	mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA1), 1);
	mbedtls_md_hmac_starts(&ctx, (const uint8 *)key.c_str(), key.size());
	mbedtls_md_hmac_update(&ctx, (const uint8 *)msg.c_str(), msg.size());
	mbedtls_md_hmac_finish(&ctx, result.data());
	mbedtls_md_free(&ctx);

	return result;
}

// HMAC-SHA1 してバイナリを Base64 した文字列を返す。
/*static*/ std::string
OAuth::HMAC_SHA1_Base64(const std::string& key, const std::string& msg)
{
	std::vector<uint8> bin = HMAC_SHA1(key, msg);
	return Base64Encode(bin);
}

// パラメータを作ってアクセス URI を返す。
std::string
OAuth::CreateParams(const std::string& method, const std::string& uri)
{
	// 1. 署名キーを作成
	auto key = ConsumerSecret + "&" + AccessSecret;

	// 2. Signature Base String (署名対象文字列) を作成。
	// これは HTTP メソッド、URL、(oauth_signature 以外のすべてのクエリ)
	// を & でつないだもの。

	// Params は oauth_signature 以外のすべての (つまり署名対象の) クエリ
	StringDictionary Params;
	auto nonce = GetNonce();
	auto unixtime = string_format("%ju", (uintmax_t)GetUnixTime());
	Params["oauth_version"] = "1.0";
	Params["oauth_signature_method"] = "HMAC-SHA1";
	Params["oauth_nonce"] = nonce;
	Params["oauth_timestamp"] = unixtime;
	Params["oauth_consumer_key"] = ConsumerKey;
	if (!AccessToken.empty()) {
		Params["oauth_token"] = AccessToken;
	}
	// ここまでが OAuth ヘッダに書き出すべきパラメータなので、
	// この時点でコピーをとる。
	OAuthParams = Params;

	// 追加パラメータは署名対象だが OAuth ヘッダには含まない
	for (const auto& kv : AdditionalParams) {
		const auto& [ k, v ] = kv;
		Params[k] = v;
	}

	auto encoded_params = UrlEncode(MakeQuery(Params));
	auto encoded_uri = UrlEncode(uri);
	auto sig_base_string = method + "&" + encoded_uri + "&" + encoded_params;

	// 3. 署名
	auto signature = HMAC_SHA1_Base64(key, sig_base_string);
	OAuthParams["oauth_signature"] = signature;

	// アクセス URI を返す
	StringDictionary p;
	if (UseOAuthHeader) {
		p = AdditionalParams;
	} else {
		// XXX ここは Params + oauth_signature だと思うけどどうしたもんか
		Params["oauth_signature"] = signature;
		p = Params;
	}
	if (p.empty()) {
		return uri;
	} else {
		auto query = MakeQuery(p);
		return uri + "?" + query;
	}
}

// paramdict を "key1=value1&key2=value2&..." 形式にエンコードします。
/*static*/ std::string
OAuth::MakeQuery(const StringDictionary& paramdict)
{
	std::string sb;
	bool first = true;

	for (const auto& kv : paramdict) {
		const auto& key = kv.first;
		const auto& val = kv.second;

		if (first) {
			first = false;
		} else {
			sb += '&';
		}
		sb += key;
		sb += '=';
		sb += UrlEncode(val);
	}
	return sb;
}

// "key1=value1&key2=value2&..." 形式の s をパースして dict に代入する
/*static*/ void
OAuth::ParseQuery(StringDictionary& dict, const std::string& s)
{
	auto keyvalues = Split(s, "&");
	for (const auto& kv : keyvalues) {
		const auto [key, val] = Split2(kv, "=");
		dict[key] = val;
	}
}

// OAuthParams から OAuth ヘッダを作成する。
// OAuthParams には authorization: OAuth ヘッダに載せるすべての
// パラメータを代入しておくこと。
std::string
OAuth::MakeOAuthHeader()
{
	std::string sb;
	bool first = true;

	sb = "Authorization: OAuth ";
	for (auto [key, val] : OAuthParams) {
		if (first) {
			first = false;
		} else {
			sb += ',';
		}
		sb += string_format("%s=\"%s\"", key.c_str(), UrlEncode(val).c_str());
	}
	return sb;
}

// method と url から IHttpClient を生成して返す。
std::unique_ptr<HttpClient>
OAuth::CreateHttp(const std::string& method, const std::string& uri)
{
	auto conn_uri = CreateParams(method, uri);

	std::unique_ptr<HttpClient> client(new HttpClient());
	if (client->Init(diag, conn_uri) == false) {
		// XXX エラーは?
	}
	if (UseOAuthHeader) {
		client->AddHeader(MakeOAuthHeader());
	}
	return client;
}

// uri_request_token に接続しトークンを取得する。
void
OAuth::RequestToken(const std::string& uri_request_token)
{
	auto client = CreateHttp("GET", uri_request_token);

	StringDictionary resultDict;
	auto stream = client->GET();
	// TODO: Content-Encoding とかに応じた処理
	for (;;) {
		std::string buf;
		if (stream->ReadLine(&buf) <= 0)
			break;
		if (buf.empty())
			break;
		ParseQuery(resultDict, buf);
	}

	AccessToken  = resultDict["oauth_token"];
	AccessSecret = resultDict["oauth_token_secret"];
}

// uri_api に method (GET/POST) で接続する。
InputStream *
OAuth::RequestAPI(const std::string& method, const std::string& uri_api)
{
	Trace(diag, "CreateHttp call");
	RequestAPIClient = CreateHttp(method, uri_api);
	Trace(diag, "CreateHttp return");

	// Ciphers 指定があれば指示
	if (!Ciphers.empty()) {
		RequestAPIClient->SetCiphers(Ciphers);
	}

	Trace(diag, "client.%s call", method.c_str());
	auto stream = RequestAPIClient->Act(method);
	Trace(diag, "client.%s return", method.c_str());

	return stream;
}
