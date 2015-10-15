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

#define TCPDUMP_MAGIC	(0xa1b2c3d4)

typedef struct {
	uint32_t tv_sec;
	uint32_t tv_usec;
	uint32_t caplen;
	uint32_t len;
} pcap_pkthdr32;

void dump_ip(const pcap_pkthdr32 *, uint8_t *, int);
void dump_tcp(const pcap_pkthdr32 *, const ip *, const tcphdr *);
void hexdump(const void *, int);

int debug;

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

	while ((c = getopt(ac, av, "d")) != -1) {
		switch (c) {
		 case 'd':
			debug++;
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
		if (debug > 1) {
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
	if (debug) {
		char timebuf[32];
		time_t t = hdr->tv_sec;
		struct tm *tm = localtime(&t);
		strftime(timebuf, sizeof(timebuf), "%T", tm);
		printf("[%s.%06d] ", timebuf, hdr->tv_usec);
	}

	ip = (struct ip*)buf;
	if (ip->ip_v == 4) {
		if (ip->ip_p == IPPROTO_TCP) {
			struct tcphdr *th = (struct tcphdr *)((uint8_t *)ip + ip->ip_hl*4);
			dump_tcp(hdr, ip, th);
		} else {
			if (debug) {
				printf("%s -> ", inet_ntoa(ip->ip_src));
				printf("%s PROTO=%d\n", inet_ntoa(ip->ip_dst), ip->ip_p);
			}
		}
	} else if (ip->ip_v == 6) {
		errx(1, "ipv6 not supported");

	} else {
		errx(1, "unknown IP packet");

	}
	return;
}

void
dump_tcp(const pcap_pkthdr32 *hdr, const ip *ip, const tcphdr *th)
{
	// ここでようやく IP:port を表示
	if (debug) {
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
	}

	// TCPペイロードとその長さ
	const uint8_t *tcp = (const uint8_t *)(((uint8_t *)th) + th->th_off * 4);

	int tcplen;
	tcplen = ntohs(ip->ip_len) - (ip->ip_hl * 4);
	tcplen = tcplen - (th->th_off * 4);

	// TCP ペイロードなし (3way ハンドシェークとか) ならここで終わり
	if (tcplen == 0) {
		return;
	}

	// そうでなければ上位レイヤなので書き出す
	if (debug) {
		hexdump(tcp, tcplen);
	} else {
		fwrite(tcp, 1, tcplen, stdout);
	}
}

// デバッグ用のバイナリダンプ
void
hexdump(const void *p, int len)
{
	int i;
	const uint8_t *buf = (const uint8_t *)p;
	for (i = 0; i < len; i++) {
		if (i % 16 == 0)
			printf("  %04x:", i);
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
