class Program
{
	public static int main(string[] args)
	{
		var ssldump = new SSLDump();
		return ssldump.main(args);
	}
}

class SSLDump
{
	public int main(string[] args)
	{
		var stdin_stream = new UnixInputStream(0, false);
		var stream = new DataInputStream(stdin_stream);

		for (;;) {
			// これが親らしい。
			DumpTLSPlainText(stream);
		}
		return 0;
	}

	//
	// TLSPlainText
	//
	public void DumpTLSPlainText(DataInputStream st)
	{
		var type = new ContentType();
		var version = new ProtocolVersion();
		uint16 length;
		// opaque fragment[TLSPlaintext.length];

		type.Read(st);
		version.Read(st);
		length = st.read_int16();

		MemoryStream ms = new MemoryStream();
		ms.AppendFromInputStream(st, length);

		stdout.printf(@"TLSPlaintext: ContentType=$(type) "
			+ @"version=$(version) Length=$(length)\n");

		switch (type.value) {
		 case ContentTypeKind.handshake:
			DumpHandshake(ms);
			break;
		 default:
			stdout.printf(@"TLSPlainText: type=$(type) not supported\n");
			break;
		}
	}

	//
	// Handshake
	//
	public void DumpHandshake(MemoryStream st)
	{
		var msg_type = new HandshakeType();
		int length;

		msg_type.Read(st);
		length = st.read_uint24();

		stdout.printf(@"Handshake: msg_type=$(msg_type) length=$(length)\n");

		switch (msg_type.value) {
		 case HandshakeTypeKind.client_hello:
			DumpHandshake_ClientHello("Client", st);
			break;
		 case HandshakeTypeKind.server_hello:
			DumpHandshake_ClientHello("Server", st);
			break;
		 default:
			stdout.printf("Handshake: msg_type=$(msg_type) not supported\n");
			break;
		}
	}

	//
	// Handshake: client_hello / server_hello
	//
	public void DumpHandshake_ClientHello(string name, MemoryStream st)
	{
		var version = new ProtocolVersion();
		var random = new Random();
		var session_id = new SessionId();
		var cipher_suites = new CipherSuites();
		var compression_methods = new CompressionMethods();
		var extensions = new Extensions();

		version.Read(st);
		random.Read(st);
		session_id.Read(st);
		cipher_suites.Read(st);
		compression_methods.Read(st);
		if (st.EOF() == false) {
			extensions.Read(st);
		}

		stdout.printf(@"\t%s_version=$(version)\n", name.down());
		stdout.printf(@"\trandom=$(random)\n");
		stdout.printf(@"\tsession_id=$(session_id)\n");
		stdout.printf(@"\tcipher_suites=<%d>\n", cipher_suites.list.length());
		int i = 0;
		foreach (var cs in cipher_suites.list) {
			stdout.printf(@"\t\t[$(i)] $(cs)\n");
			i++;
		}
		i = 0;
		stdout.printf(@"\textensions=<%d>\n", extensions.list.length());
		foreach (var ext in extensions.list) {
			stdout.printf(@"\t\t[$(i)] $(ext)\n");
			i++;
		}
	}
}

/*
class uint24
{
	public uint value;

	public bool Read(DataInputStream st)
	{
		uint v[3];
		try {
			v[0] = st.read_byte();
			v[1] = st.read_byte();
			v[2] = st.read_byte();
		} catch {
			return false;
		}
		value = (v[0] << 16) + (v[1] << 8) + v[2];
		return true;
	}

	public string to_string()
	{
		return @"$(value)";
	}
}
*/

//
// 可変長フィールドの基底クラス
//
class VLField
{
	// 可変長フィールドを読み込んで、DataInputStream を返す。
	public MemoryStream? ReadVL(MemoryStream st)
	{
		uint8 n;

		// 長さフィールドを読み込む。
		// 長さフィールド自身の長さは仕様によって各々のフィールドでの
		// 最大長を格納できる最小バイト数である。
		// フィールドが255バイトまでならフィールド長は1バイト。
		// フィールドが65534(?)バイトまでならフィールド長は2バイト。
		// という具合。ややこしすぎる。
		length = 0;
		if (len_bytes == 1) {
			length = st.read_uint8();
		} else if (len_bytes == 2) {
			length = st.read_uint16();
		} else {
			// ???
			return null;
		}

		// データフィールドを用意して、そこに読み込む
		data = new uint8[length];
		int read_len = st.Read(data);
		if (read_len != length) {
			return null;
		}

		// データフィールドからメモリストリームを作成。
		MemoryStream rv = new MemoryStream();
		rv.Append(data);

		return rv;
	}

	// 長さ(自身を含まない)
	protected uint length;

	// 長さフィールド自身のバイト数
	// 継承クラスのコンストラクタで値(1-4)を設定してください。
	protected int len_bytes;

	// データ
	protected uint8[] data;
}

