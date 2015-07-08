
public class OAuth
{
	private Diag diag = new Diag("OAuth");


	public string URL { get; set; }

	public string ConsumerSecret { get; set; }

	private Rand rand;

	public OAuth()
	{
		//rand = new Rand.with_seed((uint32)(new Datetime.now_utc().to_unix()));
		rand = new Rand();
	}

	// Nonce のための文字列を取得します。
	// 呼び出すたびに異なる文字列が生成されます。
	public string GetNonce()
	{
		var sb = new StringBuilder();
		for (int i = 0; i < 5; i++) {
			// TODO: twitter のドキュメントには alphanumeric って書いてある
			sb.append_c((char)(rand.int_range(32, 127)));
		}
		diag.Debug("rv:");
		diag.DebugHex(sb.data, (int)sb.len);
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
}
