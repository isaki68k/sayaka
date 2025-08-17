/* vi:set ts=4: */
/*
 * Copyright (C) 2016-2025 Tetsuya Isaki
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

//
// sayaka 固有の雑多なサブルーチン
//

#include "sayaka.h"
#include <string.h>
#include <time.h>
#if defined(HAVE_OPENSSL)
#include <openssl/evp.h>
#endif

// Ununtu では <openssl/md5.h> は非推奨だが MD5_DIGEST_LENGTH は使える。
// NetBSD では <openssl/md5.h> がないと見えない。うーんこの。
#ifndef MD5_DIGEST_LENGTH
#define MD5_DIGEST_LENGTH (16)
#endif

// 32ビットの乱数を返す。
uint32
rnd_get32(void)
{
#if defined(HAVE_ARC4RANDOM)
	return arc4random();
#else
	static bool initialized = false;

	if (__predict_false(initialized == false)) {
		struct timespec tv;

		clock_gettime(CLOCK_REALTIME, &tv);
		srandom(tv.tv_sec ^ tv.tv_nsec);
		initialized = true;
	}

	return random();
#endif
}

// 乱数で埋める。
void
rnd_fill(void *dst, uint dstsize)
{
#if defined(HAVE_ARC4RANDOM_BUF)
	arc4random_buf(dst, dstsize);
#else
	for (int i = 0; i < dstsize; ) {
		uint32 r = rnd_get32();
		uint copylen = MIN(dstsize - i, sizeof(r));
		memcpy((char *)dst + i, &r, copylen);
		i += copylen;
	}
#endif
}

// 文字列の FNV1a ハッシュ(32ビット) を返す。
uint32
hash_fnv1a(const char *s)
{
	static const uint32 prime  = 16777619u;
	static const uint32 offset = 2166136261u;

	uint32 hash = offset;
	uint32 c;
	while ((c = *s++) != '\0') {
		hash ^= c;
		hash *= prime;
	}
	return hash;
}

// 文字列の MD5 ハッシュ文字列を返す。
string *
hash_md5(const char *input)
{
#if defined(HAVE_OPENSSL)
	static const char tohex[] = "0123456789abcdef";
	uint8 hash[MD5_DIGEST_LENGTH];
	uint hashlen = sizeof(hash);
	char buf[MD5_DIGEST_LENGTH * 2];
	EVP_MD_CTX *ctx;

	ctx = EVP_MD_CTX_new();
	EVP_DigestInit_ex(ctx, EVP_md5(), NULL);
	EVP_DigestUpdate(ctx, (const unsigned char *)input, strlen(input));
	EVP_DigestFinal_ex(ctx, hash, &hashlen);
	EVP_MD_CTX_free(ctx);

	for (uint i = 0; i < sizeof(hash); i++) {
		buf[i * 2 + 0] = tohex[hash[i] >> 4];
		buf[i * 2 + 1] = tohex[hash[i] & 0xf];
	}

	return string_from_mem(buf, sizeof(buf));
#else
	return NULL;
#endif // HAVE_OPENSSL
}

// BASE64 エンコードした文字列を返す。
string *
base64_encode(const void *vsrc, uint srclen)
{
	static const char enc[] =
		"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
		"abcdefghijklmnopqrstuvwxyz"
		"0123456789+/";

	const uint8 *src = (const uint8 *)vsrc;
	uint dstlen = ((srclen + 2) / 3) * 4;
	string *dst = string_alloc(dstlen + 1);
	char *d0 = string_get_buf(dst);
	char *d = d0;
	uint i;

	uint packed = (srclen / 3) * 3;
	for (i = 0; i < packed; ) {
		// 0000'0011  1111'2222  2233'3333
		uint8 a0 = src[i++];
		uint8 a1 = src[i++];
		uint8 a2 = src[i++];

		*d++ = enc[a0 >> 2];
		*d++ = enc[((a0 & 0x03) << 4) | (a1 >> 4)];
		*d++ = enc[((a1 & 0x0f) << 2) | (a2 >> 6)];
		*d++ = enc[a2 & 0x3f];
	}

	// 残りは 0, 1, 2 バイト。
	uint remain = srclen - packed;
	if (remain == 1) {
		uint8 a0 = src[i];

		*d++ = enc[a0 >> 2];
		*d++ = enc[((a0 & 0x03) << 4)];
	} else if (remain == 2) {
		uint8 a0 = src[i++];
		uint8 a1 = src[i];

		*d++ = enc[a0 >> 2];
		*d++ = enc[((a0 & 0x03) << 4) | (a1 >> 4)];
		*d++ = enc[((a1 & 0x0f) << 2)];
	}

	// パディング。
	switch ((d - d0) % 4) {
	 case 2:
		*d++ = '=';	// FALLTHROUGH
	 case 3:
		*d++ = '=';	// FALLTHROUGH
	 default:
		break;
	}
	*d = '\0';
	dst->len = (d - d0);

	return dst;
}

// ISO 形式の時刻文字列を Unix time に変換する。
time_t
decode_isotime(const char *str)
{
	const char *s = &str[0];
	char *end;
	struct tm tm;
	int zh;
	int zm;

	memset(&tm, 0, sizeof(tm));

	int year = stou32def(s, -1, &end);
	if (year < 0 || *end != '-') return 0;
	tm.tm_year = year - 1900;
	s = end + 1;

	int mon = stou32def(s, -1, &end);
	if (mon < 0 || mon > 99 || *end != '-') return 0;
	tm.tm_mon = mon - 1;
	s = end + 1;

	tm.tm_mday = stou32def(s, -1, &end);
	if (tm.tm_mday < 0 || tm.tm_mday > 99 || *end != 'T') return 0;
	s = end + 1;

	tm.tm_hour = stou32def(s, -1, &end);
	if (tm.tm_hour < 0 || tm.tm_hour > 99 || *end != ':') return 0;
	s = end + 1;

	tm.tm_min = stou32def(s, -1, &end);
	if (tm.tm_min < 0 || tm.tm_min > 99 || *end != ':') return 0;
	s = end + 1;

	tm.tm_sec = stou32def(s, -1, &end);
	if (tm.tm_sec < 0 || tm.tm_sec > 99) return 0;

	// 秒の後ろは [.\d+][<timezone>]
	if (*end == '.') {
		s = end + 1;
		int frac = stou32def(s, -1, &end);
		if (frac < 0) return 0;
	}
	// <timezone> は Z か (+|-)(\d\d:?\d\d)
	if (*end == 'Z') {
		end++;
		zh = 0;
		zm = 0;
	} else if (*end == '+' || *end == '-') {
		int sign = (*end == '+') ? 1 : -1;
		s = end + 1;
		int z = stou32def(s, -1, &end);
		if (z < 0) return 0;
		if (end == s + 2) {
			zh = z;
			if (*end != ':') {
				return 0;
			}
			s = end + 1;
			zm = stou32def(s, -1, &end);
			if (zm < 0) return 0;
			if (end != s + 2) return 0;
		} else if (end == s + 4) {
			zh = z / 100;
			zm = z % 100;
		} else {
			return 0;
		}
		zh *= sign;
		zm *= sign;
	} else {
		return 0;
	}

	if (*end != '\0') {
		return 0;
	}

	// タイムゾーンを置いといて Unixtime に変換。
	time_t time = timegm(&tm);

	// タイムゾーン分を補正。
	time -= zh * 60 * 60 + zm * 60;

	return time;
}

// UNIX 時刻から表示用の文字列を返す。
string *
format_time(time_t unixtime)
{
	char buf[64];
	struct tm dtm;

	localtime_r(&unixtime, &dtm);

	// 現在時刻。
	time_t now;
	struct tm ntm;
	time(&now);
	localtime_r(&now, &ntm);

	const char *fmt;
	if (dtm.tm_year == ntm.tm_year && dtm.tm_yday == ntm.tm_yday) {
		// 今日なら時刻のみ。
		fmt = "%T";
	} else if (dtm.tm_year == ntm.tm_year) {
		// 昨日以前で今年中なら年を省略。(mm/dd HH:MM:SS)
		// XXX 半年以内ならくらいのほうがいいのか?
		fmt = "%m/%d %T";
	} else {
		// 去年以前なら yyyy/mm/dd HH:MM。(秒はもういいだろう…)
		fmt = "%Y/%m/%d %R";
	}
	strftime(buf, sizeof(buf), fmt, &dtm);
	return string_from_cstr(buf);
}
