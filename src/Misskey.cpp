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

static int misskey_stream(bool);
static void misskey_onmsg(void *aux, wslay_event_context_ptr ctx,
	const wslay_event_on_msg_recv_arg *msg);
static bool misskey_show_note(const Json *note, int depth);
static std::string misskey_format_username(const Json& user);
static std::string misskey_format_userid(const Json& user);
static std::string misskey_format_time(const Json& note);
static bool misskey_show_icon(const Json& user, const std::string& userid);
static UString misskey_display_poll(const Json& poll);
static bool misskey_show_photo(const Json& f, int resize_width, int index);
static UString misskey_display_renote_count(const Json& note);
static UString misskey_display_reaction_count(const Json& note);
static UString misskey_display_renote_owner(const Json& note);

int
cmd_misskey_stream()
{
	bool is_first = true;

	printf("Ready...");
	fflush(stdout);

	for (;;) {
		int r = misskey_stream(is_first);
		if (r < 0) {
			// エラーは表示済み。
			return -1;
		}
		// 0 なら backoff ?

		sleep(1);

		is_first = false;
	}
	return 0;
}

// Misskey Streaming を行う。定期的に切れるようだ。
// 相手からの Connection Close なら 1 を返す。
// 復旧可能そうなエラーなら 0 を返す。
// 復旧不可能ならエラーなら -1 を返す。
static int
misskey_stream(bool is_first)
{
	Random rnd;

	WSClient client(rnd, diagHttp);
	if (client.Init(&misskey_onmsg, NULL) == false) {
		warn("%s: WebSocket initialization failed", __func__);
		return -1;
	}

	std::string uri = "wss://" + opt_server + "/streaming";
	if (client.Open(uri) == false) {
		warnx("%s: WebSocket open failed: %s", __func__, uri.c_str());
		return -1;
	}

	if (client.Connect() == false) {
		warnx("%s: WebSocket connection failed: %s", __func__, uri.c_str());
		return -1;
	}
	auto ctx = client.GetContext();

	// コマンド送信。
	std::string id = string_format("sayaka-%08x", rnd.Get());
	std::string cmd = "{\"type\":\"connect\",\"body\":{"
		"\"channel\":\"localTimeline\",\"id\":\"" + id + "\"}}";
	if (client.Write(cmd.c_str(), cmd.size()) < 0) {
		warn("%s: Sending command failed", __func__);
		return -1;
	}

	if (is_first) {
		printf("Connected.\n");
	}

	// あとは受信。
	struct pollfd pfd;
	pfd.fd = client.GetFd();

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
				break;
			}
			if (r != 0) {
				warnx("%s: wslay_event_recv failed: %d", __func__, r);
				break;
			}
		}
	}

	return 0;
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
	if (renote->contains("user") && (*renote)["user"].is_object()) {
		user = &(*renote)["user"];

		name = coloring(misskey_format_username(*user), Color::Username);
		userid_str = misskey_format_userid(*user);
		userid = coloring(userid_str, Color::UserId);
	}

	// cw	text	--show-cw	display
	// ----	----	---------	-------
	// -	y		n			text
	// -	y		y			text
	// y	*		n			cw [CW]
	// y	*		y			cw [CW] text
	std::string text_str;
	std::string cw = JsonAsString((*renote)["cw"]);
	if (cw.empty() == false) {
		text_str = cw + " [CW]";
		if (opt_show_cw) {
			text_str += "\n";
			text_str += JsonAsString((*renote)["text"]);
		}
	} else {
		text_str = JsonAsString((*renote)["text"]);
	}
	UString text = UString::FromUTF8(text_str);

	ShowIcon(misskey_show_icon, *user, userid_str);
	print_(name + ' ' + userid);
	printf("\n");
	print_(text);
	printf("\n");

	// これらは本文付随なので CW 以降を表示する時だけ表示する。
	if (cw.empty() || opt_show_cw) {
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
			// どうする?
			printf("[NSFW]\n");
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
			img_url = JsonAsString(f["url"]);
			if (img_url.empty()) {
				return false;
			}
		}
		img_file = GetCacheFilename(img_url);
	}
	return ShowImage(img_file, img_url, resize_width, index);
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
