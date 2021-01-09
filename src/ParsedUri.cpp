#include "ParsedUri.h"
#include "StringUtil.h"

/*static*/ ParsedUri
ParsedUri::Parse(const std::string& uriString)
{
	ParsedUri rv;

	// スキームとそれ以降を分離
	auto a = Split2FirstOption(uriString, "://");
	rv.Scheme = a.first;
	auto apqf = a.second;

	// オーソリティとそれ以降(PathQueryFragment)を分離
	auto b = Split2(apqf, "/");
	auto authority = b.first;
	auto pqf = b.second;

	// オーソリティからユーザ情報とホストポートを分離
	auto c = Split2(authority, "@");
	// @ がないときはホストポートとみなす
	std::string userinfo;
	std::string hostport;
	if (c.second.empty()) {
		hostport = c.first;
	} else {
		userinfo = c.first;
		hostport = c.second;
	}

	// ユーザ情報をユーザ名とパスワードに分離
	auto d = Split2(userinfo, ":");
	rv.User = d.first;
	rv.Password = d.second;

	// ホストポートをホストとポートに分離
	if (hostport[0] == '[') {
		// IPv6 アドレス
		// XXX 色々手抜き
		auto e = Split2(hostport, "]");
		rv.Host = e.first.substr(1);
		auto p = Split2(e.second, ":");
		rv.Port = p.second;
	} else {
		auto e = Split2(hostport, ":");
		rv.Host = e.first;
		rv.Port = e.second;
	}

	// PathQueryFragment をパスと QF に分離
	auto f = Split2(pqf, "?");
	rv.Path = "/" + f.first;		// URI 定義では Path は / を含む
	auto qf = f.second;

	// QueryFragment をクエリとフラグメントに分離
	auto g = Split2(qf, "#");
	rv.Query = g.first;
	rv.Fragment = g.second;

	return rv;
}

std::string
ParsedUri::SchemeAuthority() const
{
	std::string sb;

	sb += Scheme;
	sb += "://";
	if (!User.empty()) {
		sb += User;
		if (!Password.empty()) {
			sb += ":";
			sb += Password;
		}
		sb += "@";
	}
	if (!Host.empty()) {
		if (Host.find(':') || Host.find('%')) {
			// IPv6
			sb += "[";
			sb += Host;
			sb += "]";
		} else {
			sb += Host;
		}
		if (!Port.empty()) {
			sb += ":";
			sb += Port;
		}
	}
	return sb;
}

// Path?Query#Fragment を返す
std::string
ParsedUri::PQF() const
{
	std::string sb;

	sb += Path;
	if (!Query.empty()) {
		sb += "?";
		sb += Query;
	}
	if (!Fragment.empty()) {
		sb += "#";
		sb += Fragment;
	}
	return sb;
}

// デバッグ用文字列を返す
std::string
ParsedUri::to_debug_string() const
{
	std::string sb;

	sb += "Scheme=|"    + Scheme   + "|";
	sb += ",Host=|"     + Host     + "|";
	sb += ",Port=|"     + Port     + "|";
	sb += ",User=|"     + User     + "|";
	sb += ",Password=|" + Password + "|";
	sb += ",Path=|"     + Path     + "|";
	sb += ",Query=|"    + Query    + "|";
	sb += ",Fragment=|" + Fragment + "|";

	return sb;
}

#if defined(SELFTEST)
#include "test.h"
int
test_ParsedUri()
{
	printf("%s\n", __func__);

	std::vector<std::array<std::string, 5>> table = {
		// input		scheme	host	port	pqf
		{ "a://b",		"a",	"b",	"",		"/" },
		{ "a://b/",		"a",	"b",	"",		"/" },
		{ "a://b:c",	"a",	"b",	"c",	"/" },
		{ "a://b:c/d",	"a",	"b",	"c",	"/d" },
		{ "/d",			"",		"",		"",		"/d" },
		{ "b:c",		"",		"b",	"c",	"/" },
		{ "b:c/d/e",	"",		"b",	"c",	"/d/e" },
	};

	for (const auto& a : table) {
		auto input      = a[0];
		auto exp_scheme = a[1];
		auto exp_host   = a[2];
		auto exp_port   = a[3];
		auto exp_pqf    = a[4];

		auto uri = ParsedUri::Parse(input);
		xp_eq(exp_scheme, uri.Scheme, input);
		xp_eq(exp_host, uri.Host, input);
		xp_eq(exp_port, uri.Port, input);
		xp_eq(exp_pqf, uri.PQF(), input);
	}
	return 0;
}
#endif // SELFTEST
