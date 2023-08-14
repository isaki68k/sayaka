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
#include "Dictionary.h"
#include "Json.h"
#include "RichString.h"
#include "StringUtil.h"
#include "subr.h"
#include "term.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <err.h>
#include <unistd.h>

#define AUTHORIZE_URL		"https://twitter.com/oauth/authorize"
#define API_URL				"https://api.twitter.com"
#define ACCESS_TOKEN_URL	API_URL "/oauth/access_token"
#define REQUEST_TOKEN_URL	API_URL "/oauth/request_token"
#define APIv1_1(path)		API_URL "/1.1/" path ".json"

#define CONSUMER_KEY		"jPY9PU5lvwb6s9mqx3KjRA"
#define CONSUMER_SECRET		"faGcW9MMmU0O6qTrsHgcUchAiqxDcU9UjDW2Zw"

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

static bool showobject(const Json& obj);
static bool showstatus(const Json *status, bool is_quoted);
static UString format_rt_owner(const Json& s);
static UString format_rt_cnt(const Json& s);
static UString format_fav_cnt(const Json& s);
static UString formatmsg(const Json& s, std::vector<MediaInfo> *mediainfo);
static void SetTag(RichString& t, const Json& list, Color color);
static void SetUrl_main(RichString& text, int start, int end,
	const std::string& url);
static void show_icon(const Json& user);
static bool show_photo(const std::string& img_url, int resize_width, int index);
[[maybe_unused]] static void get_credentials();
static Json APIJson(const std::string& method, const std::string& uri,
	const std::string& urimsg,
	const StringDictionary& options, std::vector<std::string> *recvp = NULL);
static std::string errors2string(const Json& json);
static void InitOAuth();
static void get_access_token_v1();


// 投稿する
void
cmd_tweet()
{
	// 標準入力から受け取る。UTF-8 前提。
	// ツイートは半角240字、全角140字換算で、全角はたぶんだいたい3バイト
	// なので、420 バイト程度が上限のはず?
	std::array<char, 1024> buf;
	int len = 0;
	while (len < buf.size() - 1) {
		if (fgets(buf.data() + len, buf.size() - len - 1, stdin) == NULL)
			break;
		len = strlen(buf.data());
	}

	std::string text(buf.data());
	text = Chomp(text);

	// アクセストークンを取得
	InitOAuth();

	// 投稿するパラメータを用意
	StringDictionary options;
	options.AddOrUpdate("status", text);
	options.AddOrUpdate("trim_user", "1");

	// 投稿
	Json json;
	try {
		json = APIJson("POST", APIv1_1("statuses/update"),
			"statuses/update", options);
	} catch (const std::string errmsg) {
		errx(1, "%s", errmsg.c_str());
	}
	if (json.contains("errors")) {
		errx(1, "statuses/update failed%s", errors2string(json).c_str());
	}
	printf("Posted.\n");
}

// フィルタストリーム
void
cmd_stream()
{
	// アクセストークンを取得
	InitOAuth();

	int sleep_sec = 120;
	for (;;) {
		StringDictionary options;
		std::vector<std::string> recvhdrs;

		options["include_entities"] = "1";
		options["tweet_mode"] = "extended";

		if (__predict_false(last_id.empty())) {
			// 最初の1回は home から直近の1件を取得。
			options["count"] = "1";
		} else {
			// 次からは前回以降を取得。
			sleep(sleep_sec);
			options["since_id"] = last_id;
		}

		Json json;
		try {
			json = APIJson("GET", APIv1_1("statuses/home_timeline"),
				"statuses/home_timeline", options, &recvhdrs);
		} catch (const std::string errmsg) {
			warnx("%s", errmsg.c_str());
			return;
		}
		if (json.is_array() == false) {
			warnx("statuses/home_timeline returns non-array: %s",
				json.dump().c_str());
			return;
		}
		// json は新→旧の順に並んでいるので、逆順に取り出す。
		for (int i = json.size() -1; i >= 0; i--) {
			const Json& j = json[i];
			showobject(j);

			const auto& id_str = j.value("id_str", "");
			if (id_str > last_id) {
				last_id = id_str;
			}
		}

		// x-rate-limit-reset: <UNIXTIME> と
		// x-rate-limit-remaining: <num> から次の接続までの待ち時間を決定。
		// 15分間で 15回しかないが、リセット時間までに 2回くらいは残して
		// おいてみる。つまり 15分で最大 13回分。
		auto resettime_str = HttpClient::GetHeader(recvhdrs,
			"x-rate-limit-reset");
		auto remaining_str = HttpClient::GetHeader(recvhdrs,
			"x-rate-limit-remaining");

		uint64 resettime = strtoull(resettime_str.c_str(), NULL, 10);
		uint32 remaining = strtoul(remaining_str.c_str(), NULL, 10);
		time_t now = time(NULL);
		if (resettime > now) {
			if (remaining > 2) {
				sleep_sec = (resettime - now) / (remaining - 1);
			} else {
				sleep_sec = (resettime - now);
			}
		} else {
			// ?
			sleep_sec = 120;
		}
		Debug(diag, "remain=%d until=%ld, sleep=%d",
			remaining, (long)(resettime - now), sleep_sec);
	}
}

