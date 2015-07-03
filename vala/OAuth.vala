
public class OAuth
{
	private Diag diag = new Diag("OAuth");

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
