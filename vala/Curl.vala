/*
 * Copyright (C) 2016 Y.Sugahara (moveccr)
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

using Native.Curl;

namespace ULib
{
	public class Curl
		: InputStream, IHttpClient
	{
		private Diag diag = new Diag("Curl");

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
		public List<string> SendHeaders;

		// コネクションに使用するプロトコルファミリ
		// IPv4/IPv6 only にしたい場合はコンストラクタ後に指定?
		public SocketFamily Family;

		public Curl(string uri)
		{
			EH = new EasyHandle();
			MH = new MultiHandle();

			queue = new Queue<char>();

			Uri = ParsedUri.Parse(uri);

			SendHeaders = new List<string>();
		}

		virtual ~Curl()
		{
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

		public void AddHeader(string s)
		{
			SendHeaders.append(s);
		}

		// 接続処理を開始します。
		private MultiCode Connect(string method) throws Error
		{
			// XXX とりあえず連動させておく
			if (Diag.global_debug) {
				EH.setopt(Option.VERBOSE, (long)1);
			}

			// プロトコル
			switch (Family) {
			 case SocketFamily.IPV4:
				EH.setopt(Option.IPRESOLVE, IPRESOLVE_V4);
				break;
			 case SocketFamily.IPV6:
				EH.setopt(Option.IPRESOLVE, IPRESOLVE_V6);
				break;
			 default:
				break;
			}

			// URI セット
			string path = (method == "POST")
				? Uri.SchemeAuthority() + Uri.Path + Uri.Fragment
				: Uri.to_string();
			EH.setopt(Option.URL, path);

			// 証明書のベリファイとか無視。セキュリティガバガバ
			EH.setopt(Option.SSL_VERIFYPEER, false);

			// ヘッダのセット
			if (list != null) {
				list.free_all();
			}
			// TODO: User-Agent とか Connection close とか。
			foreach (string h in SendHeaders) {
				list = Native.Curl.SList.append((owned)list, h);
			}
			list = Native.Curl.SList.append((owned)list, @"Connection: close");
			list = Native.Curl.SList.append((owned)list,
				@"User-Agent: Curl.vala");
			var r = EH.setopt(Option.HTTPHEADER, list);
			if (r != Code.OK) {
				throw new GLib.IOError.FAILED(@"HTTPHEADER: $(r)");
			}

			// POST データ
			if (method == "POST") {
				EH.setopt(Option.COPYPOSTFIELDS, Uri.Query);
			}

			// コールバックの設定
			// curl からみて write するときのコールバック関数を設定。
			EH.setopt(Option.WRITEFUNCTION, writecallback);
			EH.setopt(Option.WRITEDATA, queue);

			return MH.add_handle(EH);
		}

		// curl からのコールバックです。
		private static size_t writecallback(char* buffer, size_t size,
			size_t nitems, void* x_queue)
		{
			unowned Queue<char> q = (Queue<char>) x_queue;

			// 内部バッファに蓄積します。
			var len = size * nitems;
			for (int i = 0; i < len ; i++) {
				q.push_tail(buffer[i]);
			}
			return len;
		}

		// Ciphers を設定します。
		public void SetCiphers(string ciphers)
		{
			if (EH != null) {
				EH.setopt(Option.SSL_CIPHER_LIST, ciphers);
			}
		}

		// ----- InputStream 実装

		public override bool close(Cancellable? cancellable = null)
			throws GLib.IOError
		{
			//MH.cleanup();
			// TODO: ほかのハンドルも。

			return true;
		}

		// 内部メモリストリームから recv します。
		public override ssize_t read(uint8[] buffer,
			Cancellable? cancellable = null) throws GLib.IOError
		{
			while (true) {
				int running_handles = 1;
				var r = MH.perform(ref running_handles);
				if (r != MultiCode.OK) {
					//throw new GLib.IOError.FAILED(r.to_string());
					return -1;
				}

				if (queue.length == 0) {
					if (running_handles == 0) {
						// EOF
						return 0;
					} else {
						GLib.Thread.usleep(10000);
						continue;
					}
				}

				int n = int.min((int)queue.length, buffer.length);
				for (int i = 0; i < n; i++) {
					buffer[i] = queue.pop_head();
				}

				return (ssize_t)n;
			}
		}
	}
}