// 1ツイート分の JSON を処理する。
static bool
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
	auto userid = coloring(formatid(s_user.value("screen_name", "")),
		Color::UserId);
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

	show_icon(s_user);
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
		show_photo(m.target_url, imagesize, i);
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
		if (__predict_false((int32_t)c.code < 0)) {
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

// 現在行に user のアイコンを表示。
// 呼び出し時点でカーソルは行頭にあるため、必要なインデントを行う。
// アイコン表示後にカーソル位置を表示前の位置に戻す。
static void
show_icon(const Json& user)
{
	const std::array<std::string, 2> urls = {
		"profile_image_url",
		"profile_image_url_https",
	};
	std::string screen_name;

	// 改行x3 + カーソル上移動x3 を行ってあらかじめスクロールを
	// 発生させ、アイコン表示時にスクロールしないようにしてから
	// カーソル位置を保存する
	// (スクロールするとカーソル位置復元時に位置が合わない)
	printf("\n\n\n" CSI "3A" ESC "7");

	// インデント。
	// CSI."0C" は0文字でなく1文字になってしまうので、必要な時だけ。
	if (indent_depth > 0) {
		int left = indent_cols * indent_depth;
		printf(CSI "%dC", left);
	}

	bool shown = false;
	if (__predict_false(use_sixel == UseSixel::No)) {
		goto done;
	}

	screen_name = unescape(user.value("screen_name", ""));
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
				if (show_image(img_file, image_url, iconsize, -1)) {
					shown = true;
					goto done;
				}
			}
		}
	}

 done:
	if (__predict_true(shown)) {
		// アイコン表示後、カーソル位置を復帰
		printf("\r");
		// カーソル位置保存/復元に対応していない端末でも動作するように
		// カーソル位置復元前にカーソル上移動x3を行う
		printf(CSI "3A" ESC "8");
	} else {
		// アイコンを表示してない場合はここで代替アイコンを表示。
		printf(" *");
		// これだけで復帰できるはず
		printf("\r");
	}
}

// index は画像の番号 (位置決めに使用する)
static bool
show_photo(const std::string& img_url, int resize_width, int index)
{
	auto img_file = img_url;

	for (auto p = 0;
		(p = img_file.find_first_of(":/()? ", p)) != std::string::npos;
		p++)
	{
		img_file[p] = '_';
	}

	return show_image(img_file, img_url, resize_width, index);
}

// 自分の ID を取得
static void
get_credentials()
{
	InitOAuth();

	StringDictionary options;
	options["include_entities"] = "false";
	options["include_email"] = "false";
	Json json;
	try {
		json = APIJson("GET", APIv1_1("account/verify_credentials"),
			"account/verify_credentials", options);
	} catch (const std::string errmsg) {
		errx(1, "%s", errmsg.c_str());
	}
	Debug(diag, "json=|%s|", json.dump().c_str());
	if (json.is_object() == false) {
		errx(1, "get_credentials returned non-object: %s", json.dump().c_str());
	}
	if (json.contains("errors")) {
		errx(1, "get_credentials failed%s", errors2string(json).c_str());
	}

	myid = json.value("id_str", "");
}

