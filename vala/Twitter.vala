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

	public static const string accessTokenURL  = "https://api.twitter.com/oauth/access_token";
	public static const string authorizeURL    = "https://twitter.com/oauth/authorize";
	public static const string requestTokenURL = "https://api.twitter.com/oauth/request_token";

	public static const string APIRoot = "https://api.twitter.com/1.1/";
	public static const string StreamAPIRoot = "https://userstream.twitter.com/1.1/";

	private static const string ConsumerKey = "jPY9PU5lvwb6s9mqx3KjRA";
	private static const string ConsumerSecret = "faGcW9MMmU0O6qTrsHgcUchAiqxDcU9UjDW2Zw";

	public Twitter_Token AccessToken;

	private OAuth oauth;

	private Diag diag = new Diag("twitter");

	public Twitter()
	{
		AccessToken = new Twitter_Token();

		oauth = new OAuth();

		oauth.ConsumerKey = ConsumerKey;
		oauth.ConsumerSecret = ConsumerSecret;
	}

	// Access Token を取得するところまで
	public void GetAccessToken()
	{
		oauth.AdditionalParams.Clear();

		diag.Debug("------ REQUEST_TOKEN ------");
		oauth.RequestToken(requestTokenURL);

		stdout.printf(
			@"PLEASE GO TO\n" +
			@"$(authorizeURL)?oauth_token=$(oauth.token)\n");
		stdout.printf("AND INPUT PIN\n");

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
		return API(StreamAPIRoot, api, options);
	}

	public DataInputStream API(string apiRoot, string api,
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
		var baseStream = oauth.RequestAPI(apiRoot + api + ".json");
		diag.Trace("RequestAPI return");

		var stream = new DataInputStream(baseStream);
		stream.set_newline_type(DataStreamNewlineType.CR_LF);

		return stream;
	}
}
