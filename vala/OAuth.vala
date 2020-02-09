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

using ULib;
using StringUtil;

public class OAuth
{
	private Diag diag = new Diag("OAuth");

	public string ConsumerKey { get; set; }
	public string ConsumerSecret { get; set; }

	// OAuth ヘッダに書き出すパラメータ
	public Dictionary<string, string> OAuthParams =
		new Dictionary<string, string>();

	// リクエストのパラメータ。
	// URI の Query 句がまだ使えないので。
	public Dictionary<string, string> AdditionalParams =
		new Dictionary<string, string>();

	// OAuth ヘッダモードでは true を設定してください。
	public bool UseOAuthHeader;

	// アクセストークンとアクセスシークレット。
	public string token;
	public string token_secret;

	private Rand rand;

	// HTTP クライアント。
	// ローカル変数に出来そうに見えるが、HTTP コネクション張ってる間
	// ずっと生存してる必要があるのでメンバ変数でなければならない。
	private IHttpClient RequestAPIClient;

	// TLS で使用する cipher list。null ならデフォルト。
	private string Ciphers;

	public OAuth()
	{
		rand = new Rand();
		UseOAuthHeader = true;
		Ciphers = null;
	}

	// Ciphers を設定します。
	public void SetCiphers(string ciphers)
	{
		Ciphers = ciphers;
	}

	// Nonce のための文字列を取得します。
	// 呼び出すたびに異なる文字列が生成されます。
	public string GetNonce()
	{
		var sb = new StringBuilder.sized(16);
		for (int i = 0; i < 16; i++) {
			// twitter のドキュメントには alphanumeric って書いてある
			// 0x30-39, 41-5a, 61-7a  個数 = 10+26+26 = 62
			// 0 .. 61 の乱数を求める
			var c = (char)(rand.int_range(0, 62));
			if (c < 10) {
				c += '0';
			} else if (c < 10 + 26) {
				c += 'A' - 10;
			} else {
				c += 'a' - 36;
			}
			sb.append_c(c);
		}
		return sb.str;
	}

	// HMAC-SHA1 してバイナリを Base64 した文字列を返します。
	public string HMAC_SHA1_Base64(string key, string message)
	{
		// HMAC SHA1 して Base64 
		var hm = new Hmac(ChecksumType.SHA1, key.data);
		hm.update(message.data);

		// バイナリのダイジェストを取り出す
		size_t len = 64;
		uint8[] digest = new uint8[len];
		hm.get_digest(digest, ref len);
		assert(len <= digest.length);

		// バイナリを Base64 した文字列を返す
		digest.resize((int)len);
		var rv = Base64.encode(digest);
		return rv;
	}

	// パラメータを作ってアクセス URI を返す
	public string CreateParams(string method, string uri)
	{
		// 1. 署名キーを作成
		var key = ConsumerSecret + "&" + (token_secret ?? "");

		// 2. Signature Base String (署名対象文字列) を作成
		// これは、HTTPメソッド、URL、(oauth_signature以外のすべてのクエリ)
		// を & でつないだもの。

		// Params は oauth_signature 以外のすべての (つまり署名対象の) クエリ
		var Params = new Dictionary<string, string>();
		var nonce = GetNonce();
		var unixtime = new DateTime.now_utc().to_unix();
		Params["oauth_version"] = "1.0";
		Params["oauth_signature_method"] = "HMAC-SHA1";
		Params["oauth_nonce"] = nonce;
		Params["oauth_timestamp"] = unixtime.to_string();
		Params["oauth_consumer_key"] = ConsumerKey;
		Params["oauth_version"] = "1.0";
		if (token != null) {
			Params["oauth_token"] = token;
		}
		// ここまでが OAuth ヘッダに書き出すべきパラメータなので、
		// この時点でコピーをとる
		foreach (KeyValuePair<string, string> kv in Params) {
			OAuthParams[kv.Key] = kv.Value;
		}

		// 追加パラメータは署名対象だが OAuth ヘッダには含まない
		foreach (KeyValuePair<string, string> kv in AdditionalParams) {
			Params[kv.Key] = kv.Value;
		}
		var encoded_params = UrlEncode(MakeQuery(Params));

		var encoded_uri = UrlEncode(uri);
		var sig_base_string = @"$(method)&$(encoded_uri)&$(encoded_params)";

		// 3. 署名
		var signature = HMAC_SHA1_Base64(key, sig_base_string);
		OAuthParams["oauth_signature"] = signature;

		// アクセス URI を返す
		Dictionary<string, string> p;
		if (UseOAuthHeader) {
			p = AdditionalParams;
		} else {
			// XXX ここは Params + oauth_signature だと思うけどどうしたもんか
			Params["oauth_signature"] = signature;
			p = Params;
		}
		if (p.Count == 0) {
			return uri;
		} else {
			var query = MakeQuery(p);
			return @"$(uri)?$(query)";
		}
	}
	
