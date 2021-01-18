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
