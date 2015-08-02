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
				dIn.set_newline_type(DataStreamNewlineType.CR_LF);

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

			InputStream rv;
			var transfer_encoding = RecvHeaders["transfer-encoding"] ?? "";
			if (transfer_encoding == "chunked") {
				// チャンク
				rv = new ChunkedInputStream(dIn);
			} else {
				// ボディをメモリに読み込んで、そのメモリへのストリームを返す。
				// https の時はストリームの終了で TlsConnection が例外を吐く。
				// そのため、ストリームを直接外部に渡すと、予期しないタイミング
				// で例外になるので、一旦メモリに読み込む。
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
				rv = new MemoryInputStream.from_data(msdata, null);
			}

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

			// 1行目は応答行
			ResultLine  = dIn.read_line();
			if (ResultLine == null || ResultLine == "") {
				throw new IOError.CONNECTION_CLOSED("");
			}
			diag.Debug(@"HEADER $(ResultLine)");

			var proto_arg = StringUtil.Split2(ResultLine, " ");
			if (proto_arg[0] == "HTTP/1.1" || proto_arg[0] == "HTTP/1.0") {
				var code_msg = StringUtil.Split2(proto_arg[1], " ");
				ResultCode = int.parse(code_msg[0]);
				diag.Debug(@"ResultCode=$(ResultCode)");
			}

			string prevKey = "";

			// 2行目以降のヘッダを読みこむ
			// 1000 行で諦める
			for (int i = 0; i < 1000; i++) {

				var s = dIn.read_line();
				if (s == null) {
					throw new IOError.CONNECTION_CLOSED("");
				}

				diag.Debug(@"HEADER |$(s)|");

				// End of header
				if (s == "") break;

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
					RecvHeaders.AddOrUpdate(prevKey, StringUtil.Trim(kv[1]));
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
		private bool Tls_Accept(TlsCertificate peer_cert,
			TlsCertificateFlags errors)
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

	public class ChunkedInputStream
		: DataInputStream
	{
		private Diag diag = new Diag("ChunkedInputStream");

		// 入力ストリーム
		private DataInputStream Src;

		private MemoryInputStream Chunks;

		public ChunkedInputStream(DataInputStream stream)
		{
			Src = stream;
			Src.set_newline_type(DataStreamNewlineType.CR_LF);

			Chunks = new MemoryInputStream();
		}

#if 0
		// チャンクを読み込みます。
		// 読み込めたら true を返します。
		private bool ReadChunk() throws Error
		{
			var intlen = 0;
			var len = Src.read_line();
			len.scanf("%x", &intlen);
			diag.Debug(@"intlen = $(intlen)");
			if (intlen == 0) {
				return false;
			}

			uint8[] buf = new uint8[intlen];
			size_t redlen;
			bool r = Src.read_all(buf, out redlen);
			if (r == false) {
				diag.Debug("read_all false");
				return false;
			}
			if (redlen != intlen) {
				diag.Debug(@"redlen=$(redlen) intlen=$(intlen)");
				return false;
			}

			Chunks.add_data(buf, null);

			// 最後の CRLF を読み捨てる
			Src.read_line();

			return true;
		}

		private bool TryReadLine(out string rv) throws Error
		{
			diag.Trace("TryReadLine");
			int64 chunksLength;
			Chunks.seek(0, SeekType.END);
			chunksLength = Chunks.tell();

			if (chunksLength == 0) {
				diag.Debug("chunksLength=0");
				rv = null;
				return false;
			}

			Chunks.seek(0, SeekType.SET);

			var sb = new StringBuilder();

			bool f_crlf = false;
			uint8[] b = new uint8[1];
			for (int i = 0; i < chunksLength; i++) {
				Chunks.read(b);
//stderr.printf("%02X ", b[0]);
				char c = (char)b[0];
				sb.append_c(c);
				if (c == '\n') {
					if (sb.len > 1 && (char)sb.data[sb.len - 2] == '\r') {
						f_crlf = true;
						break;
					}
				}
			}

			if (f_crlf) {
				diag.Debug("CRLF found");
				int remain = (int)chunksLength - (int)sb.len;
				if (remain > 0) {
					// 読み込み終わった部分を Chunks を作りなおすことで破棄する
					uint8[] tmp = new uint8[remain];
					Chunks.read(tmp);
					Chunks = null;
					Chunks = new MemoryInputStream();
					Chunks.add_data(tmp, null);
				} else {
					// きっかりだったので空にする。
					Chunks = null;
					Chunks = new MemoryInputStream();
				}
				// CRLF は取り除く。
				sb.erase(sb.len - 2);
				rv = sb.str;
//stderr.printf("\nrv=%s\n", rv);
				return true;
			} else {
				diag.Debug("No line");
				rv = null;
				return false;
			}
		}

		public string? read_line() throws Error
		{
			for (;;) {
				string rv;
				if (TryReadLine(out rv)) {
					return rv;
				} else {
					if (ReadChunk() == false) {
						return null;
					}
				}
			}
		}
#endif

		public override bool close(Cancellable? cancellable = null)
			throws IOError
		{
			// XXX Not implemented
			return false;
		}

		public override ssize_t read(uint8[] buffer,
			Cancellable? cancellable = null) throws IOError
		{
			diag.Debug("read %d".printf(buffer.length));

			// 内部バッファの長さ
			int64 chunksLength;
			try {
				Chunks.seek(0, SeekType.END);
				chunksLength = Chunks.tell();
			} catch (Error e) {
				diag.Debug(@"Chunks.seek/tell $(e.message)");
				chunksLength = 0;
			}
			diag.Debug(@"chunksLength=$(chunksLength)");

			while (chunksLength == 0) {
				// 内部バッファが空なら、チャンクを読み込み
				var intlen = 0;
				var len = Src.read_line();
				if (len == null) {
					// EOF
					diag.Debug("Src is EOF");
					return -1;
				}
				len.scanf("%x", &intlen);
				diag.Debug(@"intlen = $(intlen)");
				if (intlen == 0) {
					// データ終わり。CRLF を読み捨てる
					Src.read_line();
					break;
				}

				uint8[] buf = new uint8[intlen];
				size_t redlen;
				bool r = Src.read_all(buf, out redlen);
				if (r == false) {
					diag.Debug("read_all false");
					return -1;
				}
				diag.Debug(@"redlen=$(redlen)");
				if (redlen != intlen) {
					diag.Debug(@"redlen=$(redlen) intlen=$(intlen)");
					return -1;
				}

				Chunks.add_data(buf, null);
				// 長さを再計算
				try {
					Chunks.seek(0, SeekType.END);
					chunksLength = Chunks.tell();
				} catch (Error e) {
					diag.Debug(@"Chunks.seek/tell(2) $(e.message)");
					chunksLength = 0;
				}
				diag.Debug(@"chunksLength=$(chunksLength)");

				// 最後の CRLF を読み捨てる
				Src.read_line();
			}

			// buffer に入るだけコピー
			var copylen = chunksLength;
			if (copylen > buffer.length) {
				copylen = buffer.length;
			}
			diag.Debug(@"copylen=$(copylen)");
			Chunks.read(buffer);

			// 今書き出した部分を取り除いた Chunks を再構築
			// 全部書き出したら空になってるので何もしなくていい
			if (copylen < chunksLength) {
				diag.Debug("reconst chunk");
				uint8[] tmp = new uint8[chunksLength - buffer.length];
				Chunks.read(tmp);
				Chunks = null;
				Chunks = new MemoryInputStream();
				Chunks.add_data(tmp, null);
			}

			return (ssize_t)copylen;
		}
	}
}

