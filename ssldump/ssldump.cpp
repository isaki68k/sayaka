#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pcap/pcap.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include "stream.h"

#define TCPDUMP_MAGIC	(0xa1b2c3d4)

typedef struct {
	uint32_t tv_sec;
	uint32_t tv_usec;
	uint32_t caplen;
	uint32_t len;
} pcap_pkthdr32;

void dump_ip(const pcap_pkthdr32 *, uint8_t *, int);
void dump_tcp(const pcap_pkthdr32 *, const ip *, const tcphdr *);
void dump_ssl(const uint8_t *, int);
void dump_TLSPlaintext(MemoryStream *);
void dump_TLS_handshake(MemoryStream *);
void dump_TLS_both_hello(const char *, MemoryStream *);
void hexdump(const void *, int);

int verbose;

int
main(int ac, char *av[])
{
	pcap_file_header file_hdr;
	pcap_pkthdr32 hdr;
	const char *filename;
	int linklen;
	int fd;
	int r;
	int c;

	while ((c = getopt(ac, av, "v")) != -1) {
		switch (c) {
		 case 'v':
			verbose++;
			break;
		}
	}
	ac -= optind;
	av += optind;

	if (ac < 1) {
		errx(1, "usage:...\n");
	}
	filename = av[0];

	fd = open(filename, O_RDONLY);
	if (fd == -1) {
		err(1, "fopen: %s", filename);
	}

	/* pcap ファイルヘッダ */
	r = read(fd, &file_hdr, sizeof(file_hdr));
	if (r == -1) {
		err(1, "read(file_hdr)");
	}

	/* 一応ちょっと照合 */
	/* XXX エンディアンは? */
	if (file_hdr.magic != TCPDUMP_MAGIC) {
		errx(1, "not pcap file (bad magic): %s", filename);
	}
	switch (file_hdr.linktype) {
	 case DLT_NULL:		// ループバックインタフェースなら4バイト
		linklen = 4;
		break;
	 case DLT_EN10MB:	// イーサネットなら 14バイト
		linklen = 14;
		break;
	 default:
		errx(1, "unsupported link layer %d\n", file_hdr.linktype);
	}

	for (;;) {
		r = read(fd, &hdr, sizeof(hdr));
		if (r == 0) {
			break;
		}
		if (r == -1) {
			errx(1, "pcap header too short");
		}
		if (verbose) {
			char timebuf[32];
			time_t t = hdr.tv_sec;
			struct tm *tm = localtime(&t);
			strftime(timebuf, sizeof(timebuf), "%F %T", tm);
			printf("\n");
			printf("hdr timestamp: %s.%06d\n", timebuf, hdr.tv_usec);
			printf("hdr caplen = %x\n", hdr.caplen);
			printf("hdr pktlen = %x\n", hdr.len);
		}

		/* リンクレイヤを読み捨てる? */
		uint8_t link[linklen];
		r = read(fd, link, linklen);
		if (r < 1) {
			errx(1, "unexpected EOF?");
		}

		/* IP レイヤ以降を読み込む */
		uint8_t *buf;
		int iplen = hdr.caplen - linklen;
		buf = (uint8_t *)malloc(iplen);
		if (buf == NULL) {
			errx(1, "out of memory");
		}
		r = read(fd, buf, iplen);
		if (r < 1) {
			errx(1, "unexpected EOF?");
		}
		if (verbose) {
			hexdump(buf, iplen);
		}

		dump_ip(&hdr, buf, hdr.caplen);

		free(buf);
	}

	close(fd);
	return 0;
}

void
dump_ip(const pcap_pkthdr32 *hdr, uint8_t *buf, int buflen)
{
	struct ip *ip;

	/* time */
	char timebuf[32];
	time_t t = hdr->tv_sec;
	struct tm *tm = localtime(&t);
	strftime(timebuf, sizeof(timebuf), "%T", tm);
	printf("[%s.%06d] ", timebuf, hdr->tv_usec);

	ip = (struct ip*)buf;
	if (ip->ip_v == 4) {
		if (ip->ip_p == IPPROTO_TCP) {
			struct tcphdr *th = (struct tcphdr *)((uint8_t *)ip + ip->ip_hl*4);
			dump_tcp(hdr, ip, th);
		} else {
			printf("%s -> ", inet_ntoa(ip->ip_src));
			printf("%s PROTO=%d\n", inet_ntoa(ip->ip_dst), ip->ip_p);
		}
	} else if (ip->ip_v == 6) {
		printf("ipv6 not supported\n");
		exit(1);

	} else {
		printf("unknown IP packet\n");
		exit(1);

	}
	return;
}