	// paramdict を "key1=value1&key2=value2&..." 形式にエンコードします。
	public static string MakeQuery(Dictionary<string, string> paramdict)
	{
		var sb = new StringBuilder();
		bool f_first = true;
		foreach (KeyValuePair<string, string> p in paramdict) {
			if (f_first == false) {
				sb.append_c('&');
			} else {
				f_first = false;
			}
			sb.append("%s=%s".printf(p.Key, UrlEncode(p.Value)));
		}
		return sb.str;
	}

	// "key1=val1&key2=val2&..." 形式の s をパースして dict に代入します。
	public static void ParseQuery(Dictionary<string, string> dict, string s)
	{
		var keyvalues = s.split("&");
		for (int i = 0; i < keyvalues.length; i++) {
			var kv = StringUtil.Split2(keyvalues[i], "=");
			dict[kv[0]] = kv[1];
		}
	}

	// OAuthParams から OAuth ヘッダを作成します。
	// OAuthParams には authorization: OAuth ヘッダに載せるすべての
	// パラメータを代入しておいてください。
	public string MakeOAuthHeader()
	{
		var sb = new StringBuilder();
		bool f_first = true;
		sb.append("Authorization: OAuth ");
		foreach (KeyValuePair<string, string> p in OAuthParams) {
			if (f_first == false) {
				sb.append_c(',');
			} else {
				f_first = false;
			}
			sb.append("%s=\"%s\"".printf(p.Key, UrlEncode(p.Value)));
		}
		return sb.str;
	}

	// method と url から IHttpClient を生成して返します。
	// UseOAuthHeader が true なら OAuth 認証ヘッダも用意します。
	// 接続はまだ行いません。
	private IHttpClient CreateHttp(string method, string uri)
	{
		var conn_uri = CreateParams(method, uri);

		var client = new HttpClient(conn_uri);
		if (UseOAuthHeader) {
			client.AddHeader(MakeOAuthHeader());
		}
		return client;
	}

	// uri_request_token に接続しトークンを取得します。
	// 取得したトークンは token, token_secret に格納します。
	public void RequestToken(string uri_request_token)
	{
		var client = CreateHttp("GET", uri_request_token);

		Dictionary<string, string> resultDict = new Dictionary<string, string>();

		try {
			var stream = client.GET();
			// TODO: content-encoding とかに応じた処理
			var datastream = new DataInputStream(stream);
			datastream.set_newline_type(DataStreamNewlineType.CR_LF);
			string buf;
			while ((buf = datastream.read_line()) != null) {
				ParseQuery(resultDict, buf);
			}
		} catch (Error e) {
			stderr.printf("%s\n", e.message);
		}

		token = resultDict["oauth_token"];
		token_secret = resultDict["oauth_token_secret"];
	}

	// uri_api に method (GET/POST) で接続します。
	public InputStream RequestAPI(string method, string uri_api) throws Error
	{
		diag.Trace("CreateHttp call");
		RequestAPIClient = CreateHttp(method, uri_api);
		diag.Trace("CreateHttp return");

		// Ciphers 指定があれば指示
		if (Ciphers != null) {
			RequestAPIClient.SetCiphers(Ciphers);
		}

		diag.Trace(@"client.$(method) call");
		var rv = RequestAPIClient.Act(method);
		diag.Trace(@"client.$(method) return");
		return rv;
	}
}
