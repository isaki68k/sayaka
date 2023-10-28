/*
 * Copyright (C) 2023 Tetsuya Isaki
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
#include "Display.h"
#include "JsonInc.h"
#include "Misskey.h"
#include "Random.h"
#include "StringUtil.h"
#include "UString.h"
#include "WSClient.h"
#include "subr.h"
#include "term.h"
#include <cstdio>
#include <err.h>
#include <poll.h>
#include <unistd.h>

static bool misskey_stream(WSClient&, Random&);
static void misskey_onmsg(void *aux, wslay_event_context_ptr ctx,
	const wslay_event_on_msg_recv_arg *msg);
static bool misskey_show_note(const Json *note, int depth);
static std::string misskey_format_username(const Json& user);
static std::string misskey_format_userid(const Json& user);
static void UString_tolower(UString& str);
static int UString_ncasecmp(const UString& src, int pos, const UString& key);
static UString misskey_display_text(const std::string& text, const Json& note);
static std::string misskey_format_time(const Json& note);
static bool misskey_show_icon(const Json& user, const std::string& userid);
static UString misskey_display_poll(const Json& poll);
static bool misskey_show_photo(const Json& f, int resize_width, int index);
static void misskey_print_filetype(const Json& f, const char *nsfw);
static UString misskey_display_renote_count(const Json& note);
static UString misskey_display_reaction_count(const Json& note);
static UString misskey_display_renote_owner(const Json& note);

int
cmd_misskey_stream()
{
	Random rnd;

	std::string uri = "wss://" + opt_server + "/streaming";
	printf("Ready...");
	fflush(stdout);

	// -1 は初回。0 は EOF による(正常)リトライ。
	int retry_count = -1;
	for (;;) {
		if (__predict_false(retry_count > 0)) {
			time_t now = GetUnixTime();
			struct tm tm;
			localtime_r(&now, &tm);
			char timebuf[16];
			strftime(timebuf, sizeof(timebuf), "%T", &tm);

			printf("%s Retrying...", timebuf);
			fflush(stdout);
		}

		WSClient client(rnd, diagHttp);
		if (client.Init(&misskey_onmsg, NULL) == false) {
			warn("WebSocket initialization failed");
			goto abort1;
		}

		if (__predict_false(client.Open(uri) == false)) {
			warnx("WebSocket open failed: %s", uri.c_str());
			goto abort2;
		}

		if (__predict_false(client.Connect() == false)) {
			int code = client.GetHTTPCode();
			if (code > 0) {
				warnx("Connection failed: %s responded with %d",
					opt_server.c_str(), code);
			} else {
				warnx("WebSocket connection failed");
			}
			goto abort2;
		}

		// 接続成功。
		// 初回とリトライ時に表示。EOF 後の再接続では表示しない。
		if (retry_count != 0) {
			printf("Connected\n");
		}

		// メイン処理。
		if (misskey_stream(client, rnd) == false) {
			// エラーなら終了。メッセージは表示済み。
			return -1;
		}
		client.Close();
		retry_count = 0;
		sleep(1);
		continue;

 abort2:
		client.Close();
 abort1:
		// 初回で失敗か、リトライ回数を超えたら終了。
		if (retry_count < 0) {
			return -1;
		}
		if (++retry_count >= 5) {
			warnx("Gave up reconnecting.");
			return -1;
		}
		sleep(1 << retry_count);
	}
	return 0;
}

// Misskey Streaming の接続後メインループ。定期的に切れるようだ。
// 相手からの Connection Close なら true を返す。
// エラー (おそらく復旧不可能)なら false を返す。
static bool
misskey_stream(WSClient& client, Random& rnd)
{
	// コマンド送信。
	std::string id = string_format("sayaka-%08x", rnd.Get());
	std::string cmd = "{\"type\":\"connect\",\"body\":{"
		"\"channel\":\"localTimeline\",\"id\":\"" + id + "\"}}";
	if (client.Write(cmd.c_str(), cmd.size()) < 0) {
		warn("%s: Sending command failed", __func__);
		return false;
	}

	// あとは受信。
	struct pollfd pfd;
	pfd.fd = client.GetFd();

	auto ctx = client.GetContext();
	for (;;) {
		int r;

		pfd.events = 0;
		if (wslay_event_want_read(ctx)) {
			pfd.events |= POLLIN;
		}
		if (wslay_event_want_write(ctx)) {
			pfd.events |= POLLOUT;
		}
		if (pfd.events == 0) {
			warnx("%s: Event request empty?", __func__);
			break;
		}

		while ((r = poll(&pfd, 1, -1)) < 0 && errno == EINTR)
			;
		if (r < 0) {
			warn("%s: poll", __func__);
			break;
		}

		if ((pfd.revents & POLLOUT)) {
		    r = wslay_event_send(ctx);
			if (r != 0) {
				warnx("%s: wslay_event_send failed: %d", __func__, r);
				break;
			}
		}
		if ((pfd.revents & POLLIN)) {
			r = wslay_event_recv(ctx);
			if (r == WSLAY_ERR_CALLBACK_FAILURE) {
				// EOF
				return true;
			}
			if (r != 0) {
				warnx("%s: wslay_event_recv failed: %d", __func__, r);
				break;
			}
		}
	}
	return false;
}

// メッセージ受信コールバック。
static void
misskey_onmsg(void *aux, wslay_event_context_ptr ctx,
	const wslay_event_on_msg_recv_arg *msg)
{
	if (msg->msg_length == 0) {
		return;
	}

	std::string line((const char *)msg->msg, msg->msg_length);

	if (opt_record_mode == 2) {
		record(line.c_str());
	}
	misskey_show_object(line);
}

// 1ノート(文字列)を処理する。
bool
misskey_show_object(const std::string& line)
{
	Json obj0;
	try {
		obj0 = Json::parse(line);
	} catch (const std::exception& e) {
		warnx("%s: %s\ninput line is |%s|", __func__, e.what(), line.c_str());
		return true;
	}
	if (obj0.is_object() == false) {
		warnx("%s: Unexpected line |%s|", __func__, obj0.dump().c_str());
		return true;
	}

	// ストリームから来る JSON は以下のような構造。
	// {
	//   "type":"channel",
	//   "body":{
	//     "id":"ストリーム開始時に指定した ID",
	//     "type":"note",
	//     "body":{ ノート本体 }
	//   }
	// }
	// {
	//   "type":"emojiUpdated",
	//   "body":{ }
	// } とかいうのも来たりする。
	//
	// ストリームじゃないところで取得したノートを流し込んでも
	// そのまま見えると嬉しいので、皮をむいたやつを次ステージに渡す。

	const Json *obj = &obj0;
	for (;;) {
		if (obj->contains("type") && (*obj)["type"].is_string() &&
			obj->contains("body") && (*obj)["body"].is_object())
		{
			auto type = JsonAsString((*obj)["type"]);
			if (type == "channel" || type == "note") {
				// "type" が channel か note なら "body" の下へ。
				obj = &(*obj)["body"];
			} else if (strncmp(type.c_str(), "emoji", 5) == 0) {
				// emoji{Added,Deleted} とかは無視でいい。
				return true;
			} else {
				// 知らないタイプは無視。
				warnx("Unknown message type \"%s\": %s",
					type.c_str(), obj0.dump().c_str());
				return true;
			}
		} else {
			// ここが本文っぽい。
			break;
		}
	}

	bool crlf = misskey_show_note(obj, 0);
	if (crlf) {
		printf("\n");
	}
	return true;
}

// 1ノート(Json)を処理する。
static bool
misskey_show_note(const Json *note, int depth)
{
	assert(note->is_object());

	// acl

	// 録画?
	// 階層変わるのはどうする?

	// NG ワード

	// 地文なら note == renote。
	// リノートなら RN 元を note、RN 先を renote。
	const Json *renote;
	bool has_renote = false;
	if (note->contains("renote") && (*note)["renote"].is_object()) {
		// XXX text があったらどうするのかとか。
		renote = &(*note)["renote"];
		has_renote = true;
	} else {
		renote = note;
		has_renote = false;
	}

	const Json& nullobj = Json(nullptr);
	const Json *user = &nullobj;
	std::string userid_str;
	UString name;
	UString userid;
	UString instance_name;
	if (renote->contains("user") && (*renote)["user"].is_object()) {
		user = &(*renote)["user"];

		name = coloring(misskey_format_username(*user), Color::Username);
		userid_str = misskey_format_userid(*user);
		userid = coloring(userid_str, Color::UserId);

		if (user->contains("instance") && (*user)["instance"].is_object()) {
			const Json *instance = &(*user)["instance"];
			std::string iname = JsonAsString((*instance)["name"]);
			instance_name = UString(" ") +
				coloring("[" + iname + "]", Color::Username);
		}
	}

	// cw	text	--show-cw	display
	// ----	----	---------	-------
	// -	y		n			text
	// -	y		y			text
	// y	*		n			cw [CW]
	// y	*		y			cw [CW] text
	std::string cw_str = JsonAsString((*renote)["cw"]);
	std::string text_str;
	if (cw_str.empty() || opt_show_cw) {
		text_str = JsonAsString((*renote)["text"]);
	}
	UString text;
	if (cw_str.empty() == false) {
		text += misskey_display_text(cw_str, *renote);
		text.AppendASCII(" [CW]");
		if (opt_show_cw) {
			text += '\n';
			text += misskey_display_text(text_str, *renote);
		}
	} else {
		text = misskey_display_text(text_str, *renote);
	}

	ShowIcon(misskey_show_icon, *user, userid_str);
	print_(name + ' ' + userid + instance_name);
	printf("\n");
	print_(text);
	printf("\n");

	// これらは本文付随なので CW 以降を表示する時だけ表示する。
	if (cw_str.empty() || opt_show_cw) {
		// picture
		image_count = 0;
		image_next_cols = 0;
		image_max_rows = 0;
		if (renote->contains("files") && (*renote)["files"].is_array()) {
			const Json& files = (*renote)["files"];
			for (int i = 0, end = files.size(); i < end; i++) {
				const Json& f = files[i];

				auto indent = (indent_depth + 1) * indent_cols;
				printf(CSI "%dC", indent);
				misskey_show_photo(f, imagesize, i);
				printf("\r");
			}
		}

		// 投票(poll)
		if (renote->contains("poll") && (*renote)["poll"].is_object()) {
			UString pollstr = misskey_display_poll((*renote)["poll"]);
			if (pollstr.empty() == false) {
				print_(pollstr);
				printf("\n");
			}
		}
	}

	// 引用部分

	// 時刻と、あればこのノートの既 RN 数、リアクション数。
	auto time = coloring(misskey_format_time(*renote), Color::Time);
	auto rnmsg = misskey_display_renote_count(*renote);
	auto reactmsg = misskey_display_reaction_count(*renote);
	print_(time + rnmsg + reactmsg);
	printf("\n");

	// リノート元
	if (has_renote) {
		print_(misskey_display_renote_owner(*note));
		printf("\n");
	}

	return true;
}

// user からユーザ名(表示名)の文字列を取得。
static std::string
misskey_format_username(const Json& user)
{
	// name が空なら username を使う仕様。
	std::string name = JsonAsString(user["name"]);
	return !name.empty() ? name : JsonAsString(user["username"]);
}

// user からアカウント名(+外部ならホスト名) の文字列を取得。
// @user[@host] 形式。
static std::string
misskey_format_userid(const Json& user)
{
	std::string userid = "@" + JsonAsString(user["username"]);
	std::string host = JsonAsString(user["host"]);
	if (host.empty() == false) {
		userid += "@" + host;
	}
	return userid;
}

// str の ASCII 大文字を小文字にインプレース変換します。
static void
UString_tolower(UString& str)
{
	for (int i = 0, end = str.size(); i < end; i++) {
		unichar c = str[i];
		if ('A' <= c && c <= 'Z') {
			str[i] = c + 0x20;
		}
	}
}

// src の pos 文字目からが key と ASCII 大文字小文字を無視して一致するか。
// key は英字をすべて小文字にしてあること。
// 一致すれば 0 を、一致しなければ 0 以外を返す(大小は再現していない)。
static int
UString_ncasecmp(const UString& src, int pos, const UString& key)
{
	int len = key.size();
	for (int i = 0; i < len; i++) {
		unichar s = src[pos + i];

		if ('A' <= s && s <= 'Z') s += 0x20;
		if (s != key[i]) {
			return 1;
		}
	}
	return 0;
}

// 本文を表示用に整形。
static UString
misskey_display_text(const std::string& text, const Json& note)
{
	UString src = UString::FromUTF8(text);
	//printf("src=%s\n", src.dump().c_str());
	UString dst;

	// 記号をどれだけ含むかだけが違う。
	// Mention 1文字目は   "_" + Alnum
	// Mention 2文字目以降 "_" + Alnum + "@.-"
	// URL は              "_" + Alnum + "@.-" + "#%&/:;=?^~"
	static const char urlchars[] =
		"#%&/:;=?^~"
		"@.-"
		"_0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
	#define ment2chars (urlchars + 9)
	#define ment1chars (urlchars + 9 + 3)

	// タグを集めて小文字にしておく。
	std::vector<UString> tags;
	if (note.contains("tags") && note["tags"].is_array()) {
		for (auto tagj : note["tags"]) {
			if (tagj.is_string()) {
				auto tag = tagj.get<std::string>();
				auto utag = UString::FromUTF8(tag);
				UString_tolower(utag);
				tags.emplace_back(utag);
			}
		}
	}

	int mfmtag = 0;
	for (int pos = 0, posend = src.size(); pos < src.size(); ) {
		auto c = src[pos];

		if (c == '<') {
			if (src.SubMatch(pos + 1, "plain>")) {
				// <plain> なら閉じ </plain> を探す。
				pos += 7;
				int e;
				// ループは本当は posend-7 くらいまでで十分だが、閉じタグが
				// なければ最後まで plain 扱いにするために posend まで回す。
				for (e = pos; e < posend; e++) {
					if (src[e] == '<' && src.SubMatch(e + 1, "/plain>")) {
						break;
					}
				}
				// この間は無加工で出力。
				for (; pos < e; pos++) {
					dst += src[pos];
				}
				pos += 8;
				continue;
			}
			// 他の HTML タグはとりあえず放置。

		} else if (c == '$' && src.At(pos + 1) == '[') {
			// MFM タグ開始。
			int e = pos + 2;
			// タグは全部無視するのでタグ名をスキップ。
			for (; e < posend && src[e] != ' '; e++)
				;
			// 空白の次から ']' の手前までが本文。
			mfmtag++;
			pos = e + 1;
			continue;

		} else if (c == ']' && mfmtag > 0) {
			// MFM タグ終端。
			mfmtag--;
			pos++;
			continue;

		} else if (c == '@') {
			// '@' の次が [\w\d_] ならメンション。
			auto nc = src.At(pos + 1);
			if (nc < 0x80 && strchr(ment1chars, nc) != NULL) {
				dst += ColorBegin(Color::UserId);
				dst += c;
				dst += nc;
				// 2文字目以降はホスト名も来る可能性がある。
				for (pos += 2; pos < posend; pos++) {
					c = src[pos];
					if (c < 0x80 && strchr(ment2chars, c) != NULL) {
						dst += c;
					} else {
						break;
					}
				}
				dst += ColorEnd(Color::UserId);
				continue;
			}

		} else if (c == '#') {
			// タグはこの時点で範囲(長さ)が分かるのでステート分岐不要。
			int i = 0;
			int tagcount = tags.size();
			for (; i < tagcount; i++) {
				if (UString_ncasecmp(src, pos + 1, tags[i]) == 0) {
					// 複数回同じタグがあるかも知れないので念のため
					// tags から削除はしないでおく。
					break;
				}
			}
			if (i != tagcount) {
				// 一致したらタグ。'#' 文字自身も含めてコピーする。
				dst += ColorBegin(Color::Tag);
				dst += c;
				pos++;
				int end = pos + tags[i].size();
				// tags は正規化によって何が起きてるか分からないので、
				// src.size() のほうを信じる。
				end = std::min(end, (int)posend);
				for (; pos < end; pos++) {
					dst += src[pos];
				}
				dst += ColorEnd(Color::Tag);
				continue;
			}

		} else if (c == 'h' &&
			(src.SubMatch(pos, "https://") ||
			 src.SubMatch(pos, "http://")))
		{
			// URL
			int url_in_paren = 0;
			dst += ColorBegin(Color::Url);
			for (; pos < posend; pos++) {
				// URL に使える文字集合がよく分からない。
				// 括弧 "(",")" は、開き括弧なしで閉じ括弧が来ると URL 終了。
				// 一方開き括弧は URL 内に来てもよい。
				// "(http://foo/a)b" は http://foo/a が URL。
				// "http://foo/a(b)c" は http://foo/a(b)c が URL。
				// 正気か?
				c = src[pos];
				if (c < 0x80 && strchr(urlchars, c) != NULL) {
					dst += c;
				} else if (c == '(') {
					url_in_paren++;
					dst += c;
				} else if (c == ')' && url_in_paren > 0) {
					url_in_paren--;
					dst += c;
				} else {
					break;
				}
			}
			dst += ColorEnd(Color::Url);
			continue;
		}

		// どれでもなければここに落ちてくる。
		dst += c;
		pos++;
	}

	//printf("dst=%s\n", dst.dump().c_str());
	return dst;
}

// note から時刻の文字列を取得。
static std::string
misskey_format_time(const Json& note)
{
	std::string createdAt = JsonAsString(note["createdAt"]);
	time_t unixtime = DecodeISOTime(createdAt);
	return format_time(unixtime);
}

// アイコン表示のサービス固有部コールバック。
static bool
misskey_show_icon(const Json& user, const std::string& userid)
{
	std::string avatarUrl = JsonAsString(user["avatarUrl"]);

	if (avatarUrl.empty() || userid.empty()) {
		return false;
	}

	// URL の FNV1 ハッシュをキャッシュのキーにする。
	// Misskey の画像 URL は長いのと URL がネストした構造をしているので
	// 単純に一部を切り出して使う方法は無理。
	uint32 fnv1 = FNV1(avatarUrl);

	auto img_file = string_format("icon-%dx%d-%s-%08x",
		iconsize, iconsize, userid.c_str(), fnv1);
	return ShowImage(img_file, avatarUrl, iconsize, -1);
}

// 投票を表示用に整形して返す。
static UString
misskey_display_poll(const Json& poll)
{
	// "poll" : {
	//   "choices" : [ { choice1, choice2 } ],
	//   "expiresAt" : null (or string?),
	//   "multiple" : bool,
	// }

	std::string str;
	if (poll.contains("choices") && poll["choices"].is_array()) {
		const Json& choices = poll["choices"];
		// choice は {
		//   "isVoted" : bool (自分が投票したかかな?)
		//   "text" : string
		//   "votes" : number
		// }

		// 本当は列整形したいところだが表示文字数のカウントが面倒。

		// 整形。
		for (const Json& choice : choices) {
			bool voted = (choice["isVoted"].is_boolean() &&
				choice["isVoted"].get<bool>());
			std::string text = JsonAsString(choice["text"]);
			int votes = (choice["votes"].is_number()
				? choice["votes"].get<int>() : 0);

			str += string_format(
				" [%c] %s : %d\n",
				(voted ? '*' : ' '),
				text.c_str(),
				votes);
		}
	}
	// 最後の改行は除く。
	string_rtrim(str);

	return UString::FromUTF8(str);
}

// "files" : [ file1, file2, ... ]
// file は {
//   "blurhash" : "...",
//   "isSensitive" : bool,
//   "name" : string,
//   "properties" : {
//     "width" : int,
//     "height" : int,
//   },
//   "size" : int,
//   "thumbnailUrl" : "...",
//   "type" : "image/jpeg",
//   "url" : "...",
// }
static bool
misskey_show_photo(const Json& f, int resize_width, int index)
{
	std::string img_url;
	std::string img_file;

	bool isSensitive = JsonAsBool(f["isSensitive"]);
	if (isSensitive && opt_show_nsfw == false) {
		auto blurhash = JsonAsString(f["blurhash"]);
		if (blurhash.empty()) {
			// 画像でないなど Blurhash がなければ
			// ファイルタイプだけでも表示しておくか。
			misskey_print_filetype(f, " [NSFW]");
			return false;
		}
		int width = 0;
		int height = 0;
		if (f.contains("properties") && f["properties"].is_object()) {
			const Json& p = f["properties"];
			width  = JsonAsInt(p["width"]);
			height = JsonAsInt(p["height"]);

			// 原寸のアスペクト比を維持したまま長編が resize_width になる
			// ようにする。
			// SixelConverter には入力画像サイズとしてこのサイズを、
			// 出力画像サイズも同じサイズを指定することで等倍で動作させる。
			if (width > height) {
				height = height * resize_width / width;
				width = resize_width;
			} else {
				width = width * resize_width / height;
				height = resize_width;
			}
		}
		if (width < 1) {
			width = resize_width;
		}
		if (height < 1) {
			height = resize_width;
		}
		// Json オブジェクトでエンコードも出来るけど、このくらいならええやろ。
		img_url = string_format(R"(blurhash://{"hash":"%s","w":%d,"h":%d})",
			blurhash.c_str(), width, height);
		img_file = string_format("blurhash-%s-%d-%d",
			UrlEncode(blurhash).c_str(), width, height);
	} else {
		// thumbnailUrl があればそっちを使う。
		img_url = JsonAsString(f["thumbnailUrl"]);
		if (img_url.empty()) {
			// なければ、ファイルタイプだけでも表示しとく?
			misskey_print_filetype(f, "");
			return false;
		}
		img_file = GetCacheFilename(img_url);
	}
	return ShowImage(img_file, img_url, resize_width, index);
}

// 改行してファイルタイプだけを出力する。
static void
misskey_print_filetype(const Json& f, const char *nsfw)
{
	image_count = 0;
	image_max_rows = 0;
	image_next_cols = 0;

	auto type = JsonAsString(f["type"]);
	printf("\r" CSI "%dC(%s)%s\n",
		(indent_depth + 1) * indent_cols, type.c_str(), nsfw);
}

// リノート数を表示用に整形して返す。
static UString
misskey_display_renote_count(const Json& note)
{
	UString str;

	auto rncnt = note.contains("renoteCount") ?
		note.value("renoteCount", 0) : 0;
	if (rncnt > 0) {
		str = coloring(string_format(" %dRN", rncnt), Color::Retweet);
	}
	return str;
}

// リアクション数を表示用に整形して返す。
static UString
misskey_display_reaction_count(const Json& note)
{
	UString str;
	int cnt = 0;

	if (note.contains("reactions") && note["reactions"].is_object()) {
		const Json& reactions = note["reactions"];
		for (auto& [key, val] : reactions.items()) {
			if (val.is_number()) {
				cnt += val.get<int>();
			}
		}
	}

	if (cnt > 0) {
		str = coloring(string_format(" %dReact", cnt), Color::Favorite);
	}
	return str;
}

// リノート元通知を表示用に整形して返す。
static UString
misskey_display_renote_owner(const Json& note)
{
	std::string rn_time = misskey_format_time(note);
	std::string rn_name;
	std::string rn_userid;

	if (note.contains("user") && note["user"].is_object()) {
		const Json& user = note["user"];

		rn_name = misskey_format_username(user);
		rn_userid = misskey_format_userid(user);
	}

	auto str = string_format("%s %s %s renoted",
		rn_time.c_str(), rn_name.c_str(), rn_userid.c_str());
	return coloring(str, Color::Retweet);
}