void
dump_tcp(const pcap_pkthdr32 *hdr, const ip *ip, const tcphdr *th)
{
	// ここでようやく IP:port を表示
	printf("%s:%d", inet_ntoa(ip->ip_src), ntohs(th->th_sport));
	printf(" -> ");
	printf("%s:%d", inet_ntoa(ip->ip_dst), ntohs(th->th_dport));

	// フラグ (SYN/ACK/FIN だけ)
	if (th->th_flags != 0) {
		if ((th->th_flags & TH_SYN) != 0) {
			printf(" SYN");
		}
		if ((th->th_flags & TH_ACK) != 0) {
			printf(" ACK");
		}
		if ((th->th_flags & TH_FIN) != 0) {
			printf(" FIN");
		}
	}
	printf("\n");

	// TCPペイロードとその長さ
	const uint8_t *tcp = (const uint8_t *)(((uint8_t *)th) + th->th_off * 4);

	int tcplen;
	tcplen = ntohs(ip->ip_len) - (ip->ip_hl * 4);
	tcplen = tcplen - (th->th_off * 4);

	// TCP ペイロードなし (3way ハンドシェークとか) ならここで終わり
	if (tcplen == 0) {
		return;
	}

	// そうでなければ上位レイヤ
	dump_ssl(tcp, tcplen);
}

void
dump_ssl(const uint8_t *data, int tcplen)
{
	hexdump(data, tcplen);

	MemoryStream ms(data, tcplen);

	// ルートがこれかどうか分からんけどとりあえず
	dump_TLSPlaintext(&ms);

	printf("\n");
}

/* ここから本題 */

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;

// グローバル関数?
uint8 Read8(MemoryStream *stream)
{
	return stream->GetCh();
}

uint16 Read16(MemoryStream *stream)
{
	char buf[2];
	stream->Read(buf, sizeof(buf));
	return ntohs(*(uint16 *)&buf[0]);
}

struct VNPair {
	int value;
	const char *name;
};

const char *Lookup(struct VNPair *table, int value)
{
	for (; table->name != NULL; table++) {
		if (value == table->value) {
			return table->name;
		}
	}
	return NULL;
}

class BaseObject {
 public:
	virtual bool Read(MemoryStream *) {}
//	virtual bool Write(OutputStream *) {}
};

class ContentType : public BaseObject
{
 public:
	uint8 value;

 public:
	enum {
		change_cipher_spec = 20,
		alert = 21,
		handshake = 22,
		application_data = 23,
	};

 private:
	static struct VNPair Names[];

 public:
	ContentType()
	{
		value = 0;
	}

	bool Read(MemoryStream *stream)
	{
		value = stream->GetCh();
		if (value < 0) {
			return false;
		}
		return true;
	}

	const char *to_string()
	{
		static char buf[32];
		const char *name = Lookup(Names, value);
		if (name) {
			snprintf(buf, sizeof(buf), "%s(%d)", name, value);
		} else {
			snprintf(buf, sizeof(buf), "%d", value);
		}
		return buf;
	}
};

struct VNPair ContentType::Names[] = {
	{ ContentType::change_cipher_spec, "change_cipher_spec" },
	{ ContentType::alert, "alert" },
	{ ContentType::handshake, "handshake" },
	{ ContentType::application_data, "application_data" },
	{ 0, NULL },
};

class ProtocolVersion : public BaseObject
{
 private:
	uint8 major;
	uint8 minor;

 public:
	ProtocolVersion()
	{
		major = 0;
		minor = 0;
	}

	bool Read(MemoryStream *stream)
	{
		// TODO: エラーチェック
		major = stream->GetCh();
		minor = stream->GetCh();
		return true;
	}

	const char *to_string() {
		static char buf[10];
		int n = (major << 8) + minor;
		switch (n) {
		 case 0x0301:
			return "TLS1.0";
		 case 0x0303:
			return "TLS1.2";
		 default:
			snprintf(buf, sizeof(buf), "0x%04x", n);
			return buf;
		}
	}
};

void
dump_TLSPlaintext(MemoryStream *stream)
{
	ContentType type;
	ProtocolVersion version;
	uint16 length;
	//opaque fragment[TLSPlaintext.length];

	type.Read(stream);
	version.Read(stream);
	length = Read16(stream);

	uint8 fragment[length];
	stream->Read(fragment, length);
	MemoryStream fs(fragment, length);

	printf("TLSPlaintext: ContentType=%s, version=%s Length=%d\n",
		type.to_string(),
		version.to_string(),
		length);

	if (type.value == ContentType::handshake) {
		dump_TLS_handshake(&fs);
	} else {
		printf("TLSPlaintext: ContentType %s not supported\n",
			type.to_string());
	}
}