public enum ContentTypeKind {
	change_cipher_spec = 20,
	alert = 21,
	handshake = 22,
	application_data = 23,
}

class ContentType
{
	public uint8 value;

	public bool Read(MemoryStream st)
	{
		try {
			value = st.read_byte();
		} catch {
			return false;
		}
		return true;
	}

	public string to_string()
	{
		string name = null;
		switch (value) {
		 case ContentTypeKind.change_cipher_spec:
			name = "change_cipher_spec";
			break;
		 case ContentTypeKind.alert:
			name = "alert";
			break;
		 case ContentTypeKind.handshake:
			name = "handshake";
			break;
		 case ContentTypeKind.application_data:
			name = "application_data";
			break;
		 default:
			break;
		}
		if (name != null) {
			return "%s(%d)".printf(name, value);
		} else {
			return "%d".printf(value);
		}
	}
}

class ProtocolVersion
{
	private uint8 major;
	private uint8 minor;

	public bool Read(DataInputStream st)
	{
		try {
			major = st.read_byte();
			minor = st.read_byte();
		} catch {
			return false;
		}
		return true;
	}

	public string to_string()
	{
		int n = ((int)major << 8) + minor;
		string name = null;
		switch (n) {
		 case 0x0301:
			name = "TLS1.0";
			break;
		 case 0x0303:
			name = "TLS1.2";
			break;
		 default:
			break;
		}
		if (name != null) {
			return "%s(0x%04x)".printf(name, n);
		} else {
			return "0x%04x".printf(n);
		}
	}
}

public enum HandshakeTypeKind {
	hello_request = 0,
	client_hello = 1,
	server_hello = 2,
	certificate = 11,
	server_key_exchange = 12,
	certificate_request = 13,
	server_hello_done = 14,
	certificate_verify = 15,
	client_Key_exchange = 16,
	finished = 20,
}

class HandshakeType
{
	public uint8 value;

	public bool Read(DataInputStream st)
	{
		try {
			value = st.read_byte();
		} catch {
			return false;
		}
		return true;
	}

	public string to_string()
	{
		string name = null;

		switch (value) {
		 case HandshakeTypeKind.client_hello:
			name = "client_hello";
			break;
		}
		if (name != null) {
			return "%s(%d)".printf(name, value);
		} else {
			return "%d".printf(value);
		}
	}
}

class Random
{
	public uint8 data[32];

	public bool Read(DataInputStream st)
	{
		try {
			st.read(data);
		} catch {
			return false;
		}
		return true;
	}

	public string to_string()
	{
		var sb = new StringBuilder();
		for (int i = 0; i < data.length; i++) {
			sb.append("%02x".printf(data[i]));
		}
		return sb.str;
	}
}

class SessionId : VLField
{
	public SessionId()
	{
		len_bytes = 1;
	}

	public bool Read(DataInputStream st)
	{
		ReadVL(st);
		return true;
	}

	public string to_string()
	{
		var sb = new StringBuilder();
		sb.append(@"<$(length)>");
		for (int i = 0; i < data.length; i++) {
			sb.append("%02x".printf(data[i]));
		}
		return sb.str;
	}
}

class CipherSuite
{
	private uint8 value[2];

	public bool Read(DataInputStream st)
	{
		try {
			value[0] = st.read_byte();
			value[1] = st.read_byte();
		} catch {
			return false;
		}
		return true;
	}

	public string to_string()
	{
		uint n = (value[0] << 8) + value[1];
		return "0x%04x".printf(n);
	}
}

class CipherSuites : VLField
{
	public CipherSuites()
	{
		len_bytes = 2;
		list = new List<CipherSuite>();
	}

	public bool Read(DataInputStream st)
	{
		var field = ReadVL(st);
		for (;;) {
			var cs = new CipherSuite();
			if (cs.Read(field) == false) {
				break;
			}
			list.append(cs);
		}
		return true;
	}

	public string to_string()
	{
		return "XXX";
	}

	public List<CipherSuite> list;

}

class CompressionMethods : VLField
{
	public CompressionMethods()
	{
		len_bytes = 1;
	}

	public bool Read(DataInputStream st)
	{
		ReadVL(st);
		return true;
	}
}

class Extension : VLField
{
	public Extension()
	{
		len_bytes = 2;
	}

	public bool Read(DataInputStream st)
	{
		try {
			type = st.read_uint16();
			ReadVL(st);
		} catch {
			return false;
		}
		return true;
	}

	public string to_string()
	{
		return @"Type=$(type) Length=$(length)";
	}

	public uint16 type;
}

class Extensions : VLField
{
	public Extensions()
	{
		len_bytes = 2;
		list = new List<Extension>();
	}

	public bool Read(DataInputStream st)
	{
		var ds = ReadVL(st);
		for (;;) {
			var extension = new Extension();
			if (extension.Read(ds) == false) {
				break;
			}
			list.append(extension);
		}
		return true;
	}

	public List<Extension> list;
}
