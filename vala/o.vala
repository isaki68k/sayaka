//
// 参考資料
// http://techno-st.net/2009/11/26/twitter-api-oauth-0.html
//

using ULib;
using StringUtil;

class Program 
{
	public static int main(string[] args)
	{
		return main_twitter(args);
	}

	static string token;
	static string token_secret;

	public static int main_twitter(string[] args)
	{
		token = "97356294-JUyVQlu2uXjYuN6nO2odY3Oo5Wa4YSMIlbtrAzgNH";
		token_secret = "IMXpMBFl1BBeVJTzLxZpjIHtCaqIWvMPRRWnd3Yumg36e";

		var uri_request_token = "https://twitter.com/oauth/request_token";
		var uri_access_token = "https://twitter.com/oauth/access_token";
	//	var uri_api = "https://stream.twitter.com/1.1/statuses/sample.json";
		var uri_api = "https://api.twitter.com/1.1/statuses/show.json";

		Diag.global_debug = true;
		var diag = new Diag("main");
		var oauth = new OAuth();

		oauth.ConsumerKey = "jPY9PU5lvwb6s9mqx3KjRA";
		oauth.ConsumerSecret = "faGcW9MMmU0O6qTrsHgcUchAiqxDcU9UjDW2Zw";

if (token == null) {
		diag.Debug("------ REQUEST_TOKEN ------");
		oauth.RequestToken(uri_request_token);

		diag.Debug("oauth.Params\n");
		foreach (KeyValuePair<string, string> p in oauth.Params) {
			diag.Debug(@"$(p.Key)=$(p.Value)");
		}
		stdout.printf("\n");

		diag.Debug("result");
		diag.Debug(@"token=$(oauth.token)");
		diag.Debug(@"token_secret=$(oauth.token_secret)");

		stdout.printf(
			@"PLEASE GO TO\n" +
			@"https://twitter.com/oauth/authorize?oauth_token=$(oauth.token)\n");
		stdout.printf("AND INPUT PIN\n");

		var pin_str = stdin.read_line();

		diag.Debug("------ ACCESS_TOKEN ------");

		oauth.AdditionalParams["oauth_verifier"] = pin_str;

		oauth.RequestToken(uri_access_token);

		diag.Debug("oauth.Params\n");
		foreach (KeyValuePair<string, string> p in oauth.Params) {
			diag.Debug(@"$(p.Key)=$(p.Value)");
		}
		stdout.printf("\n");

		diag.Debug("result");
		diag.Debug(@"token=$(oauth.token)");
		diag.Debug(@"token_secret=$(oauth.token_secret)");

} else {
	oauth.token = token;
	oauth.token_secret = token_secret;
}

		diag.Debug("------ API ------");
		try {
			oauth.AdditionalParams.Clear();

			oauth.AdditionalParams["id"] = "626352013131931648";
			var resultStream = oauth.RequestAPI(uri_api);

			diag.Debug("oauth.Params\n");
			foreach (KeyValuePair<string, string> p in oauth.Params) {
				diag.Debug(@"$(p.Key)=$(p.Value)");
			}
			stdout.printf("\n");

			var datastream = new DataInputStream(resultStream);
			datastream.set_newline_type(DataStreamNewlineType.CR_LF);
			string buf;
			while ((buf = datastream.read_line()) != null) {
				stdout.printf(buf);
			}
		} catch (Error e) {
			stderr.printf("ERROR: %s\n", e.message);
		}
		
		return 0;

	}

	public static int main_testserver(string[] args)
	{
		var uri_request_token = "http://term.ie/oauth/example/request_token.php";
		var uri_access_token = "http://term.ie/oauth/example/access_token.php";
		var uri_api = "http://term.ie/oauth/example/echo_api.php";


		Diag.global_debug = true;
		var diag = new Diag("main");
		var oauth = new OAuth();

		oauth.ConsumerKey = "key";
		oauth.ConsumerSecret = "secret";

		diag.Debug("------ REQUEST_TOKEN ------");
		oauth.RequestToken(uri_request_token);

		diag.Debug("oauth.Params\n");
		foreach (KeyValuePair<string, string> p in oauth.Params) {
			diag.Debug(@"$(p.Key)=$(p.Value)");
		}
		stdout.printf("\n");

		diag.Debug("result");
		diag.Debug(@"token=$(oauth.token)");
		diag.Debug(@"token_secret=$(oauth.token_secret)");

		diag.Debug("------ ACCESS_TOKEN ------");
		oauth.RequestToken(uri_access_token);

		diag.Debug("oauth.Params\n");
		foreach (KeyValuePair<string, string> p in oauth.Params) {
			diag.Debug(@"$(p.Key)=$(p.Value)");
		}
		stdout.printf("\n");

		diag.Debug("result");
		diag.Debug(@"token=$(oauth.token)");
		diag.Debug(@"token_secret=$(oauth.token_secret)");

		diag.Debug("------ API ------");
		try {
			oauth.AdditionalParams["hoge"] = "fuga";
			oauth.AdditionalParams["foo"] = "bar";

			var resultStream = oauth.RequestAPI(uri_api);

			diag.Debug("oauth.Params\n");
			foreach (KeyValuePair<string, string> p in oauth.Params) {
				diag.Debug(@"$(p.Key)=$(p.Value)");
			}
			stdout.printf("\n");

			var datastream = new DataInputStream(resultStream);
			datastream.set_newline_type(DataStreamNewlineType.CR_LF);
			string buf;
			while ((buf = datastream.read_line()) != null) {
				stdout.printf(buf);
			}
		} catch (Error e) {
			stderr.printf("ERROR: %s\n", e.message);
		}
		
		return 0;

	}

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

	public static void ParseQuery(Dictionary<string, string> dict, string s)
	{
		// key1=val1&key2=val2 のパース
		var keyvalues = s.split("&");
		for (int i = 0; i < keyvalues.length; i++) {
			var kv = StringUtil.Split2(keyvalues[i], "=");
			dict[kv[0]] = kv[1];
		}
	}



#if false
		

		paramdict["oauth_consumer_key"] = "jPY9PU5lvwb6s9mqx3KjRA";
		var consumer_secret = "faGcW9MMmU0O6qTrsHgcUchAiqxDcU9UjDW2Zw";

		var method = "GET";
		var url = "https://api.twitter.com/oauth/request_token";
		var params = 
			  "oauth_consumer_key=jPY9PU5lvwb6s9mqx3KjRA"
			+ @"&oauth_nonce=$(nonce)"
			+ "&oauth_signature_method=HMAC-SHA1"
			+ @"&oauth_timestamp=$(unixtime)"
			+ "&oauth_version=1.0";

		var encoded_url = UrlEncode(url);
		var encoded_params = UrlEncode(params);

		var message = @"$(method)&$(encoded_url)&$(encoded_params)";

		var key = consumer_secret + "&";

		var signature = oauth.HMAC_SHA1_Base64(key, message);

		params += @"&oauth_signature=$(UrlEncode(signature))";

		var client = new HttpClient(@"$(url)?$(params)");
		try {
			var stream = client.GET();
			var datastream = new DataInputStream(stream);
			string buf;
			while ((buf = datastream.read_line()) != null) {
				stdout.printf("%s", buf);
			}
		} catch (Error e) {
			stderr.printf("%s\n", e.message);
		}
		
		return 0;
	}
#endif

}

