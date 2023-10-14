/*
 * Copyright (C) 2021 Tetsuya Isaki
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

#include "UString.h"
#include <array>
#include <cstring>

// 出力文字コードが UTF-8 以外 (iconv による変換が必要) なら true。
/*static*/ bool UString::use_iconv = false;

#if defined(HAVE_ICONV)
// UTF-32 から UTF-8 以外の出力文字コードへの変換用
/*static*/ iconv_t UString::cd = (iconv_t)-1;
#endif

// 文字コードの初期化。codeset は出力文字コード名。
// 失敗すれば errno をセットし false を返す。
/*static*/ bool
UString::Init(const std::string& codeset)
{
	use_iconv = false;

	if (codeset.empty()) {
		// UTF-8 なら iconv 不要。
		return true;
	} else {
#if defined(HAVE_ICONV)
		// UTF-8 以外なら iconv を使う。
		cd = iconv_open(codeset.c_str(), "utf-8");
		if (cd == (iconv_t)-1) {
			return false;
		}
		use_iconv = true;
		return true;
#else
		// UTF-8 以外が指定されたのに iconv がなければエラー。
		// (iconv_open() のエラーと区別つけるため errno = 0 にする)
		errno = 0;
		return false;
#endif
	}
}

// pos 文字目から key.size() 文字が key と一致すれば true を返す。
bool
UString::SubMatch(size_type pos, const std::string& key) const
{
	size_type len = key.size();

	if (pos + len >= size()) {
		return false;
	}
	for (size_type i = 0; i < len; i++) {
		if ((*this)[pos + i] != key[i]) {
			return false;
		}
	}
	return true;
}

// UTF-8 文字列 str を UString に変換する。
/*static*/ UString
UString::FromUTF8(const std::string& str)
{
	UString ustr;

	const char *s = str.c_str();
	for (int i = 0, end = str.size(); i < end; ) {
		auto [ code, len ] = UCharFromUTF8(s + i);

		ustr.Append(code);
		i += len;
	}

	return ustr;
}

// この UString を Init() で指定した文字コードの std::string に変換する。
std::string
UString::ToString() const
{
	std::vector<char> dst(size() * 4 + 1);

	// まず1文字ずつ UTF-8 文字列に変換する
	int offset = 0;
	for (const auto code : *this) {
		int len = UCharToUTF8(&dst[offset], code);
		offset += len;
	}
	dst[offset] = '\0';

	std::string utf8((const char *)dst.data());

#if defined(HAVE_ICONV)
	if (use_iconv) {
		return UTF8ToOutCode(utf8);
	}
#endif
	return utf8;
}

#if defined(HAVE_ICONV)
// UTF-8 文字列 utf8 を Init() で設定した出力文字コードに変換して返す。
/*static*/ std::string
UString::UTF8ToOutCode(const std::string& utf8)
{
	// 文字列全体を変換してみる
	// 変換先バッファは変換元文字数 n に対して、EUC-JP なら 2n+1 でいいが
	// JIS なら前後のエスケープシーケンスがあるので 2n+6+1。
	const char *src = (const char *)utf8.data();
	size_t srcleft = utf8.size();
	size_t dstlen = srcleft * 2 + 6 + 1;
	std::vector<char> dstbuf(dstlen);
	char *dst = dstbuf.data();
	while (*src) {
		size_t r = ICONV(cd, &src, &srcleft, &dst, &dstlen);
		if (r == (size_t)-1) {
			if (errno == EILSEQ) {
				// 変換できない文字の場合、
				// 入力の UTF-8 を1文字飛ばす。
				auto p = UCharFromUTF8(src);
				auto uclen = p.second;
				src += uclen;
				srcleft -= uclen;

				// 代わりにゲタを出力。(再帰呼び出し)
				auto alt = UTF8ToOutCode("〓");
				strlcpy(dst, alt.c_str(), dstlen);
				dst += alt.size();
				dstlen--;

				continue;
			} else {
				// それ以外のエラーならどうする?
				break;
			}
		}
	}

	*dst = '\0';
	std::string str((const char *)dstbuf.data());

	return str;
}
#endif

