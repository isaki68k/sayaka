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

#include "test.h"
#include "OAuth.h"
#include "StringUtil.h"

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

// テスト用に固定値を返す GetNonce()
/*static*/ std::string
OAuth::GetNonce()
{
	return "testnonce";
}

static void
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

static void
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

static void
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
			"oauth_signature=KkjnHlghrW3uPecD8PNcTBQr0SU%3D&"
			"oauth_signature_method=HMAC-SHA1&"
			"oauth_timestamp=1258538052&"
			"oauth_token=accesstoken&"
			"oauth_version=1.0";
		xp_eq(expected, actual);
	}
}

static void
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

static void
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

static void
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
