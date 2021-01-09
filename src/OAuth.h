#pragma once

#include "Diag.h"
#include "Dictionary.h"
#include "HttpClient.h"
#include <string>

class OAuth
{
 public:
	OAuth();
	OAuth(const Diag& diag_);

	void SetDiag(const Diag& diag_);

	// Ciphers を設定する
	void SetCiphers(std::string ciphers_)
	{
		Ciphers = ciphers_;
	}

	// Nonce のための文字列を返す。
	// (呼び出すたびに異なる文字列を生成する)
	static std::string GetNonce();

	// Base64 した文字列を返す
	static std::string Base64Encode(const std::vector<uint8>& src);

	// HMAC-SHA1 したバイナリを返す。
	static std::vector<uint8> HMAC_SHA1(const std::string& key,
		const std::string& msg);

	// HMAC-SHA1 してバイナリを Base64 した文字列を返す。
	static std::string HMAC_SHA1_Base64(const std::string& key,
		const std::string& msg);

	// Unix 時刻を返す
	static time_t GetUnixTime();

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

	// uri_api に method (GET/POST) で接続する。
	InputStream *RequestAPI(const std::string& method,
		const std::string& uri_api);

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
	std::string AccessToken;
	std::string AccessSecret;

 private:
	// method と url から IHttpClient を生成して返す。
	// UseOAuthHeader が true なら OAuth 認証ヘッダも用意する。
	// 接続はまだ行わない。
	HttpClient CreateHttp(const std::string& method, const std::string& uri);

	Diag diag;

	// HTTP クライアント
	// (ローカル変数に出来そうに見えるが、HTTP コネクション張ってる間
	// ずっと生存している必要があるので、メンバ変数でなければならない)
	HttpClient RequestAPIClient {};

	// TLS で使用する cipher list。"" ならデフォルト。
	std::string Ciphers {};
};

#if defined(SELFTEST)
extern void test_OAuth();
#endif
