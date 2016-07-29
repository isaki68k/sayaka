using Native.Curl;

namespace ULib
{

	// Curl InputStream
	public class CurlInputStream
		: InputStream
	{
		public unowned EasyHandle Handle;

		public override bool close(Cancellable? cancellable = null) throws GLib.IOError
		{
			// クローズをしても Handle をクローズしません。
			// Curl クラス側でやってください。
			return true;
		}

		// Curl EasyHandle から recv します。
		public override ssize_t read(uint8[] buffer, Cancellable? cancellable = null) throws GLib.IOError
		{
			size_t n;
stderr.printf(@"recv buffer.length=$(buffer.length)");

/*
			Curl.Socket sock;

			{
				var r = Handle.getinfo(Option.ACTIVESOCKET, out sock);
				if (r != Code.OK) {
					throw new GLib.IOError.FAILED(r.to_string());
				}
			}

			// SELECT したーい

*/

			do {
			var r = Handle.recv(buffer, buffer.length, out n);
			if (r == Code.OK) {
stderr.printf(@"recv OK: $(r) $(n)\n");
stderr.printf(@"recv DATA: %s\n", (string)buffer);
				return (ssize_t)n;
			} else if (r == Code.AGAIN) {
				GLib.Thread.usleep(10000);
				continue;
				
			} else if (r == Code.UNSUPPORTED_PROTOCOL && n == 0) {
stderr.printf(@"recv EOF: $(r) $(n)\n");
				// ソケットが切れたっぽい
				return 0;

			} else {
stderr.printf(@"recv ERR: $(r) $(n)\n");
				return -1;
				throw new GLib.IOError.FAILED(r.to_string());
			}
			} while (true);
		}

	}


	// CurlOutputStream は未実装

	// Curl クラス
	public class Curl
	{
		// 接続先 URI
		public ParsedUri uri;

		// Curl handle
		private EasyHandle Handle;

		// ストリーム
		private CurlInputStream input_stream;

		public void Open()
		{
			Handle = new EasyHandle();

			input_stream = new CurlInputStream();
			input_stream.Handle = Handle;
		}

		public void Close()
		{
			Handle.cleanup();

			Handle = null;

			input_stream = null;
		}

		public Native.Curl.Code Perform()
		{
			
			// URI セット
			Handle.setopt(Option.URL, uri.to_string());

			// recv を使うために CONNECT_ONLY に 1 をセット
			Handle.setopt(Option.CONNECT_ONLY, 1);

			// 実行
			var r = Handle.perform();
			return r;
		}

		public Native.Curl.Code SendString(string s)
		{
			size_t n;

			var r = Handle.send(s, s.length, out n);
			return r;
		}

		public CurlInputStream get_input_stream()
		{
			return input_stream;
		}

	}
}

