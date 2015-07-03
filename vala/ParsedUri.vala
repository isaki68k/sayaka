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
				sb.append(Host);
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


		public ParsedUri()
		{
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
			var a = Split2(uriString, "://");
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
			var e = Split2(hostport, ":");
			rv.Host = e[0];
			rv.Port = e[1];

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

		/// <summary>
		/// s を、最初に現れた c で 2分割します。
		/// 返される、各文字列には c は含まれません。
		/// c が見つからないときは、rv[0] に s 、rv[1] に string.Empty を返します。
		/// string.Split とは異なり、必ず要素数 2 の配列を返します。
		/// </summary>
		/// <param name="s"></param>
		/// <param name="c"></param>
		/// <returns></returns>
		/// <example>Split2("a:", ':') と Split2("a", ':') は同じ結果を返します。</example>
		public static string[] Split2(string s, string c)
		{
			string[] rv = new string[2];
			int p = s.index_of(c);
			if (p < 0) {
				rv[0] = s;
				rv[1] = "";
			} else {
				rv[0] = s.substring(0, p);
				rv[1] = s.substring(p + c.length);
			}
			return rv;
		}

	}
}

