#pragma once

#include "mbedtls/net_sockets.h"
#include "mbedtls/ssl.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/certs.h"
#include <sys/time.h>
#include <string>

class mTLSHandle
{
 public:
	mTLSHandle();
	~mTLSHandle();

	// ディスクリプタを持っているのでコピーコンストラクタを禁止する。
	mTLSHandle(const mTLSHandle&) = delete;
	mTLSHandle& operator=(const mTLSHandle&) = delete;

	// 初期化
	bool Init();

	// HTTPS を使うかどうかを設定する。
	// Connect() より先に設定しておくこと。
	void UseSSL(bool value) { usessl = value; }

	// 接続に使用する CipherSuites を RSA_WITH_AES_128_CBC_SHA に限定する。
	// Connect() より先に設定しておくこと。
	// XXX どういう API にすべきか
	void UseRSA();
	
	// 接続タイムアウトを設定する。
	// Connect() より先に設定しておくこと。
	void SetTimeout(int timeout) { connect_timeout = timeout; }

	// mbedTLS ライブラリのデバッグレベルを指定する。
	void SetDebugLevel(int level);

	// 接続する
	bool Connect(const std::string& hostname, const std::string& servname) {
		return Connect(hostname.c_str(), servname.c_str());
	}
	bool Connect(const char *hostname, const char *servname);

	// クローズする
	void Close();

	// shutdown する
	int Shutdown(int how);

	// 読み書き
	int Read(void *buf, int len);
	int Write(const void *buf, int len);

 public:
	bool initialized {};
	bool usessl {};
	int connect_timeout {};		// [msec]

	// 内部コンテキスト
	mbedtls_net_context net {};
	mbedtls_ssl_context ssl {};
	mbedtls_ssl_config conf {};

 private:
	// mbedTLS のエラーコードを文字列にして返す
	// (static バッファを使っていることに注意)
	char errbuf[128] {};
	const char *errmsg(int code);
};
