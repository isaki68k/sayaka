namespace ULib
{
	public class FileGetter
	{
		public SocketClient Sock;
		public SocketConnection BaseConn;
		public TlsClientConnection Tls;
		public weak IOStream Conn;
		public ParsedUri uri;
		public string orig_uri;

		public InputStream GET(string uri) throws Error
		{
			orig_uri = uri;
			this.uri = ParsedUri.Parse(uri);

			Connect();

			var stream = RequestGET();

			var dIn = new DataInputStream(stream);

			do {
				var s = dIn.read_line();
				if (s == null) break;
//stderr.printf("HEADER %s\n", s);
				if (s == "\r") break;
			} while (true);

			var ms = new MemoryOutputStream.resizable();
			try {
				ms.splice(dIn, 0);
			} catch {
				// ignore
			}
			ms.close();
			// ownership move to input stream
			var msdata = ms.steal_data();
			msdata.length = (int)ms.get_data_size();
			var rv = new MemoryInputStream.from_data(msdata, null);

			return rv;
		}

		public InputStream RequestGET() throws Error
		{
			var sb = new StringBuilder();

			sb.append(@"GET /$(uri.Path) HTTP/1.1\r\n");
			sb.append(@"Host: $(uri.Host)\r\n");
			sb.append("Connection: close\r\n");
			sb.append("\r\n");

//stderr.printf("RequestGET\n%s", sb.str);

			var msg = sb.str;

			Conn.output_stream.write(msg.data);

			return Conn.input_stream;
		}

		public IOStream Connect() throws Error
		{
			int16 port = 80;
//stderr.printf("%s %s %s %s %s %s %s %s\n",
//	uri.Scheme, uri.Host, uri.Port, uri.User, uri.Password, uri.Path, uri.Query, uri.Fragment);

			if (uri.Scheme == "https") {
				port = 443;
			}

			if (uri.Port != "") {
				port = (int16)int.parse(uri.Port);
			}

			var resolver = Resolver.get_default();
			var addressList = resolver.lookup_by_name(uri.Host, null);

			var address = addressList.nth_data(0);

			Sock = new SocketClient();

			BaseConn = Sock.connect(new InetSocketAddress(address, port));

			if (uri.Scheme == "https") {
				Tls = TlsClientConnection.@new(BaseConn, null);
				Tls.accept_certificate.connect(Tls_Accept);
				Conn = Tls;
			} else {
				Conn = BaseConn;
			}

			return Conn;
		}

		private bool Tls_Accept(TlsCertificate peer_cert, TlsCertificateFlags errors)
		{
			// がばがば
			return true;
		}
	}
}