class Uint24 : public BaseObject
{
 public:
	int value;

 public:
	Uint24()
	{
		value = 0;
	}

	bool Read(MemoryStream *stream)
	{
		uint8 h, m, l;
		h = stream->GetCh();
		m = stream->GetCh();
		l = stream->GetCh();
		value = (h << 16) + (m << 8) + l;
		return true;
	}
};

class HandshakeType : public BaseObject
{
 public:
	uint8 value;

 public:
	enum {
		hello_request = 0,
		client_hello = 1,
		server_hello = 2,
		certificate = 11,
		server_key_exchange  = 12,
		certificate_request = 13,
		server_hello_done = 14,
		certificate_verify = 15,
		client_key_exchange = 16,
		finished = 20,
	};

	static struct VNPair Names[];

 public:
	HandshakeType()
	{
		value = 0;
	}

	bool Read(MemoryStream *stream)
	{
		value = stream->GetCh();
		if (value < 0) {
			return false;
		}
		return true;
	}

	const char *to_string()
	{
		static char buf[32];
		const char *name = Lookup(Names, value);
		if (name) {
			snprintf(buf, sizeof(buf), "%s(%d)", name, value);
		} else {
			snprintf(buf, sizeof(buf), "%d", value);
		}
		return buf;
	}
};

struct VNPair HandshakeType::Names[] = {
	{ HandshakeType::hello_request, "hello_request" },
	{ HandshakeType::client_hello, "client_hello" },
	{ HandshakeType::server_hello, "server_hello" },
	{ HandshakeType::certificate, "certificate" },
	{ HandshakeType::server_key_exchange , "server_key_exchange " },
	{ HandshakeType::certificate_request, "certificate_request" },
	{ HandshakeType::server_hello_done, "server_hello_done" },
	{ HandshakeType::certificate_verify, "certificate_verify" },
	{ HandshakeType::client_key_exchange, "client_key_exchange" },
	{ HandshakeType::finished, "finished" },
	{ 0, NULL },
};

void
dump_TLS_handshake(MemoryStream *stream)
{
	HandshakeType msg_type;
	Uint24 length;

	msg_type.Read(stream);
	length.Read(stream);

	printf("Handshake: msg_type=%s, length=%d\n",
		msg_type.to_string(),
		length.value);

	switch (msg_type.value) {
	 case HandshakeType::client_hello:
		dump_TLS_both_hello("Client", stream);
		break;
	 case HandshakeType::server_hello:
		dump_TLS_both_hello("Server", stream);
		break;
	 default:
		printf("Handshake: HandshakeType %s not supported\n",
			msg_type.to_string());
		break;
	}
}

class Random : public BaseObject
{
 public:
	union {
		struct {
			uint32 gmt_unix_time;
			uint8 random_bytes[28];
		};
		uint8 data[32];
	};

 public:
	Random()
	{
		memset(data, 0, sizeof(data));
	}

	bool Read(MemoryStream *stream)
	{
		return stream->Read(data, sizeof(data));
	}

	const char *to_string() {
		static char buf[66];
		int i;
		for (i = 0; i < sizeof(data); i++) {
			sprintf(buf + i * 2, "%02x", data[i]);
		}
		buf[i * 2] = '\0';
		return buf;
	}
};

class VLField : public BaseObject
{
 public:
	VLField(int l)
	{
		lenbytes = l;
	}

	bool ReadLength(MemoryStream *stream)
	{
		uint8 n;

		length = 0;
		for (int i = 0; i < lenbytes; i++) {
			n = stream->GetCh();
			length = (length << 8) + n;
		}
		return true;
	}

 protected:
	int length;

 private:
	int lenbytes;

};

class SessionID : public VLField
{
 public:
	uint8 data[32];

 public:
	SessionID()
		: VLField(1)
	{
	}

	bool Read(MemoryStream *stream)
	{
		ReadLength(stream);
		return stream->Read(data, length);
	}

	const char *to_string() {
		static char buf[80];
		int n = sprintf(buf, "<%d>", length);
		int i;
		for (i = 0; i < length; i++) {
			sprintf(buf + n + i * 2, "%02x", data[i]);
		}
		buf[n + i] = '\0';
		return buf;
	}
};

