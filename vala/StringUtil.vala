namespace StringUtil
{
	// namespace 直下に static 関数は書ける

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

	// 文字列をなんちゃって Url エンコードします。
	public static string UrlEncode(string s)
	{
		var sb = new StringBuilder();
		// %xx に大文字の16進文字が要るとの情報が.
		for (var i = 0; i < s.length; i++) {
			var c = s[i];
			if (('0' <= c && c <= '9')
			 || ('A' <= c && c <= 'Z')
			 || ('a' <= c && c <= 'z')
			 || (c == '-' || c == '_' || c == '.' || c == '~')) {
				sb.append_c(c);
			} else {
				sb.append("%%%02X".printf((int)c));
			}
		}
		return sb.str;
	}

	public static string Trim(string s)
	{
		return s.strip();
	}
}


