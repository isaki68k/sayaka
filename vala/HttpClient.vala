/*
 * Copyright (C) 2015 Y.Sugahara (moveccr)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

using Native.mTLS;

namespace ULib
{
	public class HttpClient
		: Object, IHttpClient
	{
		const int SHUT_RD = 0;
		const int SHUT_WR = 1;
		const int SHUT_RDWR = 2;

		private Diag diag;

		// mTLS ハンドル
		private Native.mTLS.mTLSHandle Handle;

		// パース後の URI
		public ParsedUri Uri;

		// リクエスト時にサーバへ送る追加のヘッダ
		// Host: はこちらで生成するので呼び出し側が指定しないでください。
		public Array<string> SendHeaders;

		// 受け取ったヘッダ
		public Array<string> RecvHeaders;

		// 受け取った応答行
		public string ResultLine;

		// 受け取った応答コード
		public int ResultCode;

		// コネクションに使用するプロトコルファミリ
		// IPv4/IPv6 only にしたい場合はコンストラクタ後に指定?
		// ただし mbedTLS 版は API が指定に対応していないので、未対応。
		public SocketFamily Family;

		// 使用する CipherSuites
		// ただし null(デフォルト) と "RSA" しか対応してない。
		public string Ciphers;

		// 特定サーバだけの透過プロキシモード?
		// "userstream.twitter.com=http://127.0.0.1:10080/"
		// みたいに指定する
		public static string ProxyMap;


		// 新しい HttpClient を生成します。
		public HttpClient(Diag diag_)
		{
			diag = diag_;

			SendHeaders = new Array<string>();
			RecvHeaders = new Array<string>();
			Ciphers = null;
			// XXX AF_UNSPEC がなさげなのでとりあえず代用
			Family = SocketFamily.INVALID;
		}

		// uri をターゲットに初期化します。
		public bool Init(string uri)
		{
			Handle = new mTLSHandle();
			if (Handle.Init() != 0) {
				stderr.printf("HttpClient.Init: mTLSHandle.Init failed\n");
				Handle = null;
				return false;
			}

			Uri = ParsedUri.Parse(uri);
			diag.Debug(Uri.to_string());

			return true;
		}

		// uri から GET して、ストリームを返します。
		public DataInputStream GET() throws Error
		{
			return Act("GET");
		}

		// uri へ POST して、ストリームを返します。
		public DataInputStream POST() throws Error
		{
			return Act("POST");
		}

		// uri へ GET/POST して、ストリームを返します。
		// GET と POST の共通部です。
		public DataInputStream Act(string method) throws Error
		{
			diag.Trace(@"$(method)()");
			DataInputStream dIn = null;

			while (true) {
				Connect();

				SendRequest(method);

				var ms = new mTLSInputStream(Handle, diag);
				dIn = new DataInputStream(ms);
				dIn.set_newline_type(DataStreamNewlineType.CR_LF);

				ReceiveHeader(dIn);

				if (300 <= ResultCode && ResultCode < 400) {
					Close();
					var location = GetHeader(RecvHeaders, "Location");
					diag.Debug(@"Redirect to $(location)");
					if (location != null) {
						var newUri = ParsedUri.Parse(location);
						if (newUri.Scheme != "") {
							// Scheme があればフルURIとみなす
							Uri = ParsedUri.Parse(location);
						} else {
							// そうでなければ相対パスとみなす
							Uri.Path = newUri.Path;
							Uri.Query = newUri.Query;
							Uri.Fragment = newUri.Fragment;
						}
						diag.Debug(Uri.to_string());
						continue;
					}
				} else if (ResultCode >= 400) {
					throw new IOError.NOT_CONNECTED(ResultLine);
				}

				break;
			}

			DataInputStream rv;
			var transfer_encoding = GetHeader(RecvHeaders, "Transfer-Encoding");
			if (transfer_encoding == "chunked") {
				// チャンク
				diag.Debug("use ChunkedInputStream");
				rv = new ChunkedInputStream(dIn, diag);
			} else {
				rv = dIn;
			}

			return rv;
		}

		public void AddHeader(string s)
		{
			SendHeaders.append_val(s);
		}

		// GET/POST リクエストを発行します。
		private void SendRequest(string method) throws Error
		{
			var sb = new StringBuilder();

			string path = (method == "POST") ? Uri.Path : Uri.PQF();
			sb.append(@"$(method) $(path) HTTP/1.1\r\n");

			for (var i = 0; i < SendHeaders.length; i++) {
				var h = SendHeaders.index(i);
				sb.append(@"$(h)\r\n");
			}
			sb.append("Connection: close\r\n");
			sb.append("Host: %s\r\n".printf(Uri.Host));

			// User-Agent は SHOULD
			sb.append("User-Agent: HttpClient.vala\r\n");

			if (method == "POST") {
				sb.append("Content-Type: " +
					"application/x-www-form-urlencoded\r\n");
				sb.append(@"Content-Length: $(Uri.Query.length)\r\n");
				sb.append("\r\n");
				sb.append(Uri.Query);
			} else {
				sb.append("\r\n");
			}

			diag.Debug(@"Request $(method)\n$(sb.str)");

			var msg = sb.str;

			Handle.write(msg.data, msg.data.length);

			Handle.shutdown(SHUT_WR);

			diag.Trace("SendRequest() request sent");
		}

		// ヘッダを受信します。
		private void ReceiveHeader(DataInputStream dIn) throws Error
		{
			RecvHeaders = null;
			RecvHeaders = new Array<string>();

			diag.Trace("ReceiveHeader()");

			// 1行目は応答行
			ResultLine  = dIn.read_line();
			if (ResultLine == null || ResultLine == "") {
				throw new IOError.CONNECTION_CLOSED("");
			}
			diag.Debug(@"HEADER |$(ResultLine)|");

			var proto_arg = StringUtil.Split2(ResultLine, " ");
			if (proto_arg[0] == "HTTP/1.1" || proto_arg[0] == "HTTP/1.0") {
				var code_msg = StringUtil.Split2(proto_arg[1], " ");
				ResultCode = int.parse(code_msg[0]);
				diag.Debug(@"ResultCode=$(ResultCode)");
			}

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
					var lastidx = RecvHeaders.length - 1;
					var prev = RecvHeaders.index(lastidx);
					RecvHeaders.remove_index(lastidx);
					prev += s.chomp();
					RecvHeaders.append_val(prev);
				} else {
					RecvHeaders.append_val(s.chomp());
				}
			}

			// XXX: 1000 行あったらどうすんの
		}

		// ヘッダ配列から指定のヘッダを検索してボディを返します。
		// 指定されたヘッダが存在しない場合は "" を返します。
		public string GetHeader(Array<string> header, string key)
		{
			var key2 = key.ascii_down();
			for (var i = 0; i < header.length; i++) {
				var kv = StringUtil.Split2(header.index(i), ":");
				if (key2 == kv[0].ascii_down()) {
					return kv[1].chug();
				}
			}
			return "";
		}

		// uri へ接続します。
		private void Connect() throws Error
		{
			mTLSHandle.set_debuglevel(diag.GetLevel());

			// 透過プロキシ(?)設定があれば対応。
			var proxyTarget = "";
			var proxyUri = new ParsedUri();
			if (ProxyMap != null && ProxyMap != "") {
				var map = ProxyMap.split("=");
				proxyTarget = map[0];
				proxyUri = ParsedUri.Parse(map[1]);

				// 宛先がプロキシサーバのアドレスなら、差し替える
				if (Uri.Host == proxyTarget) {
					Uri = proxyUri;
				}
			}

			// デフォルトポート番号の処理。
			// ParsedUri はポート番号がない URL だと Port = "" になる。
			if (Uri.Port == "") {
				if (Uri.Scheme == "https") {
					Uri.Port = "443";
				} else {
					Uri.Port = "80";
				}
			}

			// 接続
			if (Uri.Scheme == "https") {
				Handle.setssl(true);
			}
			if (Ciphers != null && Ciphers == "RSA") {
				// XXX RSA 専用
				Handle.usersa();
			}
			diag.Trace(@"Connect(): $(Uri)");
			if (Handle.connect(Uri.Host, Uri.Port) != 0) {
				diag.Debug(@"mTLSHandle.connect: failed");
				throw new IOError.HOST_NOT_FOUND(@"$(Uri.Host)");
			}
		}

		// 接続を閉じます。
		public void Close() throws Error
		{
			diag.Trace("Close");

			// nothing to do ?
		}

		// Ciphers を設定します。
		public void SetCiphers(string ciphers)
		{
			Ciphers = ciphers;
		}

		// タイムアウト [msec] を設定します。
		public void SetTimeout(int timeout)
		{
			Handle.set_timeout(timeout);
		}
	}

	public class ChunkedInputStream
		: DataInputStream
	{
		private Diag diag;

		// キャスト用
		private unowned DataInputStream Src;

		private MemoryInputStream Chunks;

		public ChunkedInputStream(DataInputStream stream, Diag diag_)
		{
			diag = diag_;

			Object(base_stream:stream);

			Src = (DataInputStream)base_stream;
			Src.set_newline_type(DataStreamNewlineType.CR_LF);

			Chunks = new MemoryInputStream();
		}

		public override ssize_t read(uint8[] buffer,
			Cancellable? cancellable = null) throws IOError
		{
			diag.Debug(@"read $(buffer.length)");

			// 内部バッファの長さ
			int64 chunksLength;
			try {
				Chunks.seek(0, SeekType.END);
				chunksLength = Chunks.tell();
			} catch (Error e) {
				diag.Debug(@"seek(END) failed: $(e.message)");
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
				} catch (Error e) {
					diag.Debug(@"seek(END) failed: $(e.message)");
				}
				chunksLength = Chunks.tell();
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
			try {
				Chunks.seek(0, SeekType.SET);
			} catch (Error e) {
				diag.Debug(@"seek(SET) failed: $(e.message)");
			}
			Chunks.read(buffer);

			var remain = chunksLength - copylen;
			diag.Debug(@"remain=$(remain)");
			if (remain > 0) {
				// 読み込み終わった部分を Chunks を作りなおすことで破棄する
				uint8[] tmp = new uint8[remain];
				try {
					Chunks.seek(copylen, SeekType.SET);
				} catch (Error e) {
					diag.Debug(@"seek(SET) failed: $(e.message)");
				}
				Chunks.read(tmp);
				Chunks = null;
				Chunks = new MemoryInputStream();
				Chunks.add_data(tmp, null);
				chunksLength = Chunks.tell();
				diag.Debug(@"new ChunkLength=$(chunksLength)");
			} else {
				// きっかりだったので空にする。
				Chunks = null;
				Chunks = new MemoryInputStream();
			}

			return (ssize_t)copylen;
		}
	}

	///////////////////
	// mTLS stream
	///////////////////

	public class mTLSInputStream : InputStream
	{
		private Diag diag;

		private unowned mTLSHandle handle;

		public mTLSInputStream(mTLSHandle handle, Diag diag_)
		{
			diag = diag_;
			this.handle = handle;
		}

		public override ssize_t read(uint8[] buffer,
			Cancellable? cancellable = null) throws IOError
		{
			ssize_t rv = (ssize_t)handle.read((uint8 *)buffer,
				buffer.length);
			diag.Debug(@"read rv=$(rv)");
			return rv;
		}

		public override bool close(Cancellable? cancellable = null)
			throws IOError
		{
			return true;
		}
	}

#if false
	// InputStream と対称なので書いてはみたものの、現在の HttpClient
	// 実装は OutputStream を使わず直接 write しているので、これは不要。
	public class mTLSOutputStream : OutputStream
	{
		private Diag diag;

		private unowned mTLSHandle handle;

		public mTLSOutputStream(mTLSHandle handle, Diag diag_)
		{
			diag = diag_;
			this.handle = handle;
		}

		public override ssize_t write(uint8[] buffer,
			Cancellable? cancellable = null) throws IOError
		{
			string buf = (string)buffer;
			ssize_t rv = (ssize_t)handle.write((uint8 *)buffer,
				buffer.length);
			diag.Debug(@"write=$(buf)");
			diag.Debug(@"write return $(rv)");
			return rv;
		}

		public override bool close(Cancellable? cancellable = null)
			throws IOError
		{
			return true;
		}
	}
#endif
}
