#pragma once

#include <string>

class ParsedUri
{
 public:
	std::string Scheme {};
	std::string Host {};
	std::string Port {};
	std::string User {};
	std::string Password {};
	std::string Path {};
	std::string Query {};
	std::string Fragment {};

	// URI 文字列を要素に分解し、ParsedUri のインスタンスを返す。
	static ParsedUri Parse(const std::string& uriString);

	// Scheme::AUTHORITY を返す
	std::string SchemeAuthority() const;

	// Path?Query#Fragment を返す
	std::string PQF() const;

	// URI 文字列を返す
	std::string to_string() const {
		return SchemeAuthority() + PQF();
	}

	// デバッグ用文字列を返す
	std::string to_debug_string() const;
};

#if defined(SELFTEST)
extern int test_ParsedUri();
#endif
