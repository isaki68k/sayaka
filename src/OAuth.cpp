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
#if defined(SELFTEST)
	// テスト用に固定値を返す
	return "testnonce";
#else
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
#endif
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

// UNIX 時刻を返す。
/*static*/ time_t
OAuth::GetUnixTime()
{
#if defined(SELFTEST)
	// テスト用に固定値を返す
	return 123456789;
#else
	return time(NULL);
#endif
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


#if defined(SELFTEST)
#include "test.h"
#include <string.h>

// テストの表記を簡単にするため
static inline std::vector<uint8> operator"" _hex2vec(const char *str,
	std::size_t len)
{
	std::vector<uint8> v;

	// XXX とりあえず
	if (len % 2 != 0)
		return v;

	for (; *str; str += 2) {
		char buf[3];
		buf[0] = str[0];
		buf[1] = str[1];
		buf[2] = '\0';
		uint8 x = stox32def(buf, 0);
		v.emplace_back(x);
	}
	return v;
}

static inline std::vector<uint8> operator"" _str2vec(const char *str,
	std::size_t len)
{
	std::vector<uint8> v;
	for (; *str; str++) {
		v.emplace_back(*str);
	}
	return v;
}

static inline std::string operator"" _hex2str(const char *str, std::size_t len)
{
	std::string v;

	// XXX とりあえず
	if (len % 2 != 0)
		return v;

	for (; *str; str += 2) {
		char buf[3];
		buf[0] = str[0];
		buf[1] = str[1];
		buf[2] = '\0';
		v += stox32def(buf, 0);
	}
	return v;
}

void
test_Base64Encode()
{
	printf("%s\n", __func__);

	std::vector<std::array<std::string, 2>> table = {
		{ "ABCDEFG",				"QUJDREVGRw==" },
		// From RFC3548
		{ "14fb9c03d97e"_hex2str,	"FPucA9l+" },
		{ "14fb9c03d9"_hex2str,		"FPucA9k=" },
		{ "14fb9c03"_hex2str,		"FPucAw==" },
	};
	for (auto& a : table) {
		const std::string& src = a[0];
		const std::string& exp = a[1];

		std::vector<uint8> input(src.begin(), src.end());
		auto actual = OAuth::Base64Encode(input);
		xp_eq(exp, actual, src);
	}
}

void
test_HMAC_SHA1()
{
	printf("%s\n", __func__);

	struct testentry {
		int testcase;
		std::vector<uint8> key;
		std::vector<uint8> data;
		std::vector<uint8> digest;
	};
	std::vector<testentry> table = {
		// RFC2202
		{ 1,
			"0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b"_hex2vec,
			"Hi There"_str2vec,
			"b617318655057264e28bc0b6fb378c8ef146be00"_hex2vec,
		},
		{ 2,
			"Jefe"_str2vec,
			"what do ya want for nothing?"_str2vec,
			"effcdf6ae5eb2fa2d27416d5f184df9c259a7c79"_hex2vec,
		},
		{ 3,
			"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"_hex2vec,
			std::vector<uint8>(50, 0xdd),
			"125d7342b9ac11cd91a39af48aa17b4f63f175d3"_hex2vec,
		},
		{ 4,
			"0102030405060708090a0b0c0d0e0f10111213141516171819"_hex2vec,
			std::vector<uint8>(50, 0xcd),
			"4c9007f4026250c6bc8414f9bf50c86c2d7235da"_hex2vec,
		},
	};
	for (const auto& a : table) {
		std::string key;
		std::string msg;
		std::string digest;

		for (const auto& c : a.key) {
			key += (char)c;
		}
		for (const auto& c : a.data) {
			msg += (char)c;
		}
		// 比較用に(もう一回)文字列に変換
		for (const auto& c : a.digest) {
			digest += string_format("%02x", c);
		}

		std::vector<uint8> bin = OAuth::HMAC_SHA1(key, msg);
		// 比較用に文字列に変換
		std::string actual;
		for (const auto& c : bin) {
			actual += string_format("%02x", c);
		}

		xp_eq(digest, actual, string_format("testcase%d", a.testcase));
	}
}

void
test_CreateParams()
{
	Diag diag;

	printf("%s\n", __func__);

	{
		// 1. AdditionalParams なしの場合
		OAuth oauth(diag);

		auto actual = oauth.CreateParams("GET", "http://example.com/test/");
		auto expected = "http://example.com/test/";
		xp_eq(expected, actual);
	}
	{
		// 2. AdditionalParams ありの場合
		StringDictionary dict;
		dict["key1"] = "val1";
		dict["key2"] = "val2";

		OAuth oauth(diag);
		oauth.AdditionalParams = dict;

		auto actual = oauth.CreateParams("GET", "http://example.com/test/");
		auto expected = "http://example.com/test/?key1=val1&key2=val2";
		xp_eq(expected, actual);
	}
	{
		// 3. UseOAuthHeader == false の場合
		// 元コードを適当に書き換えて実行させてみたもの。
		OAuth oauth(diag);
		oauth.UseOAuthHeader = false;

		oauth.ConsumerKey = "consumerkey";
		oauth.ConsumerSecret = "consumersecret";
		oauth.AccessToken = "accesstoken";
		oauth.AccessSecret = "accesssecret";
		StringDictionary dict;
		dict["cursor"] = "-1";
		oauth.AdditionalParams = dict; 

		std::string url = "https://api.twitter.com/1.1/blocks/ids.json";
		auto actual = oauth.CreateParams("GET", url);
		auto expected = url + "?"
			"cursor=-1&"
			"oauth_consumer_key=consumerkey&"
			"oauth_nonce=testnonce&"
			"oauth_signature=Vlinu3NEGfcaO50JgPIQj1LGgQw%3D&"
			"oauth_signature_method=HMAC-SHA1&"
			"oauth_timestamp=123456789&"
			"oauth_token=accesstoken&"
			"oauth_version=1.0";
		xp_eq(expected, actual);
	}
}

void
test_MakeQuery()
{
	printf("%s\n", __func__);

	// 1テストエントリは key1, value1, key2, value2, ..., expected
	// という奇数個の文字列配列。
	std::vector<std::vector<std::string>> table = {
		{
			""			// 空 Dictionary なら空文字列
		},
		{
			"a", "b",
			"a=b",
		},
		{
			"a", "b", "cc", "dd", "e", "f",
			"a=b&cc=dd&e=f",
		},
		{
			"a!", "#b",	// キーには通常記号は来ないから?
			"a!=%23b",
		},
	};
	for (auto& a : table) {
		// 末尾が期待値
		const auto& exp = a.back();
		a.pop_back();
		// 末尾を取り除いたら key, value の組になっているはず
		StringDictionary dict;
		for (int i = 0; i < a.size();) {
			const auto& key = a[i++];
			const auto& val = a[i++];
			dict[key] = val;
		}

		// MakeQuery
		auto actual = OAuth::MakeQuery(dict);
		xp_eq(exp, actual, exp);
	}
}

void
test_ParseQuery()
{
	printf("%s\n", __func__);

	std::vector<std::vector<std::string>> table = {
		{
			""			// 空文字列なら空 Dictionary
		},
		{
			"a=b",
			"a", "b",
		},
		{
			"a=b&cc=dd&e=f",
			"a", "b", "cc", "dd", "e", "f",
		},
		{
			// 分解するだけで URL エンコードのデコードは行わない
			"a%21=%23b",
			"a%21", "%23b",
		},
	};
	for (auto& a : table) {
		// 先頭が入力文字列
		const auto src = a.front();
		a.erase(a.begin());
		// 残りが key, value の組
		StringDictionary exp;
		for (int i = 0; i < a.size();) {
			const auto& key = a[i++];
			const auto& val = a[i++];
			exp[key] = val;
		}

		// ParseQuery
		StringDictionary parsed;
		OAuth::ParseQuery(parsed, src);
		// 照合
		if (exp.size() == parsed.size()) {
			for (const auto& [key, val] : exp) {
				xp_eq(val, parsed[key], src);
			}
		} else {
			xp_eq(exp.size(), parsed.size(), src);
		}
	}

	{
		// ParseQuery(dict) は dict へ追記。
		StringDictionary dict;
		dict["a"] = "b";
		OAuth::ParseQuery(dict, "c=d");
		// 個数だけでいいか
		xp_eq(2, dict.size());
	}
}

void
test_MakeOAuthHeader()
{
	printf("%s\n", __func__);

	std::vector<std::vector<std::string>> table = {
		// expected		OAuthParams...
		{ "Authorization: OAuth ", },	// 空 Dictionary
		{ "Authorization: OAuth a=\"b%21\"",		"a", "b!" },
		{ "Authorization: OAuth a=\"b\",c=\"d\"",	"a", "b", "c", "d" },
	};
	Diag diag;
	for (auto a : table) {
		auto exp = a[0];
		a.erase(a.begin());
		// 残りが OAuthParams の key, value, ...
		StringDictionary dict;
		for (int i = 0; i < a.size(); ) {
			const auto& key = a[i++];
			const auto& val = a[i++];
			dict[key] = val;
		}

		OAuth oauth(diag);
		oauth.OAuthParams = dict;
		auto actual = oauth.MakeOAuthHeader();
		xp_eq(exp, actual, exp);
	}
}

void
test_OAuth()
{
	test_Base64Encode();
	test_HMAC_SHA1();
	test_CreateParams();
	test_MakeQuery();
	test_ParseQuery();
	test_MakeOAuthHeader();
}
#endif
