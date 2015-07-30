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
		var consumer_key = "key";
		var consumer_secret = "secret";

		var uri_request_token = "http://term.ie/oauth/example/request_token.php";
		var uri_access_token = "http://term.ie/oauth/example/access_token.php";
		var uri_api = "http://term.ie/oauth/example/echo_api.php";


		Diag.global_debug = true;
		var diag = new Diag("main");
		var oauth = new OAuth();

		var paramdict = new Dictionary<string, string>();

		var nonce = oauth.GetNonce();
		var unixtime = new DateTime.now_utc().to_unix();

		paramdict["oauth_version"] = "1.0";
		paramdict["oauth_signature_method"] = "HMAC-SHA1";
		paramdict["oauth_nonce"] = nonce;
		paramdict["oauth_timestamp"] = unixtime.to_string();
		paramdict["oauth_consumer_key"] = consumer_key;
		paramdict["oauth_version"] = "1.0";

		var method = "GET";
		var encoded_uri = UrlEncode(uri_request_token);
		var encoded_params = UrlEncode(MakeQuery(paramdict));
		var message = @"$(method)&$(encoded_uri)&$(encoded_params)";

		var key = consumer_secret + "&";

		var signature = oauth.HMAC_SHA1_Base64(key, message);

		paramdict["oauth_signature"] = signature;

		var client = new HttpClient(@"$(uri_request_token)?$(MakeQuery(paramdict))");
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
		


		foreach (KeyValuePair<string, string> p in paramdict) {
			diag.Debug(@"$(p.Key)=$(p.Value)");
		}

		stdout.printf("\n");

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