class CipherSuite : public BaseObject
{
 private:
	uint8 value[2];

	static struct VNPair Names[];

 public:
	CipherSuite()
	{
		value[0] = 0;
		value[1] = 0;
	}

	bool Read(MemoryStream *stream)
	{
		value[0] = stream->GetCh();
		value[1] = stream->GetCh();
		return true;
	}

	const char *to_string() {
		static char buf[64];
		int n = (value[0] << 8) + value[1];
		const char *name = Lookup(Names, n);
		if (name) {
			snprintf(buf, sizeof(buf), "0x%04x %s", n, name);
		} else {
			snprintf(buf, sizeof(buf), "0x%04x", n);
		}
		return buf;
	}
};

struct VNPair CipherSuite::Names[] = {
	{ 0x0000, "NULL" },
	{ 0x0001, "RSA_MD5" },
	{ 0x0002, "RSA_SHA" },
	{ 0x003b, "RSA_SHA256" },
	{ 0x0004, "RSA_RC4_128_MD5" },
	{ 0x0005, "RSA_RC4_128_SHA" },
	{ 0x000a, "RSA_3DES_EDE_CBC_SHA" },
	{ 0x002f, "RSA_AES_128_CBC_SHA" },
	{ 0x0000, NULL },
};

class CipherSuites : public VLField
{
 public:
	CipherSuites()
		: VLField(2)
	{
	}

	bool Read(MemoryStream *stream)
	{
		ReadLength(stream);
		cs.clear();
		MemoryStream *ms = stream->ReadSlice(length);
		while (!ms->Eof()) {
			CipherSuite cs;
			cs.Read(ms);
			list.push_back(cs);
		}
		delete ms;
		return true;
	}

 public:
	std::vector<CipherSuite> list;

}

void
dump_TLS_both_hello(const char *scname, MemoryStream *stream)
{
	ProtocolVersion version;
	Random random;
	uint8 variables[0];
	SessionID session_id;
	CipherSuites cipher_suites;
	//Vector<CompressionMethod> compression_methods;
	//uint8 Extension[0];

	version.Read(stream);
	random.Read(stream);
	session_id.Read(stream);
	cipher_suites.Read(stream);

	printf("%sHello: version=%s random=%s\n",
		scname,
		version.to_string(),
		random.to_string());
	printf("%sHello: session_id%s\n",
		scname,
		session_id.to_string());

	int count = cipher_suites.list.size());
	printf("%sHello: cipher_suites<%d>\n", count);
	for (cipher_suites.list.front()

#if 0
	// CipherSuite sipher_suites<2..2^16-2>
	// 最大65534バイトなので、長さフィールドは2バイト
	{
		uint16 len = ntohs(*(uint16 *)p);
		p += 2;
		// 要素数を求める
		int nelem = len / sizeof(CipherSuite);
		printf("ClientHello: cipher_suites<%d>\n", nelem);
		CipherSuite *cs = (CipherSuite *)p;
		for (int i = 0; i < nelem; i++) {
			printf("\t[%d] %s\n", i, cs->to_string());
			cs++;
		}
		p = (uint8 *)cs;
	}

	// CompressionMethod compression_methods<1..2^8-1>
	{
		uint8 len = *p++;
		printf("ClientHello: compression_methods<%d> ", len);
		for (int i = 0; i < len; i++) {
			printf("%02x", *p++);
		}
		printf("\n");
	}

	// Extension extensions<0..2^16-1> if extension_present
	{
		uint16 len = ntohs(*(uint16 *)p);
		p += 2;
		// 要素が可変長なので事前に要素数は求められない
		printf("ClientHello: extensions<%d>\n", len);
		for (uint8 *s = p; s < p + len; ) {
			uint16 extype = ntohs(*(uint16 *)s);
			s += 2;
			printf("\tType=0x%04x,", extype);
			uint16 exlen = ntohs(*(uint16 *)s);
			printf("<%d> ", exlen);
			s += 2;
			for (int j = 0; j < exlen; j++) {
				printf("%02x", *s++);
			}
			printf("\n");
		}
	}

#endif

}

// デバッグ用のバイナリダンプ
void
hexdump(const void *p, int len)
{
	int i;
	const uint8_t *buf = (const uint8_t *)p;
	for (i = 0; i < len; i++) {
		if (i % 16 == 0)
			printf("%04x:", i);
		printf(" %02x", buf[i]);
		if (i % 16 == 7)
			printf(" ");
		if (i % 16 == 15)
			printf("\n");
	}
	if (i % 16 != 0) {
		printf("\n");
	}
}
