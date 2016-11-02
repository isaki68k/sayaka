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

	// 文字列 s を、最初に現れた文字列 c で2分割します。
	// 返される各文字列には c は含まれません。
	// c が見つからないときは、rv[0] に ""、rv[1] に s を返します。
	public static string[] Split2FirstOption(string s, string c)
	{
		string[] rv = new string[2];
		int p = s.index_of(c);
		if (p < 0) {
			rv[0] = "";
			rv[1] = s;
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
				sb.append("%%%02X".printf((uint8)c));
			}
		}
		return sb.str;
	}

	public static string Trim(string s)
	{
		return s.strip();
	}
}


