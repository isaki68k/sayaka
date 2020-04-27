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

public class Twitter_Token
{
	public string Token;
	public string Secret;

	public void LoadFromFile(string filename) throws Error
	{
		var json = Json.FromString(FileReadAllText(filename));

		Token = json.GetString("token");
		Secret = json.GetString("secret");
	}

	public void SaveToFile(string filename) throws Error
	{
		var dict = new Dictionary<string, Json>();
		dict["token"] = new Json.String(Token);
		dict["secret"] = new Json.String(Secret);

		var json = new Json.Object(dict);
		FileWriteAllText(filename, json.ToString());
	}
}


public class Twitter
{

	public const string accessTokenURL  = "https://api.twitter.com/oauth/access_token";
	public const string authorizeURL    = "https://twitter.com/oauth/authorize";
	public const string requestTokenURL = "https://api.twitter.com/oauth/request_token";

	public const string APIRoot = "https://api.twitter.com/1.1/";
	public const string StreamAPIRoot = "https://userstream.twitter.com/1.1/";
	public const string PublicAPIRoot = "https://stream.twitter.com/1.1/";

	private const string ConsumerKey = "jPY9PU5lvwb6s9mqx3KjRA";
	private const string ConsumerSecret = "faGcW9MMmU0O6qTrsHgcUchAiqxDcU9UjDW2Zw";

	public Twitter_Token AccessToken;

	private OAuth oauth;

	private Diag diag = new Diag("twitter");

	public Twitter()
	{
		diag.SetLevel(HttpClient.debuglevel);
		AccessToken = new Twitter_Token();

		oauth = new OAuth();

		oauth.ConsumerKey = ConsumerKey;
		oauth.ConsumerSecret = ConsumerSecret;
	}

	// Ciphers を設定します。
	// 実際には OAuth に指示するだけです。
	public void SetCiphers(string ciphers)
	{
		if (oauth != null) {
			oauth.SetCiphers(ciphers);
		}
	}

	// Access Token を取得するところまで
	public void GetAccessToken()
	{
		oauth.AdditionalParams.Clear();

		diag.Debug("------ REQUEST_TOKEN ------");
		oauth.RequestToken(requestTokenURL);

		stdout.printf(
			@"Please go to:\n" +
			@"$(authorizeURL)?oauth_token=$(oauth.token)\n");
		stdout.printf("\n");
		stdout.printf("And input PIN code: ");
		stdout.flush();

		var pin_str = stdin.read_line();

		diag.Debug("------ ACCESS_TOKEN ------");

		oauth.AdditionalParams["oauth_verifier"] = pin_str;

		oauth.RequestToken(accessTokenURL);

		AccessToken.Token = oauth.token;
		AccessToken.Secret = oauth.token_secret;
	}

	// UserStreamAPI("user", dictionary)
	public DataInputStream UserStreamAPI(string api,
		Dictionary<string, string>? options = null) throws Error
	{
		return GetAPI(StreamAPIRoot, api, options);
	}

	public DataInputStream GetAPI(string apiRoot, string api,
		Dictionary<string, string>? options = null) throws Error
	{
		return API("GET", apiRoot, api, options);
	}

	public DataInputStream PostAPI(string apiRoot, string api,
		Dictionary<string, string>? options = null) throws Error
	{
		return API("POST", apiRoot, api, options);
	}

	private DataInputStream API(string method, string apiRoot, string api,
		Dictionary<string, string>? options = null) throws Error
	{
		oauth.token = AccessToken.Token;
		oauth.token_secret = AccessToken.Secret;

		oauth.AdditionalParams.Clear();

		if (options != null) {
			foreach (KeyValuePair<string, string> kv in options) {
				oauth.AdditionalParams[kv.Key] = kv.Value;
			}
		}

		diag.Trace("RequestAPI call");
		var baseStream = oauth.RequestAPI(method, apiRoot + api + ".json");
		diag.Trace("RequestAPI return");

		var stream = new DataInputStream(baseStream);
		stream.set_newline_type(DataStreamNewlineType.CR_LF);

		return stream;
	}

	// API に接続し、結果の JSON を返します。
	// 接続が失敗、あるいは JSON が正しく受け取れなかった場合は
	// null を返します。
	public Json? API2Json(string method, string apiRoot, string api,
		Dictionary<string, string>? options = null)
	{
		DataInputStream stream = null;
		string line = null;
		try {
			stream = API(method, apiRoot, api, options);
			line = stream.read_line();
		} catch (Error e) {
			diag.Debug(@"$(api): $(e.message)");
			// nop
		}
		if (line == null || line == "") {
			return null;
		}

		Json json = null;
		var parser = new JsonParser();
		try {
			json = parser.Parse(line);
		} catch (Error e) {
			diag.Debug(@"$(api): Json parser failed: $(e.message)");
			// fall through
		}
		// どちらにしても json を返す
		return json;
	}
}
