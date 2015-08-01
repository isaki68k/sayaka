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
		public ParsedUri Uri;

		// リクエスト時にサーバへ送るヘッダ
		// キーは小文字です。
		public Dictionary<string, string> SendHeaders;

		// 受け取ったヘッダ
		// キーは小文字に変換されて格納されます。
		public Dictionary<string, string> RecvHeaders;

		// 受け取った応答行
		public string ResultLine;

		// 受け取った応答コード
		public int ResultCode;

		// コネクションに使用するプロトコルファミリ
		// IPv4/IPv6 only にしたい場合はコンストラクタ後に指定?
		public SocketFamily Family;


		// uri をターゲットにした HttpClient を作成します。
		public HttpClient(string uri)
		{
 			diag = new Diag("HttpClient");

			// XXX AF_UNSPEC がなさげなのでとりあえず代用
			Family = SocketFamily.INVALID;

			Uri = ParsedUri.Parse(uri);
			diag.Debug(Uri.to_string());

			SendHeaders = new Dictionary<string, string>();
			RecvHeaders = new Dictionary<string, string>();
		}

		// uri から GET して、ストリームを返します。
		public InputStream GET() throws Error
		{
			diag.Trace("GET()");
			DataInputStream dIn = null;

			while (true) {

				Connect();

				SendRequest("GET");

				dIn = new DataInputStream(Conn.input_stream);

				ReceiveHeader(dIn);

				if (300 <= ResultCode && ResultCode < 400) {
					Close();
					var location = StringUtil.Trim(RecvHeaders["location"]);
					diag.Debug(@"Redirect to $(location)");
					if (location != null) {
						Uri = ParsedUri.Parse(location);
						diag.Debug(Uri.to_string());
						SendHeaders.AddOrUpdate("host", Uri.Host);
						continue;
					}
				}

				break;
			}

#if 0
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
#else
			var rv = dIn;
#endif

			return rv;
		}

		// リクエストを発行します。
		private void SendRequest(string verb) throws Error
		{
			var sb = new StringBuilder();

			sb.append(@"$(verb) $(Uri.PQF()) HTTP/1.1\r\n");

			SendHeaders.AddIfMissing("host", Uri.Host);
			SendHeaders.AddIfMissing("connection", "close");

			foreach (KeyValuePair<string, string> h in SendHeaders) {
				sb.append(@"$(h.Key): $(h.Value)\r\n");
			}
			sb.append("\r\n");

			diag.Debug(@"Request $(verb)\n$(sb.str)");

			var msg = sb.str;

			Conn.output_stream.write(msg.data);
		}

		// ヘッダを受信します。
		private void ReceiveHeader(DataInputStream dIn) throws Error
		{
			RecvHeaders.Clear();

			string prevKey = "";

			// ヘッダを読みこむ
			// 1000 行で諦める
			for (int i = 0; i < 1000; i++) {

				var s = dIn.read_line();
				// End of stream
				if (s == null) break;

				diag.Debug(@"HEADER $(s)");

				// End of header
				if (s == "\r") break;

				if (i == 0) {
					// 応答行
					ResultLine = s;
					var proto_arg = StringUtil.Split2(s, " ");
					if (proto_arg[0] == "HTTP/1.1" || proto_arg[0] == "HTTP/1.0") {
						var code_msg = StringUtil.Split2(proto_arg[1], " ");
						ResultCode = int.parse(code_msg[0]);
						diag.Debug(@"ResultCode=$(ResultCode)");
					}
				} else {
					// ヘッダ行
					if (s[0] == ' ') {
						// 行継続
						if (prevKey == "") {
							throw new IOError.FAILED("Invalid Header");
						}
						RecvHeaders[prevKey] = RecvHeaders[prevKey] + s;
					} else {
						var kv = StringUtil.Split2(s, ":");
						// キーは小文字にする。
						prevKey = StringUtil.Trim(kv[0]).ascii_down();
						RecvHeaders.AddOrUpdate(prevKey, kv[1]);
					}
				}
			}

			// XXX: 1000 行あったらどうすんの

			diag.Debug(RecvHeaders.DumpString());
		}

		// uri へ接続します。
		private void Connect() throws Error
		{
			int16 port = 80;

			// デフォルトポートの書き換え
			if (Uri.Scheme == "https") {
				port = 443;
			}

			if (Uri.Port != "") {
				port = (int16)int.parse(Uri.Port);
			}

			// 名前解決
			var resolver = Resolver.get_default();
			var addressList = resolver.lookup_by_name(Uri.Host, null);

			InetAddress address = null;
			for (var i = 0; i < addressList.length(); i++) {
				address = addressList.nth_data(i);
				diag.Debug(@"Connect: address[$(i)]=$(address) port=$(port)");

				// アドレスファミリのチェック
				if (Family != SocketFamily.INVALID) {
					if (address.get_family() != Family) {
						diag.Debug(@"Connect: $(address) is not $(Family),"
							+ " skip");
						continue;
					}
				}

				// 基本コネクションの接続
				Sock = new SocketClient();
				try {
					BaseConn = Sock.connect(
						new InetSocketAddress(address, port));
				} catch (Error e) {
					diag.Debug(@"Sock.connect: $(e.message)");
					continue;
				}

				if (Uri.Scheme == "https") {
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
			}

			if (Conn == null) {
				throw new IOError.HOST_NOT_FOUND(@"$(Uri.Host)");
			}
		}

		// TLS の証明書を受け取った時のイベント。
		private bool Tls_Accept(TlsCertificate peer_cert, TlsCertificateFlags errors)
		{
			// true を返すと、その証明書を受け入れる。
			// がばがば
			return true;
		}

		// 接続を閉じます。
		public void Close() throws Error
		{
			diag.Trace("Close");
			Conn = null;
			if (Tls != null) {
				Tls.close();
				Tls = null;
			}
			if (BaseConn != null) {
				BaseConn.close();
				BaseConn = null;
			}
			if (Sock != null) {
				Sock = null;
			}
		}
	}
}

