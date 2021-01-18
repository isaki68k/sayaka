#pragma once

#include "ChunkedInputStream.h"
#include "Diag.h"
#include "mtls.h"
#include "ParsedUri.h"
#include "StreamBase.h"
#include <memory>
#include <string>
#include <vector>
#include <sys/socket.h>

class mTLSInputStream : public InputStream
{
 public:
	mTLSInputStream(mTLSHandle *mtls, const Diag& diag);
	virtual ~mTLSInputStream() override;

	ssize_t NativeRead(void *buf, size_t buflen) override;

 private:
	mTLSHandle *mtls {};

	Diag diag {};
};

class HttpClient
{
 public:
	// コンストラクタ
	HttpClient();

	// uri をターゲットにして初期化する
	bool Init(const Diag& diag, const std::string& uri);

	// uri から GET して、ストリームを返す
	InputStream *GET() {
		return Act("GET");
	}

	// uri へ POST して、ストリームを返す
	InputStream *POST() {
		return Act("POST");
	}

	// uri へ GET/POST して、ストリームを返す
	// GET と POST の共通部。
	InputStream *Act(const std::string& method);

	// 接続を閉じる
	void Close();

	// 送信ヘッダを追加する。
	// s は改行を含まない HTTP ヘッダ1行の形式。
	void AddHeader(const std::string& s) {
		SendHeaders.push_back(s);
	}

	// ヘッダ配列から指定のヘッダを検索してボディを返す。
	// 指定されたヘッダが存在しない場合は "" を返す。
	std::string GetHeader(const std::vector<std::string>& header,
		const std::string& key) const;

	// Ciphers を設定する
	void SetCiphers(const std::string& ciphers_) {
		Ciphers = ciphers_;
	}

	// タイムアウトを設定する
	void SetTimeout(int timeout) {
		mtls.SetTimeout(timeout);
	}

	// パース後の URI
	ParsedUri Uri {};

	// リクエスト時にサーバに送る追加のヘッダ
	// Host: はこちらで生成するので呼び出し側は指定しないこと。
	std::vector<std::string> SendHeaders {};

	// 受け取ったヘッダ
	std::vector<std::string> RecvHeaders {};

	// 受け取った応答行
	std::string ResultLine {};

	// 受け取った応答コード
	int ResultCode {};

	// コネクションに使用するプロトコルファミリ
	// XXX ただし mbedTLS 版は API が指定に対応していないので、未対応
	int family {};

	// 使用する CipherSuites
	// ただし ""(デフォルト) と "RSA" しか対応していない
	std::string Ciphers {};

 private:
	// GET/POST リクエストを発行する
	void SendRequest(const std::string& method);

	// ヘッダを受信する
	bool ReceiveHeader(InputStream *stream);

	// 接続する
	bool Connect();

	// mTLS ハンドル
	mTLSHandle mtls {};

	// mTLS ストリーム
	std::unique_ptr<mTLSInputStream> mstream {};

	// チャンク用
	std::unique_ptr<ChunkedInputStream> chunk_stream {};

	Diag diag {};
};