// UTF-32 文字 uni が Init() で指定した文字コードに変換できるかどうかを返す。
// 変換できれば true、出来なければ false を返す。
/*static*/ bool
UString::IsUCharConvertible(unichar uni)
{
#if defined(HAVE_ICONV)
	if (use_iconv) {
		// UTF-32 の uni を UTF-8 の srcbuf に変換
		std::array<char, 4> srcbuf;
		size_t srcleft = UCharToUTF8(srcbuf.data(), uni);
		const char *src = srcbuf.data();
		// dstbuf は EUC-JP なら 2バイト、
		// JIS なら前後のエスケープシーケンスも含めるので8バイト必要。
		std::array<char, 8> dstbuf;
		char *dst = dstbuf.data();
		size_t dstlen = dstbuf.size();

		// UTF-8 を出力文字コードに変換してみる
		auto r = ICONV(cd, &src, &srcleft, &dst, &dstlen);
		if (__predict_true(r == 0)) {
			return true;
		}
		// 変換できない(>0)でもエラー(<0)でもとりあえず false で帰る
	}
#else
	// これは起きないはずなので、これでいい
#endif
	return false;
}

// UTF-8 文字列 src から始まる1文字を Unicode コードポイントに変換する。
// 戻り値はコードポイントとこの文字のバイト数のペア。
// 変換出来なかったりしたら諦めてその時点での状態で帰る。
/*static*/ std::pair<unichar, int>
UString::UCharFromUTF8(const char *src)
{
	unichar code;
	int bytelen;
	uint8 c;

	c = *src;
	if (__predict_false(c == 0)) {
		return { 0, 1 };
	}

	// 1バイト目。UTF-8 は1バイト目でこの文字のバイト数が分かる
	if (__predict_true(c < 0x80)) {
		// 1バイト
		return { c, 1 };

	} else if (__predict_true(0xc2 <= c && c <= 0xdf)) {
		bytelen = 2;
		code = c & 0x1f;
	} else if (__predict_true(0xe0 <= c && c <= 0xef)) {
		bytelen = 3;
		code = c & 0x0f;
	} else if (__predict_true(0xf0 <= c && c <= 0xf4)) {
		bytelen = 4;
		code = c & 0x07;
	} else {
		// こないはずだけど、とりあえず
		return { c, 1 };
	}

	// 2バイト目以降。
	int pos;
	for (pos = 1; pos < bytelen && src[pos] != 0; pos++) {
		code = (code << 6) | (src[pos] & 0x3f);
	}

	// 文字の途中で '\0' になってしまったらエラーだが、
	// 正常系と同じ値でそのまま帰るくらいしか、出来ることはない。

	return { code, pos };
}

// Unicode コードポイント code を UTF-8 に変換して dst に書き出す。
// dst は '\0' 終端しない。
// 戻り値は書き出したバイト数。
/*static*/ int
UString::UCharToUTF8(char *dst, unichar code)
{
	if (code < 0x80) {
		// 1バイト
		*dst = (char)code;
		return 1;

	} else if (code < 0x7ff) {
		// 2バイト
		*dst++ = 0xc0 | (code >> 6);
		*dst++ = 0x80 | (code & 0x3f);
		return 2;

	} else if (code < 0x10000) {
		// 3バイト
		*dst++ = 0xe0 |  (code >> 12);
		*dst++ = 0x80 | ((code >> 6) & 0x3f);
		*dst++ = 0x80 |  (code & 0x3f);
		return 3;

	} else {
		// 4バイト
		*dst++ = 0xf0 |  (code >> 18);
		*dst++ = 0x80 | ((code >> 12) & 0x3f);
		*dst++ = 0x80 | ((code >>  6) & 0x3f);
		*dst++ = 0x80 |  (code & 0x3f);
		return 4;
	}
}

std::string
UString::dump() const
{
	std::string str;

	for (int i = 0, end = size(); i < end; i++) {
		const auto c = (*this)[i];

		str += string_format("[%d] U+%04x", i, c);
		if (c < 0x80) {
			if (c == 0x1b) {
				str += " ESC";
			} else if (c == 0x0a) {
				str += " \\n";
			} else {
				str.push_back(' ');
				str.push_back('\'');
				str.push_back(c);
				str.push_back('\'');
			}
		}
		str += "\n";
	}
	return str;
}
