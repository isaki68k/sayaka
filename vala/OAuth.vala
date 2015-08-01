using ULib;
using StringUtil;

public class OAuth
{
	private Diag diag = new Diag("OAuth");

	public string ConsumerKey { get; set; }
	public string ConsumerSecret { get; set; }

	public Dictionary<string, string> Params;

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

	public OAuth()
	{
		//rand = new Rand.with_seed((uint32)(new Datetime.now_utc().to_unix()));
		rand = new Rand();
		UseOAuthHeader = true;
	}

	// Nonce のための文字列を取得します。
	// 呼び出すたびに異なる文字列が生成されます。
	public string GetNonce()
	{
		var sb = new StringBuilder.sized(4);
		for (int i = 0; i < 4; i++) {
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
		diag.ProgErr(len > digest.length, "HMAC_SHA1 digest buffer overflow");

		// バイナリを Base64 した文字列を返す
		digest.resize((int)len);
		var rv = Base64.encode(digest);
		return rv;
	}

	// パラメータを作ってアクセス URI を返す
	public string CreateParams(string method, string uri)
	{
		Params = new Dictionary<string, string>();

		// 追加パラメータで初期化
		foreach (KeyValuePair<string, string> kv in AdditionalParams) {
			Params[kv.Key] = kv.Value;
		}

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

		var encoded_uri = UrlEncode(uri);
		var encoded_params = UrlEncode(MakeQuery(Params));
		var message = @"$(method)&$(encoded_uri)&$(encoded_params)";

		var key = ConsumerSecret + "&" + (token_secret ?? "");

		var signature = HMAC_SHA1_Base64(key, message);

		Params["oauth_signature"] = signature;

		// アクセス URI を返す
		if (UseOAuthHeader) {
			return @"$(uri)?$(MakeQuery(AdditionalParams))";
		} else {
			return @"$(uri)?$(MakeQuery(Params))";
		}
	}
	
	// dict を key1=value1&key2=value2 にする
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

	// key1=val1&key2=val2 のパース
	public static void ParseQuery(Dictionary<string, string> dict, string s)
	{
		var keyvalues = s.split("&");
		for (int i = 0; i < keyvalues.length; i++) {
			var kv = StringUtil.Split2(keyvalues[i], "=");
			dict[kv[0]] = kv[1];
		}
	}

	public static string MakeOAuthHeader(Dictionary<string, string> dict)
	{
		var sb = new StringBuilder();
		bool f_first = true;
		sb.append("OAuth ");
		foreach (KeyValuePair<string, string> p in dict) {
			if (f_first == false) {
				sb.append_c(',');
			} else {
				f_first = false;
			}
			sb.append("%s=\"%s\"".printf(p.Key, UrlEncode(p.Value)));
		}
		return sb.str;
	}

	private HttpClient CreateHttp(string method, string uri)
	{
		var conn_uri = CreateParams(method, uri);

		var client = new HttpClient(conn_uri);
		if (UseOAuthHeader) {
			client.SendHeaders["authorization"] = MakeOAuthHeader(Params);
		}
		return client;
	}

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

	private HttpClient RequestAPIClient;

	public InputStream RequestAPI(string uri_api) throws Error
	{
		diag.Trace("CreateHttp call");
		RequestAPIClient = CreateHttp("GET", uri_api);
		diag.Trace("CreateHttp return");

		diag.Trace("client.Get call");
		var rv = RequestAPIClient.GET();
		diag.Trace("client.Get return");
		return rv;
	}
}

