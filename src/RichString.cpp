#include "RichString.h"
#include "StringUtil.h"
#include <iconv.h>
#include <sys/endian.h>

// コンストラクタ
RichString::RichString(const std::string& text_)
{
	text = text_;

	// 先頭からの文字数とバイト数を数える
	MakeInfo(&charinfo, text_);
}

// UTF-8 文字列 srcstr から内部情報 info を作成する。
//
// 最後の文字のバイト長も統一的に扱うため、末尾に終端文字があるとして
// そのオフセットも保持しておく。そのため以下のように2文字の文字列なら
// TextTag 配列は要素数 3 になる。
//
// "あA" の2文字の場合
//                       +0    +1    +2    +3    +4
//                       'あ'               'A'
//                      +-----+-----+-----+-----+
//               text = | $e3   $81   $82 | $41 |
//                      +-----+-----+-----+-----+
//                       ^                 ^      ^
//                       |                 |      :
// info[0].byteoffset=0 -+                 |      :
// info[0].charoffset=0                    |      :
//                                         |      :
// info[1].byteoffset=3 -------------------+      :
// info[1].charoffset=1                           :
//                                                :
// info[2].byteoffset=4 - - - - - - - - - - - - - +
// info[2].charoffset=2
//
bool
RichString::MakeInfo(std::vector<RichChar> *info_, const std::string& srcstr)
	const
{
	std::vector<RichChar>& info = *info_;
	iconv_t cd;

	cd = iconv_open(UTF32_HE, "utf-8");
	if (cd == (iconv_t)-1) {
		return false;
	}

	int charoffset = 0;
	int byteoffset = 0;
	for (int end = srcstr.size(); byteoffset < end; ) {
		// この文字
		RichChar rc;

		rc.byteoffset = byteoffset;
		rc.charoffset = charoffset++;

		// UTF-8 は1バイト目でこの文字のバイト数が分かる
		uint8 c = srcstr[byteoffset];
		int bytelen;
		if (__predict_true(c < 0x80)) {
			bytelen = 1;
		} else if (__predict_true(0xc0 <= c && c < 0xe0)) {
			bytelen = 2;
		} else if (__predict_true(0xe0 <= c && c < 0xf0)) {
			bytelen = 3;
		} else if (__predict_true(0xf0 <= c && c < 0xf8)) {
			bytelen = 4;
		} else if (__predict_true(0xf8 <= c && c < 0xfc)) {
			bytelen = 5;
		} else if (__predict_true(0xfc <= c && c < 0xfe)) {
			bytelen = 6;
		} else {
			// こないはずだけど、とりあえず
			bytelen = 1;
		}
		byteoffset += bytelen;

		// この1文字を UTF-32 に変換
		const char *src = &srcstr[rc.byteoffset];
		size_t srcleft = bytelen;
		char dstbuf[4];	// 32bit で足りるはず
		char *dst = dstbuf;
		size_t dstlen = sizeof(dstbuf);
		size_t r = iconv(cd, &src, &srcleft, &dst, &dstlen);
		if (r == (size_t)-1) {
			// どうすべ
			iconv_close(cd);
			return false;
		}
		if (r > 0) {
			// どうすべ
			iconv_close(cd);
			errno = 0;
			return false;
		}
		rc.code = *(unichar *)&dstbuf[0];

		info.emplace_back(rc);
	}
	iconv_close(cd);

	// 終端文字分を持っておくほうが何かと都合がよい
	RichChar t;
	t.code = 0;
	t.byteoffset = byteoffset;
	t.charoffset = charoffset;
	info.emplace_back(t);

	return true;
}

// n 文字目の文字(の先頭)を返す
RichChar&
RichString::GetNthChar(int n)
{
	for (auto& c : charinfo) {
		if (c.charoffset == n) {
			return c;
		}
	}

	// 終端文字を返す
	return charinfo.back();
}

std::string
RichString::dump() const
{
	std::string rv;

	for (int i = 0; i < charinfo.size(); i++) {
		const auto& c = charinfo[i];
		const auto& next = charinfo[i + 1];
		rv += string_format("[%d] char=%d byte=%d",
			i, c.charoffset, c.byteoffset);

		if ((int32_t)c.code < 0) {
			rv += " code=-1";
		} else {
			rv += string_format(" U+%02x '%s'", c.code,
				text.substr(c.byteoffset, next.byteoffset - c.byteoffset)
					.c_str());
			for (int j = c.byteoffset; j < next.byteoffset; j++) {
				rv += string_format(" %02x", (unsigned char)text[j]);
			}
		}

		if (!c.altesc.empty()) {
			rv += " altesc=";
			for (int j = 0; j < c.altesc.size(); j++) {
				rv += string_format(" %02x", (unsigned char)c.altesc[j]);
			}
		}
		if (!c.alturl.empty()) {
			rv += " alturl=|" + c.alturl + "|";
		}
		rv += "\n";
	}

	return rv;
}