// uri に method (GET/POST) で接続し、結果の JSON を返す。
// urimsg はエラーメッセージ表示用 (API名など)。
// 接続が失敗したらエラーメッセージ(std::string)をスローする。
// 接続できれば受信した JSON を返す。
// recvp が指定されていれば受信ヘッダを返す。
static Json
APIJson(const std::string& method, const std::string& uri,
	const std::string& urimsg,
	const StringDictionary& options, std::vector<std::string> *recvp)
{
	HttpClient client;
	InputStream *stream = NULL;
	std::string line;
	Json json;

	oauth.AdditionalParams.clear();

	if (!options.empty()) {
		for (const auto& [key, val] : options) {
			oauth.AdditionalParams[key] = val;
		}
	}

	Trace(diag, "InitHttp call");
	if (oauth.InitHttp(client, method, uri) == false) {
		Debug(diag, "%s: InitHttp failed", uri.c_str());
		throw urimsg + ": InitHttp failed";
	}
	Trace(diag, "InitHttp return");

	// Ciphers 指定があれば指示
	if (!opt_ciphers.empty()) {
		client.SetCiphers(opt_ciphers);
	}

	Trace(diag, "client.Act call");
	stream = client.Act(method);
	Trace(diag, "client.Act return");
	if (stream == NULL) {
		throw method + " " + urimsg + ": " + client.ResultMsg;
	}

	if (recvp) {
		*recvp = client.RecvHeaders;
	}

	auto r = stream->ReadLine(&line);
	if (__predict_false(r < 0)) {
		throw urimsg + ": ReadLine failed: " + std::string(strerrno());
	}
	Trace(diag, "ReadLine |%s|", line.c_str());

	if (line.empty()) {
		return json;
	}

	return Json::parse(line);
}

// APIJson の応答がエラーだった時に表示用文字列に整形して返す。
// if (json.contains("errors")) {
//   auto msg = errors2string(json);
// のように呼び出す。
static std::string
errors2string(const Json& json)
{
	const Json& errors = json["errors"];
	if (errors.is_array()) {
		// エラーが複数返ってきたらどうするかね
		const Json& error = errors[0];
		auto code = error.value("code", 0);
		const auto& message = error.value("message", "");
		return string_format(": %s(%d)", message.c_str(), code);
	}
	return "";
}

// OAuth オブジェクトを初期化
static void
InitOAuth()
{
	assert(oauth.ConsumerKey.empty());

	oauth.SetDiag(diagHttp);
	oauth.ConsumerKey    = CONSUMER_KEY;
	oauth.ConsumerSecret = CONSUMER_SECRET;

	// ファイルからトークンを取得
	// なければトークンを取得してファイルに保存
	if (tokenfile.empty()) {
		tokenfile = basedir + "token.json";
	}
	bool r = oauth.LoadTokenFromFile(tokenfile);
	if (r == false) {
		get_access_token_v1();
	}
}

// OAuth v1.0 のアクセストークンを取得する。
// 取得できなければ errx(3) で終了する。
static void
get_access_token_v1()
{
	oauth.AdditionalParams.clear();

	Debug(diag, "----- Request Token -----");
	oauth.RequestToken(REQUEST_TOKEN_URL);

	printf("Please go to:\n"
		AUTHORIZE_URL "?oauth_token=%s\n", oauth.AccessToken.c_str());
	printf("\n");
	printf("And input PIN code: ");
	fflush(stdout);

	char pin_str[1024];
	if (fgets(pin_str, sizeof(pin_str), stdin) == NULL) {
		err(1, "fgets");
	}

	Debug(diag, "----- Access Token -----");

	oauth.AdditionalParams["oauth_verifier"] = pin_str;
	oauth.RequestToken(ACCESS_TOKEN_URL);

	if (oauth.AccessToken.empty()) {
		errx(1, "GIVE UP");
	}

	bool r = oauth.SaveTokenToFile(tokenfile);
	if (r == false) {
		errx(1, "Token save failed");
	}
}
