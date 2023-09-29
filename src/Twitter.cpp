/*
 * Copyright (C) 2014-2023 Tetsuya Isaki
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

#include "sayaka.h"
#include "JsonInc.h"
#include "Dictionary.h"
#include "Display.h"
#include "RichString.h"
#include "StringUtil.h"
#include "Twitter.h"
#include "subr.h"
#include "term.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <err.h>

class MediaInfo
{
 public:
	MediaInfo()
	{
	}
	MediaInfo(const std::string& target_url_, const std::string& display_url_)
	{
		target_url = target_url_;
		display_url = display_url_;
	}

	std::string target_url  {};
	std::string display_url {};
};

static bool showstatus(const Json *status, bool is_quoted);
static std::string formatid(const std::string& text);
static UString format_rt_owner(const Json& s);
static UString format_rt_cnt(const Json& s);
static UString format_fav_cnt(const Json& s);
static UString formatmsg(const Json& s, std::vector<MediaInfo> *mediainfo);
static void SetTag(RichString& t, const Json& list, Color color);
static void SetUrl_main(RichString& text, int start, int end,
	const std::string& url);
static bool twitter_show_icon(const Json& user, const std::string& screen_name);

// 1ツイート分の JSON を処理する。
bool
showobject(const Json& obj)
{
	// 全ツイートを録画
	if (opt_record_mode == 2) {
		record(obj);
	}

	if (obj.contains("full_text") || obj.contains("text")) {
		// 通常のツイート
		bool crlf = showstatus(&obj, false);
		if (crlf) {
			printf("\n");
		}
	} else {
		// それ以外はとりあえず無視
	}
	return true;
}

// 1ツイートを表示。
// true なら戻ったところで1行空ける改行。ツイートとツイートの間は1行
// 空けるがここで判定の結果何も表示しなかったら空けないなど。
static bool
showstatus(const Json *status, bool is_quoted)
{
#if 0
	// このツイートを表示するかどうかの判定。
	// これは、このツイートがリツイートを持っているかどうかも含めた判定を
	// 行うのでリツイート分離前に行う。
	if (acl(*status, is_quoted) == false) {
		return false;
	}
#endif

	// 表示範囲だけ録画ならここで保存。
	// 実際にはここから NG ワードと鍵垢の非表示判定があるけど
	// もういいだろう。
	if (opt_record_mode == 1 && is_quoted == false) {
		record(*status);
	}

	// NGワード
	NGStatus ngstat;
	bool match = ngword_list.Match(&ngstat, *status);
	if (match) {
		// マッチしたらここで表示
		Debug(diagShow, "showstatus: ng -> false");
		if (opt_show_ng) {
			auto userid = coloring(formatid(ngstat.screen_name), Color::NG);
			auto name = coloring(formatname(ngstat.name), Color::NG);
			auto time = coloring(ngstat.time, Color::NG);
			auto msg = coloring("NG:" + ngstat.ngword, Color::NG);

			print_(name + ' ' + userid + '\n'
			     + time + ' ' + msg + '\n');
			return true;
		}
		return false;
	}

	// RT なら、RT 元を status に、RT先を s に。
	const Json *s = status;
	bool has_retweet = false;
	if ((*status).contains("retweeted_status")) {
		s = &(*status)["retweeted_status"];
		has_retweet = true;
	}

	// --protect オプションなら鍵ユーザのツイートを表示しない。
	if (opt_protect) {
		match = false;
		const Json& user = (*status)["user"];
		if (user.contains("protected") && user.value("protected", false)) {
			match = true;
		} else if (has_retweet) {
			// リツイート先も調べる
			const Json& rusr = (*s)["user"];
			if (rusr.contains("protected") && rusr.value("protected", false)) {
				match = true;
			}
		}
		// どちらかで一致すれば非表示
		if (match) {
			print_(coloring("鍵垢", Color::NG) + UString("\n")
				+ coloring(formattime(*status), Color::Time) + UString("\n"));
			return true;
		}
	}

	// 簡略表示の判定。QT 側では行わない
	if (is_quoted == false) {
		if (has_retweet) {
			const auto& rt_id = (*s).value("id_str", "");

			// 直前のツイートが (フォロー氏による) 元ツイートで
			// 続けてこれがそれを RT したツイートなら簡略表示だが、
			// この二者は別なので1行空けたまま表示。
			if (rt_id == last_id) {
				if (last_id_count++ < last_id_max) {
					auto rtmsg = format_rt_owner(*status);
					auto rtcnt = format_rt_cnt(*s);
					auto favcnt = format_fav_cnt(*s);
					print_(rtmsg + rtcnt + favcnt + '\n');
					// これ以降のリツイートは連続とみなす
					last_id += "_RT";
					return true;
				}
			}
			// 直前のツイートがすでに誰か氏によるリツイートで
			// 続けてこれが同じツイートを RT したものなら簡略表示だが、
			// これはどちらも他者をリツイートなので区別しなくていい。
			if (rt_id + "_RT" == last_id) {
				if (last_id_count++ < last_id_max) {
					auto rtmsg = format_rt_owner(*status);
					auto rtcnt = format_rt_cnt(*s);
					auto favcnt = format_fav_cnt(*s);
					printf(CSI "1A");
					print_(rtmsg + rtcnt + favcnt + '\n');
					return true;
				}
			}
		}

		// 直前のツイートのふぁぼなら簡略表示
		if (0) {
			// userstream でしか来ない
		}

		// 表示確定
		// 次回の簡略表示のために覚えておく。その際今回表示するのが
		// 元ツイートかリツイートかで次回の連続表示が変わる。
		if (has_retweet) {
			last_id = (*s).value("id_str", "") + "_RT";
		} else {
			last_id = (*status).value("id_str", "");
		}
		last_id_count = 0;
	}

	const Json& s_user = (*s)["user"];
	std::string screen_name = s_user.value("screen_name", "");
	auto userid = coloring(formatid(screen_name), Color::UserId);
	auto name = coloring(formatname(s_user.value("name", "")), Color::Username);
	auto src = coloring(unescape(strip_tags((*s).value("source", ""))) + "から",
		Color::Source);
	auto time = coloring(formattime(*s), Color::Time);
	UString verified;
	if (s_user.value("verified", false)) {
		verified = coloring(" ●", Color::Verified);
	}
	UString protected_mark;
	if (s_user.value("protected", false)) {
		protected_mark = coloring(" ■", Color::Protected);
	}

	std::vector<MediaInfo> mediainfo;
	auto msg = formatmsg(*s, &mediainfo);

	ShowIcon(twitter_show_icon, s_user, screen_name);
	print_(name + ' ' + userid + verified + protected_mark);
	printf("\n");
	print_(msg);
	printf("\n");

	// picture
	image_count = 0;
	image_next_cols = 0;
	image_max_rows = 0;
	for (int i = 0; i < mediainfo.size(); i++) {
		const auto& m = mediainfo[i];

		auto indent = (indent_depth + 1) * indent_cols;
		printf(CSI "%dC", indent);
		ShowPhoto(m.target_url, imagesize, i);
		printf("\r");
	}

	// コメント付きRT の引用部分
	if ((*s).contains("quoted_status")) {
		// この中はインデントを一つ下げる
		printf("\n");
		indent_depth++;
		showstatus(&(*s)["quoted_status"], true);
		indent_depth--;
		// 引用表示後のここは改行しない
	}

	// このステータスの既 RT、既ふぁぼ数
	auto rtmsg = format_rt_cnt(*s);
	auto favmsg = format_fav_cnt(*s);
	print_(time + ' ' + src + rtmsg + favmsg);
	printf("\n");

	// リツイート元
	if (has_retweet) {
		print_(format_rt_owner(*status));
		printf("\n");
	}

	// ふぁぼはもう飛んでこない

	return true;
}

// ID 表示用に整形
static std::string
formatid(const std::string& text)
{
	return "@" + text;
}

// リツイート元通知を整形して返す
static UString
format_rt_owner(const Json& status)
{
	const Json& user = status["user"];
	auto rt_time   = formattime(status);
	auto rt_userid = formatid(user.value("screen_name", ""));
	auto rt_name   = formatname(user.value("name", ""));
	auto str = coloring(string_format("%s %s %s がリツイート",
		rt_time.c_str(), rt_name.c_str(), rt_userid.c_str()), Color::Retweet);
	return str;
}

// リツイート数を整形して返す
static UString
format_rt_cnt(const Json& s)
{
	UString str;

	auto rtcnt = s.value("retweet_count", 0);
	if (rtcnt > 0) {
		str = coloring(string_format(" %dRT", rtcnt), Color::Retweet);
	}
	return str;
}

// ふぁぼ数を整形して返す
static UString
format_fav_cnt(const Json& s)
{
	UString str;

	auto favcnt = s.value("favorite_count", 0);
	if (favcnt > 0) {
		str = coloring(string_format(" %dFav", favcnt), Color::Favorite);
	}
	return str;
}

// URL を差し替える
#define SetUrl(r, s, e, u) SetUrl_inline(r, s, e, u, display_end)
static inline void
SetUrl_inline(RichString& richtext, int start, int end, const std::string& url,
	int display_end)
{
	if (end <= display_end) {
#if defined(DEBUG_FORMAT)
		if (__predict_false(opt_debug_format)) {
			printf("SetUrl [%d,%d) |%s|\n", start, end, url.c_str());
		}
#endif
		SetUrl_main(richtext, start, end, url);
	}
#if defined(DEBUG_FORMAT)
	else if (__predict_false(opt_debug_format)) {
		printf("SetUrl [%d,%d) |%s| out of range\n",
			start, end, url.c_str());
	}
#endif
}

// 本文を整形して返す
// (そのためにここでハッシュタグ、メンション、URL を展開)
//
// 従来はこうだった(↓)が
//   "text":本文,
//   "entities":{
//     "hashtag":[..]
//     "user_mentions":[..]
//     "urls":[..]
//   },
//   "extended_entities":{
//     "media":[..]
//   }
// extended_tweet 形式ではこれに加えて
//   "extended_tweet":{
//     "full_text":本文,
//     "entities":{
//     "hashtag":[..]
//     "user_mentions":[..]
//     "urls":[..]
//     "media":[..]
//   }
// が追加されている。media の位置に注意。
// 今取得出来るのはこうなっているようだ。どうして…。
//   "full_text":本文,
//   "entities":{
//     "hashtags":[..]
//     "media":[..] <-おそらく古い形式
//     "urls":[..]
//     "user_mentions":[..]
//   }
//   "extended_entities":{
//     "media":[..] <-おそらく新しい形式
//   }
static UString
formatmsg(const Json& s, std::vector<MediaInfo> *mediainfo)
{
	// 本文
	const Json *textj = GetFullText(s);
	if (__predict_false(textj == NULL)) {
		// ないことはないはず
		return UString("(no text field?)");
	}
	const auto& text = (*textj).get<std::string>();
	RichString richtext(text);

	// richtext は終端文字も含んだ長さなので、最後の文字は一つ手前。
	int display_end = richtext.size() - 1;

	// エンティティの位置が新旧で微妙に違うのを吸収
	const Json *entities = NULL;
	const Json *media_entities = NULL;
	if (s.contains("extended_tweet")) {
		const Json *extw = &s["extended_tweet"];
		if ((*extw).contains("entities")) {
			entities = &(*extw)["entities"];
		}
		media_entities = entities;

		// 表示区間が指定されていたら取得。
		// 後ろの添付画像 URL とかを削るためのもので、
		// 前がどうなるのかは不明。
		if ((*extw).contains("display_text_range")) {
			const Json& range = (*extw)["display_text_range"];
			if (range.is_array() && range.size() >= 2) {
				display_end = range[1].get<int>();
			}
		}
	} else {
		if (s.contains("entities")) {
			entities = &s["entities"];
		}
		if (s.contains("extended_entities")) {
			media_entities = &s["extended_entities"];
		}
	}

	// エンティティを調べる
	if (entities) {
		// ハッシュタグ情報を展開
		if ((*entities).contains("hashtags")) {
			const Json& hashtags = (*entities)["hashtags"];
			SetTag(richtext, hashtags, Color::Tag);
		}

		// ユーザID情報を展開
		if ((*entities).contains("user_mentions")) {
			const Json& mentions = (*entities)["user_mentions"];
			SetTag(richtext, mentions, Color::UserId);
		}

		// URL を展開
		if ((*entities).contains("urls")) {
			const Json& urls = (*entities)["urls"];
			if (urls.is_array()) {
				for (const Json& u : urls) {
					if (u.contains("indices")) {
						const Json& indices = u["indices"];
						if (!indices.is_array() || indices.size() != 2) {
							continue;
						}
						int start = indices[0].get<int>();
						int end   = indices[1].get<int>();

						// url          … 本文中の短縮 URL
						// display_url  … 差し替えて表示用の URL
						// expanded_url … 展開後の URL
						const auto& url      = u.value("url", "");
						const auto& disp_url = u.value("display_url", "");
						const auto& expd_url = u.value("expanded_url", "");

						// 本文の短縮 URL を差し替える
						std::string newurl;
						const auto& qid = s.value("quoted_status_id_str", "");
						std::string text2 = Chomp(text);
						if (s.contains("quoted_status")
						 && expd_url.find(qid) != std::string::npos
						 && EndWith(text2, url))
						{
							// この場合は引用 RT の URL なので、表示しなくていい
							newurl = "";
						} else {
							newurl = disp_url;
						}
						// --full-url モードなら短縮 URL ではなく元 URL を使う
						if (opt_full_url
						 && newurl.find("…") != std::string::npos)
						{
							newurl = string_replace(expd_url, "http://", "");
						}
						SetUrl(richtext, start, end, newurl);

						// 外部画像サービスを解析
						MediaInfo minfo;
#if 0
						if (format_image_url(&minfo, expd_url, disp_url)) {
							(*mediainfo).emplace_back(minfo);
						}
#endif
					}
				}
			}
		}
	}

	// メディア情報を展開
	if (media_entities != NULL && (*media_entities).contains("media")) {
		const Json& media = (*media_entities)["media"];
		for (const Json& m : media) {
			// 本文の短縮 URL を差し替える
			const std::string& disp_url = m.value("display_url", "");
			if (m.contains("indices")) {
				const Json& indices = m["indices"];
				if (indices.is_array() && indices.size() == 2) {
					int start = indices[0].get<int>();
					int end   = indices[1].get<int>();
					SetUrl(richtext, start, end, disp_url);
				}
			}

			// 画像展開に使う
			//   url         本文中の短縮 URL (twitterから)
			//   display_url 差し替えて表示用の URL (twitterから)
			//   media_url   指定の実ファイル URL (twitterから)
			//   target_url  それを元に実際に使う URL (こちらで付加)
			//   width       幅指定。ピクセルか割合で (こちらで付加)
			const std::string& media_url = m.value("media_url", "");
			std::string target_url = media_url + ":small";
			MediaInfo minfo(target_url, disp_url);
			(*mediainfo).emplace_back(minfo);
		}
	}

#if defined(DEBUG_FORMAT)
	if (__predict_false(opt_debug_format)) {
		printf("%s", richtext.dump().c_str());
		printf("display_end = %d\n", display_end);
	}
#endif

	// RichString を UString に変換。
	// ついでに HTML unescape と 改行を処理。
	UString new_text;
	int i;
	for (i = 0; i < display_end; i++) {
		auto& c = richtext[i];

		// 直前に差し込むエスケープシーケンスがあれば先に処理
		if (__predict_false(!c.altesc.empty())) {
			new_text.Append(c.altesc);
			// FALL THROUGH
		}

		// URL があれば展開
		if (__predict_false(!c.alturl.empty())) {
			new_text.Append(UString::FromUTF8(c.alturl));
			// FALL THROUGH
		}

		// 文字を展開
		// ついでに簡単なテキスト置換も同時にやってしまう。

		// URL 展開元の文字は負数にしてあるのでその部分は無視する
		if (__predict_false((int32)c.code < 0)) {
			continue;
		}
		// '\r' は無視
		if (__predict_false(c.code == '\r')) {
			continue;
		}

		// もう一度文字列にするのもあほらしいので、なんだかなあとは
		// 思うけど、ここでついでに unescape() もやってしまう。
		// "&amp;" -> "&"
		// "&lt;"  -> "<"
		// "&gt;"  -> ">"
		if (__predict_false(c.code == '&')) {
			if (richtext[i + 1].code == 'a' &&
			    richtext[i + 2].code == 'm' &&
			    richtext[i + 3].code == 'p' &&
			    richtext[i + 4].code == ';'   )
			{
				new_text.Append('&');
				i += 4;
				continue;
			}
			if (richtext[i + 1].code == 'l' &&
			    richtext[i + 2].code == 't' &&
			    richtext[i + 3].code == ';'   )
			{
				new_text.Append('<');
				i += 3;
				continue;
			}
			if (richtext[i + 1].code == 'g' &&
			    richtext[i + 2].code == 't' &&
			    richtext[i + 3].code == ';'   )
			{
				new_text.Append('>');
				i += 3;
				continue;
			}
		}

		new_text.Append(c.code);
	}

	// 表示する最後の文字の直後のエスケープシーケンスを出力
	if (!richtext[i].altesc.empty()) {
		new_text.Append(richtext[i].altesc);
	}

	return new_text;
}

// formatmsg() の下請け。
// list からタグ情報を取り出して tags にセットする。
// ハッシュタグとユーザメンションがまったく同じ構造なので。
//
// "hashtag": [
//   { "indices": [
//	     <start> … 開始位置、1文字目からなら 0
//       <end>   … 終了位置。この1文字前まで
//     ],
//     "...": 他のキーもあるかも知れないがここでは見ない
//   }, ...
// ]
static void
SetTag(RichString& richtext, const Json& list, Color color)
{
	if (list.is_array() == false) {
		return;
	}

	for (const Json& t : list) {
		if (t.contains("indices")) {
			const Json& indices = t["indices"];
			if (indices.is_array() && indices.size() == 2) {
				int start = indices[0].get<int>();
				int end   = indices[1].get<int>();
#if defined(DEBUG_FORMAT)
				if (__predict_false(opt_debug_format)) {
					printf("SetTag [%d,%d)\n", start, end);
				}
#endif
				richtext[start].altesc += ColorBegin(color);
				richtext[end].altesc += ColorEnd(color);
			}
		}
	}
}

// formatmsg() の下請け。
// text の [start, end) を url で差し替える(ための処理をする)。
static void
SetUrl_main(RichString& text, int start, int end, const std::string& url)
{
	int i = start;

	// すでにあれば何もしない (もうちょっとチェックしたほうがいいかも)
	if (!text[i].alturl.empty()) {
		return;
	}

	// 開始位置に URL を覚えておく
	text[i].alturl = url;
	text[i].altesc = ColorBegin(Color::Url);
	// この範囲の元の文字列を非表示にマークする
	for (; i < end; i++) {
		auto& c = text[i];
		c.code = -c.code;
	}
	// (この手前で)終了
	text[i].altesc = ColorEnd(Color::Url);
}

// アイコン表示のサービス固有部コールバック。
static bool
twitter_show_icon(const Json& user, const std::string& screen_name)
{
	const std::array<std::string, 2> urls = {
		"profile_image_url",
		"profile_image_url_https",
	};

	// http, https の順で試す
	for (const auto& url : urls) {
		if (user.contains(url)) {
			const Json& image_url_json = user[url];
			const auto& image_url = image_url_json.get<std::string>();

			// URL のファイル名部分をキャッシュのキーにする
			auto p = image_url.rfind('/');
			if (__predict_true(p >= 0)) {
				auto img_file = string_format("icon-%dx%d-%s-%s",
					iconsize, iconsize, screen_name.c_str(),
					image_url.c_str() + p + 1);
				if (ShowImage(img_file, image_url, iconsize, -1)) {
					return true;
				}
			}
		}
	}
	return false;
}
