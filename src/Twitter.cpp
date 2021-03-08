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

#include "Twitter.h"
#include "FileUtil.h"

//
// TwitterToken
//

// ファイルから token, secret を読み込む。
bool
TwitterToken::LoadFromFile(const std::string& filename)
{
	auto text = FileReadAllText(filename);
	if (text.empty()) {
		return false;
	}

	auto json = Json::parse(text);
	Token  = json["token"];
	Secret = json["secret"];
	return true;
}

// token, secret をファイルに書き出す。
bool
TwitterToken::SaveToFile(const std::string& filename)
{
	Json json;

	json["token"]  = Token;
	json["secret"] = Secret;

	return FileWriteAllText(filename, json.dump());
}

//
// Twitter
//

/*static*/ const char Twitter::accessTokenURL[] =
	"https://api.twitter.com/oauth/access_token";
/*static*/ const char Twitter::authorizeURL[] =
	"https://twitter.com/oauth/authorize";
/*static*/ const char Twitter::requestTokenURL[] =
	"https://api.twitter.com/oauth/request_token";

/*static*/ const char Twitter::APIRoot[] =
	"https://api.twitter.com/1.1/";
/*static*/ const char Twitter::StreamAPIRoot[] =
	"https://stream.twitter.com/1.1/";

/*static*/ const char Twitter::ConsumerKey[] =
	"jPY9PU5lvwb6s9mqx3KjRA";
/*static*/ const char Twitter::ConsumerSecret[] =
	"faGcW9MMmU0O6qTrsHgcUchAiqxDcU9UjDW2Zw";

// コンストラクタ
Twitter::Twitter()
{
	oauth.ConsumerKey = Twitter::ConsumerKey;
	oauth.ConsumerSecret = Twitter::ConsumerSecret;
}

// コンストラクタ
Twitter::Twitter(const Diag& diag_)
	: Twitter()
{
	SetDiag(diag_);
}

void
Twitter::SetDiag(const Diag& diag_)
{
	diag = diag_;
	oauth.SetDiag(diag_);
}

// Ciphers を設定する。
void
Twitter::SetCiphers(const std::string& ciphers)
{
	// 実際には OAuth に指示するだけ。
	oauth.SetCiphers(ciphers);
}

// Access Token を取得するところまで
void
Twitter::GetAccessToken()
{
	oauth.AdditionalParams.clear();

	Debug(diag, "----- Request Token -----");
	oauth.RequestToken(requestTokenURL);

	printf("Please go to:\n"
		"%s?oauth_token=%s\n", authorizeURL, oauth.AccessToken.c_str());
	printf("\n");
	printf("And input PIN code: ");
	fflush(stdout);

	char pin_str[1024];
	fgets(pin_str, sizeof(pin_str), stdin);

	Debug(diag, "----- Access Token -----");

	oauth.AdditionalParams["oauth_verifier"] = pin_str;
	oauth.RequestToken(accessTokenURL);

	AccessToken.Token  = oauth.AccessToken;
	AccessToken.Secret = oauth.AccessSecret;
}

InputStream *
Twitter::GetAPI(const std::string& apiRoot, const std::string& api,
	const StringDictionary& options)
{
	return API("GET", apiRoot, api, options);
}

InputStream *
Twitter::PostAPI(const std::string& apiRoot, const std::string& api,
	const StringDictionary& options)
{
	return API("POST", apiRoot, api, options);
}

InputStream *
Twitter::API(const std::string& method, const std::string& apiRoot,
	const std::string& api, const StringDictionary& options)
{
	oauth.AccessToken  = AccessToken.Token;
	oauth.AccessSecret = AccessToken.Secret;

	oauth.AdditionalParams.clear();

	if (!options.empty()) {
		for (const auto& [key, val] : options) {
			oauth.AdditionalParams[key] = val;
		}
	}

	Trace(diag, "RequestAPI call");
	auto stream = oauth.RequestAPI(method, apiRoot + api + ".json");
	Trace(diag, "RequestAPI return");

	return stream;
}

// API に接続し、結果の JSON を返す。
// 接続が失敗、あるいは JSON が正しく受け取れなかった場合は {} を返す。
Json
Twitter::API2Json(const std::string& method, const std::string& apiRoot,
	const std::string& api, const StringDictionary& options)
{
	InputStream *stream = NULL;
	std::string line;
	Json json;

	stream = API(method, apiRoot, api, options);
	if (stream == NULL) {
		Debug(diag, "%s: API failed", api.c_str());
		return json;
	}
	auto r = stream->ReadLine(&line);
	if (__predict_false(r < 0)) {
		Debug(diag, "%s: ReadLine failed: %s", api.c_str(), strerror(errno));
		return json;
	}
	Debug(diag, "ReadLine |%s|", line.c_str());

	if (line.empty()) {
		return json;
	}

	return Json::parse(line);
}
