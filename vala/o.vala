using ULib;

class Program 
{
	public static int main(string[] args)
	{
		Diag.global_debug = true;
		var diag = new Diag("main");

		var unixtime = new DateTime.now_utc().to_unix();
		var once = "abcdef";

		var consumer_secret = "faGcW9MMmU0O6qTrsHgcUchAiqxDcU9UjDW2Zw";

		var method = "GET";
		var url = "https://api.twitter.com/oauth/request_token";
		var params = 
			  "oauth_consumer_key=jPY9PU5lvwb6s9mqx3KjRA"
			+ @"&oauth_nonce=$(once)"
			+ "&oauth_signature_method=HMAC-SHA1"
			+ @"&oauth_timestamp=$(unixtime)"
			+ "&oauth_version=1.0";

		var encoded_url = UrlEncode(url);
		var encoded_params = UrlEncode(params);

		var message = @"$(method)&$(encoded_url)&$(encoded_params)";

		var key = consumer_secret + "&";

		var signature = OAuth.GenHash(OAuth.HashType.HMAC_SHA1, key, message);

		params += @"&oauth_signature=$(signature)";

Process.exit(1);
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
}

public errordomain OAuthError
{
	Fatal,
}

public class OAuth
{
	public const string ConsumerKeyName = "oauth_consumer_key";

	public enum HashType
	{
		HMAC_SHA1,
		PlainText,
		RSA_SHA1,
	}

//	public uint8[] key;

	public static string GenHash(HashType hashType, string key, string message)
//		throws OAuthError
	{
		var diag = new Diag("HASH");

		if (hashType == HashType.HMAC_SHA1) {
			// HMAC SHA1 して Base64 
			diag.Debug(@"key=$(key)");
			diag.Debug(@"msg=$(message)");
			var hmstr = Hmac.compute_for_string(ChecksumType.SHA1,
				key.data,
				message);
			diag.Debug(@"HMAC=$(hmstr)");
			var rv = Base64.encode(hmstr.data);
			diag.Debug(@"rv=$(rv)");
			return rv;
/*
			var hm = new Hmac(ChecksumType.SHA1, key);
			hm.update(s.data);
			size_t len = 64;
			uint8[] digest = new uint8[len];
			hm.get_digest(digest, ref len);
			if (len > digest.length) {
				throw new OAuthError.Fatal("get_digest");
			}
			digest.resize((int)len);
			var rv = Base64.encode(digest);
			return rv;
*/
		} else {
//			throw new OAuthError.Fatal("hashType");
			return "";
		}
	}
}
