using Native.Curl;

namespace ULib
{
	public class Curl
		: InputStream
	{
		// Curl ハンドル
		private MultiHandle MH;
		private EasyHandle EH;

		// 内部バッファ
		private Queue<char> queue;

		// HTTP ヘッダ用の Curl.SList
		// コネクション期間と等しい寿命が必要
		private Native.Curl.SList list = null;

		// パース後の URI
		public ParsedUri Uri;

		// リクエスト時にサーバへ送る追加のヘッダ
		// Host: はこちらで生成するので呼び出し側が指定しないでください。
		public Dictionary<string, string> SendHeaders;

		// コネクションに使用するプロトコルファミリ
		// IPv4/IPv6 only にしたい場合はコンストラクタ後に指定?
		public SocketFamily Family;

		public Curl(string uri)
		{
			MH = new MultiHandle();

			queue = new Queue<char>();

			Uri = ParsedUri.Parse(uri);

			SendHeaders = new Dictionary<string, string>();
		}

		virtual ~Curl()
		{
stderr.printf("Curl destruct\n");
			MH.remove_handle(EH);
			EH = null;
			MH = null;
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
			// TODO: GET/POST
			var r = Connect(method);
			if (r != MultiCode.OK) {
				throw new GLib.IOError.FAILED(@"Curl Connect Failed: $(r)");
			}
			return new DataInputStream(this);
		}

		// 接続処理を開始します。
		private MultiCode Connect(string method) throws Error
		{
			EH = new EasyHandle();

			string path = (method == "POST") ? Uri.SchemeAuthority() + Uri.Path + Uri.Fragment : Uri.to_string();

stderr.printf(@"path=$(path)\n");

			// URI セット
			EH.setopt(Option.URL, path);

			// ヘッダのセット
			if (list != null) {
				list.free_all();
			}
			// TODO: User-Agent とか Connection close とか。
			foreach (KeyValuePair<string, string> h in SendHeaders) {
				list = Native.Curl.SList.append((owned)list, @"$(h.Key): $(h.Value)");
			}
			list = Native.Curl.SList.append((owned)list, @"connection: close");
			list = Native.Curl.SList.append((owned)list, @"User-Agent: Curl.vala");
			var r = EH.setopt(Option.HTTPHEADER, list);
			if (r != Code.OK) {
				throw new GLib.IOError.FAILED(@"HTTPHEADER: $(r)");
			}

			// POST データ
			if (method == "POST") {
				var body = Uri.Query + "\n";
				EH.setopt(Option.COPYPOSTFIELDS, body);
			}

			// コールバックの設定
			// WRITEDATA はダミーをセット。これで stdout には出なくなる
stderr.printf("addr of queue=%p\n", queue);

			EH.setopt(Option.WRITEDATA, queue);

			// curl からみて write するときのコールバック関数を設定。
			EH.setopt(Option.WRITEFUNCTION, writecallback);

			return MH.add_handle(EH);
		}

		// curl からのコールバックです。
		private static size_t writecallback(char* buffer, size_t size, size_t nitems, void* x_queue)
		{
stderr.printf(@"writecallback size=$(size) nitems=$(nitems) addrof x_queue=%p\n", x_queue);

			unowned Queue<char> q = (Queue<char>) x_queue;

			// 内部バッファに蓄積します。
			var len = size * nitems;
			for (int i = 0; i < len ; i++) {
				q.push_tail(buffer[i]);
			}
stderr.printf(@"writecallback $(len)\n");
			return len;
		}

		// ----- InputStream 実装

		public override bool close(Cancellable? cancellable = null) throws GLib.IOError
		{
			//MH.cleanup();
			// TODO: ほかのハンドルも。

			return true;
		}

		// 内部メモリストリームから recv します。
		public override ssize_t read(uint8[] buffer, Cancellable? cancellable = null) throws GLib.IOError
		{
stderr.printf("read\n");
			do {
				int running_handles = 1;
//stderr.printf("read 1\n");
				var r = MH.perform(ref running_handles);
//stderr.printf("read 2\n");
				if (r == MultiCode.OK) {
					if (queue.length == 0) {
						if (running_handles == 0) {
							// EOF
stderr.printf("read EOF\n");
							return 0;
						} else {
							GLib.Thread.usleep(10000);
							continue;
						}
					}

					int n = (int) queue.length;
					if (n > buffer.length) n = buffer.length;

					for (int i = 0; i < n; i++) {
						buffer[i] = queue.pop_head();
					}

stderr.printf(@"recv OK: $(r) $(n)\n");
					return (ssize_t)n;
				} else {
stderr.printf(@"recv ERR: $(r)\n");
					return -1;
					//throw new GLib.IOError.FAILED(r.to_string());
				}
			} while (true);
		}
	}
}
