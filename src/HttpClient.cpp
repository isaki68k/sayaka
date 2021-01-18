#include "HttpClient.h"
#include "ChunkedInputStream.h"
#include "StringUtil.h"
#include <err.h>
#include <sys/socket.h>

// コンストラクタ
HttpClient::HttpClient()
{
	family = AF_UNSPEC;
}

// uri をターゲットにして初期化する
bool
HttpClient::Init(const Diag& diag_, const std::string& uri_)
{
	diag = diag_;

	if (mtls.Init() == false) {
		warnx("HttpClient.Init: mTLSHandle.Init failed");
		return false;
	}

	Uri = ParsedUri::Parse(uri_);
	diag.Debug("Uri=|%s|", Uri.to_string().c_str());

	return true;
}

// uri へ GET/POST して、ストリームを返す。
// (GET と POST の共通部)
InputStream *
HttpClient::Act(const std::string& method)
{
	diag.Trace("%s()", method.c_str());
	InputStream *dIn = NULL;

	for (;;) {
		Connect();

		SendRequest(method);

		dIn = new mTLSInputStream(&mtls, diag);

		ReceiveHeader(dIn);

		if (300 <= ResultCode && ResultCode < 400) {
			Close();
			auto location = GetHeader(RecvHeaders, "Location");
			diag.Debug("Redirect to %s", location.c_str());
			if (!location.empty()) {
				auto newUri = ParsedUri::Parse(location);
				if (!newUri.Scheme.empty()) {
					// Scheme があればフルURIとみなす
					Uri = ParsedUri::Parse(location);
				} else {
					// そうでなければ相対パスとみなす
					Uri.Path = newUri.Path;
					Uri.Query = newUri.Query;
					Uri.Fragment = newUri.Fragment;
				}
				diag.Debug("%s", Uri.to_string().c_str());
				continue;
			}
		} else if (ResultCode >= 400) {
			// XXX この場合でも本文にエラーのヒントがある場合があるので、
			// 本文を読んでデバッグ表示だけでもしておきたいのだが。
			errno = ENOTCONN;
			return NULL;
		}
		break;
	}

	InputStream *stream;
	auto transfer_encoding = GetHeader(RecvHeaders, "Transfer-Encoding");
	if (transfer_encoding == "chunked") {
		// チャンク
		diag.Debug("use ChunkedInputStream");
		stream = new ChunkedInputStream(dIn, diag);
	} else {
		stream = dIn;
	}

	return stream;
}

// GET/POST リクエストを発行する
void
HttpClient::SendRequest(const std::string& method)
{
	std::string sb;

	std::string path = (method == "POST") ? Uri.Path : Uri.PQF();
	sb += string_format("%s %s HTTP/1.1\r\n", method.c_str(), path.c_str());

	for (const auto& h : SendHeaders) {
		sb += h.c_str();
		sb += "\r\n";
	}
	sb += "Connection: close\r\n";
	sb += string_format("Host: %s\r\n", Uri.Host.c_str());

	// User-Agent は SHOULD
	sb += "User-Agent: HttpClient\r\n";

	if (method == "POST") {
		sb += "Content-Type: application/x-www-form-urlencoded\r\n";
		sb += string_format("Content-Length: %zd\r\n", Uri.Query.length());
		sb += "\r\n";
		sb += Uri.Query;
	} else {
		sb += "\r\n";
	}

	diag.Debug("Request %s\n%s", method.c_str(), sb.c_str());

	mtls.Write(sb.c_str(), sb.length());
	mtls.Shutdown(SHUT_WR);

	diag.Trace("SendRequest() request sent");
}

// ヘッダを受信する
bool
HttpClient::ReceiveHeader(InputStream *dIn)
{
	size_t r;

	diag.Trace("ReceiveHeader()");

	RecvHeaders.clear();

	// 1行目は応答
	r = dIn->ReadLine(&ResultLine);
	if (r <= 0) {
		return false;
	}
	if (ResultLine.empty()) {
		return false;
	}
	diag.Debug("HEADER |%s|", ResultLine.c_str());

	auto proto_arg = Split2(ResultLine, " ");
	auto protocol = proto_arg.first;
	auto arg = proto_arg.second;
	if (protocol == "HTTP/1.1" || protocol == "HTTP/1.0") {
		auto code_msg = Split2(arg, " ");
		auto code = code_msg.first;
		ResultCode = stoi(code);
		diag.Debug("ResultCode=%d", ResultCode);
	}

	// 2行目以降のヘッダを読み込む
	// XXX 1000行で諦める
	for (int i = 0; i < 1000; i++) {
		std::string s;
		r = dIn->ReadLine(&s);
		if (r <= 0) {
			return false;
		}
		if (s.empty()) {
			return false;
		}
		diag.Debug("HEADER |%s|", s.c_str());

		// まず行継続の処理
		if (s[0] == ' ') {
			auto& prev = RecvHeaders.back();
			prev += Chomp(s);
			continue;
		}
		// その後で改行等を削って、空行ならここで終了
		s = Chomp(s);
		if (s.empty()) {
			break;
		}
		RecvHeaders.emplace_back(s);
	}
	return false;
}

// 指定のヘッダ配列から指定のヘッダを検索してボディを返す。
// 指定されたヘッダが存在しない場合は "" を返す。
std::string
HttpClient::GetHeader(const std::vector<std::string>& header,
	const std::string& key_) const
{
	auto key = StringToLower(key_);
	for (const auto& h : header) {
		auto [ k, v ] = Split2(h, ":");
		k = StringToLower(k);

		if (k == key) {
			return Chomp(v);
		}
	}
	return "";
}

// uri へ接続する。
bool
HttpClient::Connect()
{
	mtls.SetDebugLevel(diag.GetLevel());

#if 0
	// 透過プロキシ(?)設定があれば対応
	std::string proxyTarget;
	ParsedUri proxyUri;
	if (!ProxyMap.empty()) {
		auto map = ProxyMap.Split("=");
		proxyTarget = map.first;
		proxyUri = ParsedUri::Parse(map.second);

		// 宛先がプロキシサーバのアドレスなら、差し替える
		if (Uri.Host == proxyTarget) {
			Uri = proxyUri;
		}
	}
#endif

	// デフォルトポートの処理
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
		mtls.UseSSL(true);
	}
	if (Ciphers == "RSA") {
		// XXX RSA 専用
		mtls.UseRSA();
	}
	diag.Trace("Connect(): %s", Uri.to_string());
	if (mtls.Connect(Uri.Host, Uri.Port) != 0) {
		diag.Debug("mTLSHandle.Connect failed");
		return false;
	}

	return true;
}

// 接続を閉じる
void
HttpClient::Close()
{
	diag.Trace("Close");
	// nothing to do ?
}


//
// mTLS stream
//

// コンストラクタ
mTLSInputStream::mTLSInputStream(mTLSHandle *mtls_, const Diag& diag_)
	: diag(diag_)
{
	mtls = mtls_;
}

// デストラクタ
mTLSInputStream::~mTLSInputStream()
{
}

// 読み出し
ssize_t
mTLSInputStream::Read(char *buf, size_t buflen)
{
	int r;

	r = mtls->Read(buf, buflen);
	diag.Trace("read=%d", r);

	return (ssize_t)r;
}
