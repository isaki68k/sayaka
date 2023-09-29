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
#include "StringUtil.h"
#include "UString.h"
#include "WSClient.h"
#include "subr.h"
#include <cstdio>
#include <err.h>
#include <poll.h>
#include <unistd.h>

static int misskey_stream();
static void misskey_onmsg(void *aux, wslay_event_context_ptr ctx,
	const wslay_event_on_msg_recv_arg *msg);
static bool misskey_show_note(const Json *note, int depth);
static std::string misskey_get_username(const Json& user);
static std::string misskey_get_userid(const Json& user);
static std::string misskey_get_time(const Json& note);
static bool misskey_show_icon(const Json& user, const std::string& userid);
static UString misskey_format_poll(const Json& poll);
static UString misskey_format_renote_count(const Json& note);
static UString misskey_format_reaction_count(const Json& note);
static UString misskey_format_renote_owner(const Json& note);

int
cmd_misskey_stream()
{
	for (;;) {
		int r = misskey_stream();
		if (r < 0) {
			// エラーは表示済み。
			return -1;
		}
		// 0 なら backoff ?

		sleep(1);
	}
	return 0;
}

// Misskey Streaming を行う。定期的に切れるようだ。
// 相手からの Connection Close なら 1 を返す。
// 復旧可能そうなエラーなら 0 を返す。
// 復旧不可能ならエラーなら -1 を返す。
static int
misskey_stream()
{
	WSClient client;

	if (client.Init(diagHttp, &misskey_onmsg, NULL) == false) {
		fprintf(stderr, "client Init\n");
		return -1;
	}

	std::string uri = "wss://misskey.io/streaming";
	if (client.SetURI(uri) == false) {
		fprintf(stderr, "client SetURI failed\n");
		return -1;
	}

	if (client.Connect() == false) {
		// エラーは表示済み。
		// XXX 復旧可能かどうか。
		return -1;
	}
	auto ctx = client.GetContext();

	// コマンド送信。
	std::string id = string_format("sayaka-%d", (int)time(NULL));
	std::string cmd = "{\"type\":\"connect\",\"body\":{"
		"\"channel\":\"localTimeline\",\"id\":\"" + id + "\"}}";
	client.Write(cmd.c_str(), cmd.size());

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
			fprintf(stderr, "poll: %s", strerror(errno));
			break;
		}

		if ((pfd.revents & POLLOUT)) {
		    r = wslay_event_send(ctx);
			if (r != 0) {
				fprintf(stderr, "wslay_event_send failed: %d\n", r);
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
				printf("wslay_event_recv failed: %d\n", r);
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
		return true;
	}
	const Json *obj = &obj0;

	// ストリームから来る JSON は以下のような構造。
	// {
	//   "type":"channel",
	//   "body":{
	//     "id":"ストリーム開始時に指定した ID",
	//     "type":"note",
	//     "body":{ ノート本体 }
	//   }
	// }

	// "type":"channel" と "body":{} があれば "body" の中がチャンネル。
	const Json *chan;
	if (JsonAsString((*obj)["type"]) == "channel" &&
		(*obj)["body"].is_object())
	{
		chan = &(*obj)["body"];
	} else {
		chan = obj;
	}

	// "type":"note" と "body":{} があれば "body" の中がノート。
	const Json *note;
	if (JsonAsString((*chan)["type"]) == "note" &&
		(*chan)["body"].is_object())
	{
		note = &(*chan)["body"];
	} else {
		note = chan;
	}

	bool crlf = misskey_show_note(note, 0);
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

		name = coloring(misskey_get_username(*user), Color::Username);
		userid_str = misskey_get_userid(*user);
		userid = coloring(userid_str, Color::UserId);
	}

	// インタラクションがないので CW があれば CW、なければ text というだけ。
	std::string text_str;
	std::string cw = JsonAsString((*renote)["cw"]);
	if (cw.empty() == false) {
		text_str = cw + " [CW]";
	} else {
		text_str = JsonAsString((*renote)["text"]);
	}
	UString text = UString::FromUTF8(text_str);

	ShowIcon(misskey_show_icon, *user, userid_str);
	print_(name + ' ' + userid);
	printf("\n");
	print_(text);
	printf("\n");

	// 投票(poll)
	if (renote->contains("poll") && (*renote)["poll"].is_object()) {
		UString pollstr = misskey_format_poll((*renote)["poll"]);
		if (pollstr.empty() == false) {
			print_(pollstr);
			printf("\n");
		}
	}

	// picture

	// 引用部分

	// 時刻と、あればこのノートの既 RN 数、リアクション数。
	auto time = coloring(misskey_get_time(*renote), Color::Time);
	auto rnmsg = misskey_format_renote_count(*renote);
	auto reactmsg = misskey_format_reaction_count(*renote);
	print_(time + rnmsg + reactmsg);
	printf("\n");

	// リノート元
	if (has_renote) {
		print_(misskey_format_renote_owner(*note));
		printf("\n");
	}

	return true;
}

// user からユーザ名(表示名)の文字列を取得。
static std::string
misskey_get_username(const Json& user)
{
	return JsonAsString(user["name"]);
}

// user からアカウント名(+外部ならホスト名) の文字列を取得。
// @user[@host] 形式。
static std::string
misskey_get_userid(const Json& user)
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
misskey_get_time(const Json& note)
{
	std::string createdAt = JsonAsString(note["createdAt"]);
	time_t unixtime = DecodeISOTime(createdAt);
	return formattime(unixtime);
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

	auto img_file = string_format("icon-%dx%d-%s-%08x.sixel",
		iconsize, iconsize, userid.c_str(), fnv1);
	return show_image(img_file, avatarUrl, iconsize, -1);
}

// 投票を表示用に整形して返す。
static UString
misskey_format_poll(const Json& poll)
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

// リノート数を表示用に整形して返す。
static UString
misskey_format_renote_count(const Json& note)
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
misskey_format_reaction_count(const Json& note)
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
misskey_format_renote_owner(const Json& note)
{
	std::string rn_time = misskey_get_time(note);
	std::string rn_name;
	std::string rn_userid;

	if (note.contains("user") && note["user"].is_object()) {
		const Json& user = note["user"];

		rn_name = misskey_get_username(user);
		rn_userid = misskey_get_userid(user);
	}

	auto str = string_format("%s %s %s renoted",
		rn_time.c_str(), rn_name.c_str(), rn_userid.c_str());
	return coloring(str, Color::Retweet);
}
