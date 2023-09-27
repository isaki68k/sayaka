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

static void misskey_onmsg(void *aux, wslay_event_context_ptr ctx,
	const wslay_event_on_msg_recv_arg *msg);
static bool misskey_show_note(const Json *note, int depth);
static std::string misskey_format_time(const std::string&);
static bool misskey_show_icon(const Json& user, const std::string& userid);

int
cmd_misskey_stream()
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
		return -1;
	}
	auto ctx = client.GetContext();

	// コマンド送信。
	std::string id = string_format("sayaka-%d", (int)time(NULL));
	std::string cmd = "{\"type\":\"connect\",\"body\":{"
		"\"channel\":\"localTimeline\",\"id\":\"" + id + "\"}}";
printf("cmd=|%s|\n", cmd.c_str());
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
#if 1
		printf("poll(%s%s)\n",
			((pfd.events & POLLIN) ? "IN" : ""),
			((pfd.events & POLLOUT) ? "OUT" : ""));
#endif

		while ((r = poll(&pfd, 1, -1)) < 0 && errno == EINTR)
			;
		if (r < 0) {
			fprintf(stderr, "poll: %s", strerror(errno));
			return -1;
		}
#if 1
		printf("revents=%s%s\n",
			((pfd.revents & POLLIN) ? "IN" : ""),
			((pfd.revents & POLLOUT) ? "OUT" : ""));
#endif

		if ((pfd.revents & POLLOUT)) {
printf("wslay_event_send\n");
		    r = wslay_event_send(ctx);
			if (r != 0) {
				fprintf(stderr, "wslay_event_send failed: %d\n", r);
				break;
			}
		}
		if ((pfd.revents & POLLIN)) {
printf("wslay_event_recv\n");
			r = wslay_event_recv(ctx);
			if (r == WSLAY_ERR_CALLBACK_FAILURE) {
				printf("EOF\n");
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

		name = coloring(JsonAsString((*user)["name"]), Color::Username);

		userid_str = "@" + JsonAsString((*user)["username"]);
		std::string host = JsonAsString((*user)["host"]);
		if (host.empty() == false) {
			userid_str += "@" + host;
		}
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

	std::string createdAt = JsonAsString((*renote)["createdAt"]);
	UString time = coloring(misskey_format_time(createdAt), Color::Time);

	ShowIcon(misskey_show_icon, *user, userid_str);
	print_(name + ' ' + userid);
	printf("\n");
	print_(text);
	printf("\n");
	print_(time);
	printf("\n");

	(void)has_renote;
	return true;
}

// createdAt の時刻 (ISO 形式文字列) を表示用に整形する。
std::string
misskey_format_time(const std::string& createdAt)
{
	time_t unixtime = DecodeISOTime(createdAt);
	return formattime(unixtime);
}

// アイコン表示のサービス固有部コールバック。
static bool
misskey_show_icon(const Json& user, const std::string& userid)
{
	std::string avatarUrl = JsonAsString(user["avatarUrl"]);

	if (avatarUrl.empty() == false && userid.empty() == false) {
		// URL のファイル名部分をキャッシュのキーにする。
		auto p = avatarUrl.rfind('/');
		if (__predict_true(p >= 0)) {
			auto img_file = string_format("icon-%dx%d-%s-%s",
				iconsize, iconsize, userid.c_str(),
				avatarUrl.c_str() + p + 1);
			if (show_image(img_file, avatarUrl, iconsize, -1)) {
				return true;
			}
		}
	}
	return false;
}
