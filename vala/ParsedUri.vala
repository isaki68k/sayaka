/*
 * Copyright (C) 2015 Y.Sugahara (moveccr)
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

using StringUtil;

namespace ULib
{
	/// <summary>
	/// URI パーサ。
	/// UriParser とかは名前被る。ちょっと検討が必要。
	/// </summary>
	public class ParsedUri
	{
		public string Scheme;
		public string Host;
		public string Port;
		public string User;
		public string Password;
		public string Path;
		public string Query;
		public string Fragment;

		// Path?Query#Fragment を返します。
		public string PQF()
		{
			var sb = new StringBuilder();
			sb.append(Path);
			if (Query != "") {
				sb.append("?");
				sb.append(Query);
			}
			if (Fragment != "") {
				sb.append("#");
				sb.append(Fragment);
			}
			return sb.str;
		}

		// Scheme://AUTHORITY を返します。
		public string SchemeAuthority()
		{
			var sb = new StringBuilder();
			sb.append(Scheme);
			sb.append("://");
			if (User != "") {
				sb.append(User);
				if (Password != "") {
					sb.append(":");
					sb.append(Password);
				}
				sb.append("@");
			}
			if (Host != "") {
				if (Host.contains(":") || Host.contains("%")) {
					// IPv6
					sb.append("[");
					sb.append(Host);
					sb.append("]");
				} else {
					sb.append(Host);
				}
				if (Port != "") {
					sb.append(":");
					sb.append(Port);
				}
			}
			return sb.str;
		}

		// URI 文字列を生成します。
		public string to_string()
		{
			return SchemeAuthority() + PQF();
		}

		// デバッグ用文字列を返します。
		public string to_debug_string()
		{
			var sb = new StringBuilder();
			sb.append(@"Scheme=|$(Scheme)|");
			sb.append(@",Host=|$(Host)|");
			sb.append(@",Port=|$(Port)|");
			sb.append(@",User=|$(User)|");
			sb.append(@",Password=|$(Password)|");
			sb.append(@",Path=|$(Path)|");
			sb.append(@",Query=|$(Query)|");
			sb.append(@",Fragment=|$(Fragment)|");
			return sb.str;
		}

		// 空の ParsedUri オブジェクトを生成します。
		public ParsedUri()
		{
			Scheme = "";
			Host = "";
			Port = "";
			User = "";
			Password = "";
			Path = "";
			Query = "";
			Fragment = "";
		}

		/// <summary>
		/// URI 文字列を要素にパースし、ParsedUri のインスタンスを返します。
		/// </summary>
		/// <param name="uriString"></param>
		/// <returns></returns>
		public static ParsedUri Parse(string uriString)
		{
			var rv = new ParsedUri();

			// スキームとそれ以降を分離
			var a = Split2FirstOption(uriString, "://");
			rv.Scheme = a[0];
			var APQF = a[1];

			// オーソリティとそれ以降(PathQueryFragment)を分離
			var b = Split2(APQF, "/");
			var authority = b[0];
			var PQF = b[1];

			// オーソリティからユーザ情報とホストポートを分離
			var c = Split2(authority, "@");
			// @ がないときはホストポートとみなす
			var userinfo = c[0];
			var hostport = c[1];
			if (hostport == "") {
				userinfo = "";
				hostport = c[0];
			}

			// ユーザ情報をユーザ名とパスワードに分離
			var d = Split2(userinfo, ":");
			rv.User = d[0];
			rv.Password = d[1];

			// ホストポートをホストとポートに分離
			if (hostport[0] == '[') {
				// IPv6 アドレス
				// XXX 色々手抜き
				var e = Split2(hostport, "]");
				rv.Host = e[0].substring(1);
				var p = Split2(e[1], ":");
				rv.Port = p[1];
			} else {
				var e = Split2(hostport, ":");
				rv.Host = e[0];
				rv.Port = e[1];
			}

			// PathQueryFragmentをパスとQFに分離
			var f = Split2(PQF, "?");
			rv.Path = "/" + f[0];			// URI 定義では Path は / を含む。
			var QF = f[1];

			// QueryFragment をクエリとフラグメントに分離
			var g = Split2(QF, "#");
			rv.Query = g[0];
			rv.Fragment = g[1];

			return rv;
		}
	}
}

#if SELF_TEST
// valac -X -w -D SELF_TEST ParsedUri.vala StringUtil.vala
class program
{
	public static int tests;
	public static int failed;
	public static int testnum;

	private static void xp_str(string exp, string value, string msg)
	{
		tests++;
		if (value != exp) {
			stdout.printf(@"Testcase $(testnum): $(msg) expects "
				+ @"\"$(exp)\" but \"$(value)\"\n");
			failed++;
		}
	}

	public static int main(string[] args)
	{
		string[] table = new string[] {
			// input	scheme, host, port, PQF
			"a://b",	"a", "b", "", "/",
			"a://b/",	"a", "b", "", "/",
			"a://b:c",	"a", "b", "c", "/",
			"a://b:c/d","a", "b", "c", "/d",
			"/d",		"",  "",  "",  "/d",
			"b:c",		"",  "b", "c", "/",
			"b:c/d/e",	"",  "b", "c", "/d/e",
		};

		testnum = 0;
		tests = 0;
		failed = 0;
		for (var i = 0; i < table.length; i += 5) {
			var input = table[i];
			var exp_scheme = table[i + 1];
			var exp_host = table[i + 2];
			var exp_port = table[i + 3];
			var exp_PQF = table[i + 4];

			var uri = ULib.ParsedUri.Parse(input);
			xp_str(exp_scheme, uri.Scheme, "uri.Scheme");
			xp_str(exp_host, uri.Host, "uri.Host");
			xp_str(exp_port, uri.Port, "uri.Port");
			xp_str(exp_PQF, uri.PQF(), "uri.PQF");

			testnum++;
		}

		if (failed == 0) {
			stdout.printf(@"$(tests) tests, ALL passed\n");
		} else {
			stdout.printf(@"$(tests) tests, $(failed) failed\n");
		}

		return 0;
	}
}
#endif
