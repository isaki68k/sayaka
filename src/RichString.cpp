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
	bool regional_flag = false;
	for (int end = srcstr.size(); byteoffset < end; ) {
		// この文字
		RichChar rc;

		rc.byteoffset = byteoffset;

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
		if (0) {
			printf("%02X\n", rc.code);
		}

		// Unicode の文字数カウントは地獄だが、
		// とりあえずこの記事の中盤以降にある独自方針を参考にする。
		// https://qiita.com/_sobataro/items/47989ee4b573e0c2adfc

#define RANGE(x, l, h) __predict_false((l) <= (x) && (x) <= (h))

		if (__predict_false(rc.code == 0x20e3) // Combining Enclosing Keycap
		 || RANGE(rc.code, 0x1f3fb, 0x1f3ff))
								// Emoji Modifier Fitzpatrick Type 1-2 .. 6
		{
			// カウントしない
			charoffset = info.back().charoffset;
		} else
		if (RANGE(rc.code, 0xfe00,  0xfe0f)		// Variation Selector {1-16}
		 || RANGE(rc.code, 0xe0100, 0xe01ef))	// Variation Selector {17-256}
		{
			// VS は 1文字としてカウントしないので、
			// 文字数カウンタを1文字前の状態に戻して再開。
			// …のはずだが twitter さんは、VS が連続していると2つ目の
			// ほうを独立した1文字としてカウントした形跡があるので、
			// 仕方ないので適当に対処。
			auto& prev = info.back();
			if (RANGE(prev.code, 0xfe00, 0xfe0f)
			 || RANGE(prev.code, 0xe0100, 0xe01ef)) {
				// 前も VS だと、VS 扱いしない?
			} else {
				// VS はカウントしない。(こっちが通常)
				charoffset = prev.charoffset;
			}
		} else if (RANGE(rc.code, 0x1f1e6, 0x1f1ff)) { // Regional Indicator
			// 連続する2文字で1文字とカウントする…
			// 連続しなかったらとかはとりあえず無視。
			if (__predict_true(regional_flag == false)) {
				// 1文字目はフラグを立てて文字をカウント
				regional_flag = true;
			} else {
				// 2文字目は文字はカウントしない
				regional_flag = false;
				charoffset = info.back().charoffset;
			}
		}
		rc.charoffset = charoffset++;

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

#if defined(SELFTEST)
#include "test.h"

void
test_RichString()
{
	printf("%s\n", __func__);

	std::vector<std::pair<std::string, std::vector<int>>> table = {
		// テスト表示名,入力文字列						期待値
		{ "A,A!",										{ 0, 1, 2 } },

		// UTF-16 でサロゲートペアになる文字 (UTF-8/32 では関係ない)
		// U+20BB7 (吉野家のツチヨシ)
		{ "吉,\xf0\xa0\xae\xb7" "!",					{ 0, 1, 2 } },

		// IVS
		// "葛" U+845B (IVSなし) (= くさかんむりに日に匂)
		{ "葛,\xe8\x91\x9b" "!",						{ 0, 1, 2 } },
		// "葛" U+845B U+E0101 (IVSあり) (= くさかんむりに曷)
		// https://seiai.ed.jp/sys/text/csd/cf14/c14a090.html
		// https://xtech.nikkei.com/it/article/COLUMN/20100126/343783/
		{ "葛IVS,\xe8\x91\x9b\xf3\xa0\x84\x81" "!",		{ 0, 0, 1, 2 } },

		// SVS
		// https://qiita.com/_sobataro/items/47989ee4b573e0c2adfc
		// U+231b "Hourglass" (SVSなし)
		{ "HG,\xe8\x8c\x9b" "!",						{ 0, 1, 2 } },
		// U+231b U+FE0E (TPVS)
		{ "HG+TPVS,\xe8\x8c\x9b\xef\xb8\x8e" "!",		{ 0, 0, 1, 2 } },
		// U+231b U+FE0F (EPVS)
		{ "HG+EPVS,\xe8\x8c\x9b\xef\xb8\x8f" "!",		{ 0, 0, 1, 2 } },

		// VS が連続すると2つ目のほうを独立した1文字と数えたようだ。
		// どう解釈したらそうなるのか分からんけど。
		{ "VS2,\xe8\x8c\x9b" "\xef\xb8\x8f" "\xef\xb8\x8e" "!",
														{ 0, 0, 1, 2, 3 } },

		// Emoji Combining Sequence (囲み文字)
		{ "Keycap,1" "\xef\xb8\x8f" "\xe2\x83\xa3" "!",	{ 0, 0, 0, 1, 2 } },

		// Skin tone
		{ "Skin,\xf0\x9f\x91\xa8" "\xf0\x9f\x8f\xbd" "!", { 0, 0, 1, 2 } },

		// Regional Indicator (国旗絵文字)
		{ "Flag,\xf0\x9f\x87\xaf" "\xf0\x9f\x87\xb5"
		       "\xf0\x9f\x87\xaf" "\xf0\x9f\x87\xb5",	{ 0, 0, 1, 1, 2 } },
	};
	for (const auto& a : table) {
		const auto& name_input = Split2(a.first, ',');
		const auto& expected = a.second;

		const auto& testname = name_input.first;
		const auto& input = name_input.second;

		RichString rtext(input);
		if (rtext.size() == expected.size()) {
			for (int i = 0; i < expected.size(); i++) {
				xp_eq(expected[i], rtext[i].charoffset,
					testname + string_format("[%d]", i));
			}
		} else {
			xp_eq(expected.size(), rtext.size(), testname);
			printf("expected");
			for (auto& x : expected) {
				printf(" %d", x);
			}
			printf(" but");
			for (int i = 0; i < rtext.size(); i++) {
				printf(" %d", rtext[i].charoffset);
			}
			printf("\n");
		}
	}
}
#endif
