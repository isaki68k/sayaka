/* vi:set ts=4: */
/*
 * Copyright (C) 2023-2024 Tetsuya Isaki
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
// Misskey
//

#include "sayaka.h"
#include <err.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static bool misskey_init(void);
static bool misskey_stream(wsclient *);
static void misskey_recv_cb(const string *);
static void misskey_message(string *);
static bool misskey_show_note(const json *, int, uint);
static bool misskey_show_announcement(const json *, int);
static const char *misskey_get_username(const json *, int, string *, string *);
static string *misskey_format_time(const json *, int);

static json *js;

// サーバ接続とローカル再生との共通の初期化。
static bool
misskey_init(void)
{
	js = json_create(diag_json);
	if (js == NULL) {
		return false;
	}

	return true;
}

static void
misskey_cleanup(void)
{
	json_destroy(js);
}

void
cmd_misskey_play(const char *infile)
{
	char buf[8192];	// XXX
	FILE *fp;

	misskey_init();

	if (infile == NULL) {
		fp = stdin;
	} else {
		fp = fopen(infile, "r");
		if (fp == NULL) {
			err(1, "%s", infile);
		}
	}

	while (fgets(buf, sizeof(buf), fp) != NULL) {
		string *s = string_from_cstr(buf);
		misskey_message(s);
		string_free(s);
	}

	if (infile != NULL) {
		fclose(fp);
	}

	misskey_cleanup();
}

void
cmd_misskey_stream(const char *server)
{
	const diag *diag = diag_net;
	char url[strlen(server) + 20];

	misskey_init();

	snprintf(url, sizeof(url), "wss://%s/streaming", server);

	printf("Ready...");
	fflush(stdout);

	// -1 は初回。0 は EOF による正常リトライ。
	int retry_count = -1;
	bool terminate = false;
	for (;;) {
		if (retry_count > 0) {
			time_t now;
			struct tm tm;
			char timebuf[16];

			time(&now);
			localtime_r(&now, &tm);
			strftime(timebuf, sizeof(timebuf), "%T", &tm);
			printf("%s Retrying...", timebuf);
			fflush(stdout);
		}

		wsclient *ws = wsclient_create(diag);
		if (ws == NULL) {
			Debug(diag, "%s: wsclient_create failed", __func__);
			goto abort;
		}

		if (wsclient_init(ws, misskey_recv_cb) == false) {
			Debug(diag, "%s: wsclient_init failed", __func__);
			goto abort;
		}

		if (wsclient_connect(ws, url) != 0) {
			Debug(diag, "%s: %s: wsclient_connect failed", __func__, server);
			goto abort;
		}

		// 接続成功。
		// 初回とリトライ時に表示。EOF 後の再接続では表示しない。
		if (retry_count != 0) {
			printf("Connected\n");
		}

		// メイン処理。
		if (misskey_stream(ws) == false) {
			// エラーなら終了。メッセージは表示済み。
			terminate = true;
			goto abort;
		}

		retry_count = 0;
 abort:
		wsclient_destroy(ws);
		if (terminate) {
			break;
		}
		// 初回で失敗か、リトライ回数を超えたら終了。
		if (retry_count < 0) {
			break;
		}
		if (++retry_count >= 5) {
			warnx("Gave up reconnecting.");
			break;
		}
		sleep(1 << retry_count);
	}

	misskey_cleanup();
}

// Misskey Streaming の接続後メインループ。定期的に切れるようだ。
// 相手からの Connection Close なら true を返す。
// エラー (おそらく復旧不可能)なら false を返す。
static bool
misskey_stream(wsclient *ws)
{
	// コマンド送信。
	char cmd[128];
	snprintf(cmd, sizeof(cmd), "{\"type\":\"connect\",\"body\":{"
		"\"channel\":\"localTimeline\",\"id\":\"sayaka-%08x\"}}",
		rnd_get32());

	if (wsclient_send_text(ws, cmd) < 0) {
		warn("%s: Sending command failed", __func__);
		return false;
	}

	// あとは受信。メッセージが出来ると misskey_recv_cb() が呼ばれる。
	for (;;) {
		int r = wsclient_process(ws);
		if (__predict_false(r <= 0)) {
			if (r < 0) {
				warn("%s: wsclient_process failed", __func__);
				break;
			} else {
				// EOF
				return true;
			}
		}
	}

	return false;
}

// サーバから1メッセージ (以上?)を受信したコールバック。
static void
misskey_recv_cb(const string *msg)
{
	misskey_message(UNCONST(msg));
}

// 1メッセージの処理。ここからストリーミングとローカル再生共通。
static void
misskey_message(string *jsonstr)
{
	int n = json_parse(js, jsonstr);
	Debug(diag_json, "token = %d\n", n);

	//json_jsmndump(js);

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
	// }
	// {
	//   "type":"announcementCreated",
	//   "body":{
	//     "announcement": { }
	//   }
	// } とかいうのも来たりする。
	//
	// ストリームじゃないところで取得したノートを流し込んでも
	// そのまま見えると嬉しいので、皮をむいたやつを次ステージに渡す。

	int id = 0;
	for (;;) {
		int typeid = json_obj_find(js, id, "type");
		int bodyid = json_obj_find(js, id, "body");
		if (typeid >= 0 && json_is_str(js, typeid) &&
			bodyid >= 0 && json_is_obj(js, bodyid))
		{
			const char *typestr = json_get_cstr(js, typeid);
			if (strcmp(typestr, "channel") == 0 ||
				strcmp(typestr, "note") == 0 ||
				strcmp(typestr, "announcementCreated") == 0)
			{
				// "body" の下へ。
				id = bodyid;
			} else if (strncmp(typestr, "emoji", 5) == 0) {
				// emoji{Added,Deleted} とかは無視でいい。
				return;
			} else {
				// 知らないタイプは無視。
				warnx("Unknown message type \"%s\"", typestr);
				return;
			}
		} else {
			// ここが本文っぽい。
			break;
		}
	}

	bool crlf = misskey_show_note(js, id, 0);
	if (crlf) {
		printf("\n");
	}
}

// 1ノートを処理する。
static bool
misskey_show_note(const json *js, int inote, uint depth)
{
	json_dump(js, inote);
	assert(json_is_obj(js, inote));

	// acl

	// 録画?
	// 階層変わるのはどうする?

	// NG ワード

	// アナウンスなら別処理。
	int iann = json_obj_find(js, inote, "announcement");
	if (iann >= 0 && json_is_obj(js, iann)) {
		return misskey_show_announcement(js, iann);
	}

	// 地文なら note == renote。
	// リノートなら RN 元を note、RN 先を renote。
	bool has_renote;
	int irenote = json_obj_find(js, inote, "renote");
	if (irenote >= 0 && json_is_obj(js, irenote)) {
		// XXX text があったらどうするのかとか。
		has_renote = true;
	} else {
		irenote = inote;
		has_renote = false;
	}

	const char *name = NULL;
	string *userid = string_init();
	string *instname = string_init();
	int iuser = json_obj_find(js, irenote, "user");
	if (iuser >= 0 && json_is_obj(js, iuser)) {
		name = misskey_get_username(js, iuser, userid, instname);
	}

	// XXX CW
	const char *text = NULL;
	int itext = json_obj_find(js, irenote, "text");
	if (__predict_true(itext >= 0 && json_is_str(js, itext))) {
		text = json_get_cstr(js, itext);
	}

	// ShowIcon
	// XXX print_ 未復旧
	printf("%s", name);
	printf(" ");
	printf("%s", string_get(userid));
	if (string_len(instname) != 0) {
		printf("%s", string_get(instname));
	}
	printf("\n");
	if (text) {
		printf("\t");
		printf("%s", text);
		printf("\n");
	}

	// これらは本文付随なので CW 以降を表示する時だけ表示する。

	// 引用部分

	// 時刻と、あればこのノートの既 RN 数、リアクション数。
	string *time = misskey_format_time(js, irenote);
	//rnmsg = misskey_display_renote_count(js, irenote);
	//rectmsg = misskey_display_reaction_count(js, irenote);

	printf("\t");
	printf("%s", string_get(time));
	printf("\n");

	// リノート元

	(void)has_renote;
	return true;
}

// アナウンス文を処理する。構造が全然違う。
static bool
misskey_show_announcement(const json *js, int inote)
{
	printf("%s not yet\n", __func__);
	abort();
	return true;
}

// user オブジェクトから
// o ユーザ名 (表示名、user->name)、
// o アカウント名 (user->username) + ホスト名(user->host)、
// o インスタンス名 (instance->name)
// の文字列を取得する。
// ユーザ名は const char * で得られるので戻り値で返す。
// アカウント名、インスタンス名は初期化済みの string * に書き出す。
static const char *
misskey_get_username(const json *js, int iuser,
	string *userid, string *instance_name)
{
	const char *dispname;
	const char *name;
	const char *username;
	const char *host;
	const char *instname;

	int iname = json_obj_find(js, iuser, "name");
	if (__predict_true(iname >= 0 && json_is_str(js, iname))) {
		name = json_get_cstr(js, iname);
	} else {
		name = "";
	}

	int iusername = json_obj_find(js, iuser, "username");
	if (__predict_true(iusername >= 0 && json_is_str(js, iusername))) {
		username = json_get_cstr(js, iusername);
	} else {
		username = NULL;
	}

	int ihost = json_obj_find(js, iuser, "host");
	if (ihost >= 0 && json_is_str(js, ihost)) {
		host = json_get_cstr(js, ihost);
	} else {
		host = NULL;
	}

	instname = NULL;
	int iinstance = json_obj_find(js, iuser, "instance");
	if (iinstance >= 0 && json_is_obj(js, iinstance)) {
		int iinstname = json_obj_find(js, iinstance, "name");
		if (iinstname >= 0 && json_is_str(js, iinstname)) {
			instname = json_get_cstr(js, iinstname);
		}
	}

	// ユーザ名(表示名) は name。name が空なら username を使う仕様のようだ。

	if (__predict_true(name[0] != '\0')) {
		dispname = name;
	} else {
		dispname = username;
	}

	// ユーザ ID は '@' + username。
	// 外部サーバなら '@' + host を追加。

	string_append_char(userid, '@');
	if (__predict_true(username)) {
		string_append_cstr(userid, username);
	}
	if (host) {
		string_append_char(userid, '@');
		string_append_cstr(userid, host);
	}

	// インスタンス名。
	if (instname) {
		string_append_cstr(instance_name, " [");
		string_append_cstr(instance_name, instname);
		string_append_char(instance_name, ']');
	}

	return dispname;
}

// note オブジェクトから表示用時刻文字列を取得。
static string *
misskey_format_time(const json *js, int inote)
{
	const char *createdat;
	int icreatedAt = json_obj_find(js, inote, "createdAt");
	if (__predict_true(icreatedAt >= 0 && json_is_str(js, icreatedAt))) {
		createdat = json_get_cstr(js, icreatedAt);
		time_t unixtime = decode_isotime(createdat);
		return format_time(unixtime);
	} else {
		return string_init();
	}
}
