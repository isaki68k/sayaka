namespace ULib
{
	public class HttpClient
	{
		private Diag diag = new Diag("HttpClient");

		// ソケット
		public SocketClient Sock;

		// 基本コネクション。
		public SocketConnection BaseConn;

		// https の時の TlsConnection。
		public TlsClientConnection Tls;

		// 最終的に選択されたコネクション。
		// uri.Scheme に応じて設定される。
		public weak IOStream Conn;

		// パース後の URI
		public ParsedUri uri;

		// パース前の URI
		public string orig_uri;

		// uri をターゲットにした HttpClient を作成します。
		public HttpClient(string uri)
		{
			orig_uri = uri;
			this.uri = ParsedUri.Parse(uri);
			diag.Debug(this.uri.to_string());
		}

		// uri から GET して、ストリームを返します。
		public InputStream GET() throws Error
		{
			Connect();

			var stream = RequestGET();

			var dIn = new DataInputStream(stream);

			// ヘッダを読み飛ばす。
			// TODO: 200 OK かどうかの処理とか。
			do {
				var s = dIn.read_line();
				if (s == null) break;
//stderr.printf("HEADER %s\n", s);
				if (s == "\r") break;
			} while (true);

			// ボディをメモリに読み込んで、そのメモリへのストリームを返す。
			// https の時はストリームの終了で TlsConnection が例外を吐く。
			// そのため、ストリームを直接外部に渡すと、予期しないタイミングで
			// 例外になるので、一旦メモリに読み込む。
			var ms = new MemoryOutputStream.resizable();
			try {
				ms.splice(dIn, 0);
			} catch {
				// ignore
			}
			ms.close();

			// TODO: ソケットのクローズ

			// ms のバックエンドバッファの所有権を移す。
			var msdata = ms.steal_data();
			msdata.length = (int)ms.get_data_size();
			var rv = new MemoryInputStream.from_data(msdata, null);

			return rv;
		}

		// GET リクエストを発行します。
		// TODO: private にする
		public InputStream RequestGET() throws Error
		{
			var sb = new StringBuilder();

			sb.append(@"GET $(uri.Path) HTTP/1.1\r\n");
			sb.append(@"Host: $(uri.Host)\r\n");
			sb.append("Connection: close\r\n");
			sb.append("\r\n");

//stderr.printf("RequestGET\n%s", sb.str);

			var msg = sb.str;

			Conn.output_stream.write(msg.data);

			return Conn.input_stream;
		}

		// uri へ接続します。
		// TODO: private にする
		public IOStream Connect() throws Error
		{
			int16 port = 80;
//stderr.printf("%s %s %s %s %s %s %s %s\n",
//	uri.Scheme, uri.Host, uri.Port, uri.User, uri.Password, uri.Path, uri.Query, uri.Fragment);

			// デフォルトポートの書き換え
			if (uri.Scheme == "https") {
				port = 443;
			}

			if (uri.Port != "") {
				port = (int16)int.parse(uri.Port);
			}

			// 名前解決
			var resolver = Resolver.get_default();
			var addressList = resolver.lookup_by_name(uri.Host, null);

			// １個目のアドレスへ接続。
			var address = addressList.nth_data(0);

			Sock = new SocketClient();

			// 基本コネクションの接続。
			BaseConn = Sock.connect(new InetSocketAddress(address, port));

			if (uri.Scheme == "https") {
				// TLS コネクションに移行する。
				Tls = TlsClientConnection.@new(BaseConn, null);

				// どんな証明書でも受け入れる。
				// 本当は、Tls.validation_flags で制御できるはずだが
				// うまくいかない。
				// accept_certificate signal (C# の event 相当)
				// を接続して対処したらうまく行った。
				Tls.accept_certificate.connect(Tls_Accept);
				Conn = Tls;
			} else {
				Conn = BaseConn;
			}

			return Conn;
		}

		// TLS の証明書を受け取った時のイベント。
		private bool Tls_Accept(TlsCertificate peer_cert, TlsCertificateFlags errors)
		{
			// true を返すと、その証明書を受け入れる。
			// がばがば
			return true;
		}
	}
}

