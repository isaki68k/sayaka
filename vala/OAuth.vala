
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

	public uint8[] key;

	public string GenHash(HashType hashType, string s) throws OAuthError
	{
		if (hashType == HashType.HMAC_SHA1) {
			// HMAC SHA1 して Base64 
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
		} else if (hashType == HashType.PlainText) {
			return s;

		} else {
			throw new OAuthError.Fatal("hashType");
		}
	}

	public static string UrlEncode(string s)
	{
		var sb = new StringBuilder();
		// %xx に大文字の16進文字が要るとの情報が.
		foreach (char c in s) {
			if (('0' <= c && c <= '9')
			 || ('A' <= c && c <= 'Z')
			 || ('a' <= c && c <= 'z')
			 || (c == '-' || c == '_' || c == '.' || c == '~')) {
				sb.append_c(c);
			} else {
				sb.append("%02X".printf((int)c));
			}
		}
		return sb.str;
	}

}
