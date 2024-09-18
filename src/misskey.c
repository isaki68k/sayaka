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
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

// ユーザ名。毎回このセットが必要なので。
typedef struct misskey_user_ {
	string *name;		// "name"、こっちは表示名 (NULL でない)
	string *id;			// "username"、アカウント名っぽい方 (NULL でない)
	string *instance;	// "instance/name"、インスタンス名 (なければ NULL)
} misskey_user;

static bool misskey_init(void);
static bool misskey_stream(struct wsclient *, bool);
static void misskey_recv_cb(const string *);
static void misskey_message(string *);
static int  misskey_show_note(const struct json *, int);
static int  misskey_show_announcement(const struct json *, int);
static int  misskey_show_notification(const struct json *, int);
static void misskey_show_icon(const struct json *, int, const string *);
static bool misskey_show_photo(const struct json *, int, int);
static void misskey_print_filetype(const struct json *, int, const char *);
static void make_cache_filename(char *, uint, const char *);
static ustring *misskey_display_text(const struct json *, int, const char *);
static bool unichar_submatch(const unichar *, const char *);
static int  unichar_ncasecmp(const unichar *, const unichar *);
static string *misskey_format_poll(const struct json *, int);
static string *misskey_format_time(const struct json *, int);
static string *misskey_format_renote_count(const struct json *, int);
static string *misskey_format_reaction_count(const struct json *, int);
static string *misskey_format_renote_owner(const struct json *, int);
static misskey_user *misskey_get_user(const struct json *, int);
static void misskey_free_user(misskey_user *);

static struct json *global_js;

// サーバ接続とローカル再生との共通の初期化。
static bool
misskey_init(void)
{
	global_js = json_create(diag_json);
	if (global_js == NULL) {
		return false;
	}

	return true;
}

static void
misskey_cleanup(void)
{
	json_destroy(global_js);
}

void
cmd_misskey_play(const char *infile)
{
	string *s;
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

	while ((s = string_fgets(fp)) != NULL) {
		misskey_message(s);
		string_free(s);
	}

	if (infile != NULL) {
		fclose(fp);
	}

	misskey_cleanup();
}

void
cmd_misskey_stream(const char *server, bool home, const char *token)
{
	const struct diag *diag = diag_net;
	string *url;

	misskey_init();

	url = string_init();
	string_append_printf(url, "wss://%s/streaming", server);
	if (token) {
		string_append_printf(url, "?i=%s", token);
	}

	printf("Ready...");
	fflush(stdout);

	// -1 は初回。0 は EOF による正常リトライ。
	int retry_count = -1;
	for (;;) {
		enum {
			CLOSED,	// 接続後、正常に EOF になった -> 再接続
			RETRY,	// reconnect できなかった -> 再接続
			ERROR,	// エラー -> 中止
		} status;

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

		struct wsclient *ws = wsclient_create(diag);
		if (ws == NULL) {
			warn("%s: wsclient_create failed", __func__);
			break;
		}

		wsclient_init(ws, misskey_recv_cb);

		// 応答コード 101 が成功。
		int code = wsclient_connect(ws, string_get(url), &netopt_main);
		if (code != 101) {
			if (code == -2) {
				warnx("SSL not compiled");
			} else if (code < 0) {
				warn("%s: connection failed", server);
			} else if (code == 0) {
				warnx("%s: connection failed: EOF?", server);
			} else {
				warnx("%s: connection failed: HTTP %u", server, code);
			}
			if (retry_count < 0) {
				// 初回接続でエラーなら、それはエラー。再試行しない。
				status = ERROR;
			} else {
				// 接続実績はあるが、今回の接続がエラーになったら再試行。
				status = RETRY;
			}
			goto done;
		}

		// 接続成功。
		// 初回とリトライ時に表示。EOF 後の再接続では表示しない。
		if (retry_count != 0) {
			printf("Connected\n");
		}
		retry_count = 0;

		// メイン処理。
		if (misskey_stream(ws, home) == true) {
			status = CLOSED;
		} else {
			status = RETRY;
		}

 done:
		wsclient_destroy(ws);
		if (status == ERROR) {
			// エラーなら終了。メッセージは表示済み。
			break;
		}
		if (status == RETRY) {
			retry_count++;
			if (retry_count >= 5) {
				warnx("Gave up reconnecting.");
				break;
			}
		}
		sleep(1);
	}

	misskey_cleanup();
	string_free(url);
}

// Misskey Streaming の接続後メインループ。定期的に切れるようだ。
// 相手からの Connection Close なら true を返す。
// エラー (おそらく復旧不可能)なら false を返す。
static bool
misskey_stream(struct wsclient *ws, bool home)
{
	char cmd[128];

	// コマンド送信。
	snprintf(cmd, sizeof(cmd), "{\"type\":\"connect\",\"body\":{"
		"\"channel\":\"%s\",\"id\":\"%s-sayaka%08x\"}}",
		home ? "homeTimeline" : "localTimeline",
		home ? "htl" : "ltl",
		rnd_get32());
	if (wsclient_send_text(ws, cmd) < 0) {
		warn("%s: Sending command failed", __func__);
		return false;
	}

	if (home) {
		snprintf(cmd, sizeof(cmd), "{\"type\":\"connect\",\"body\":{"
			"\"channel\":\"%s\",\"id\":\"%s-sayaka%08x\"}}",
			"main", "main", rnd_get32());
		if (wsclient_send_text(ws, cmd) < 0) {
			warn("%s: Sending command failed", __func__);
			return false;
		}
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
	// 録画。
	if (__predict_false(opt_record_file)) {
		FILE *fp = fopen(opt_record_file, "a");
		if (fp) {
			fputs(string_get(msg), fp);
			fputc('\n', fp);
			fclose(fp);
		}
	}

	misskey_message(UNCONST(msg));
}

// 1メッセージの処理。ここからストリーミングとローカル再生共通。
static void
misskey_message(string *jsonstr)
{
	struct json *js = global_js;

	int n = json_parse(js, jsonstr);
	if (__predict_false(n < 0)) {
		warnx("%s: json_parse failed: %d", __func__, n);
		return;
	}
	Debug(diag_json, "%s: token = %d\n", __func__, n);

	if (__predict_false(diag_get_level(diag_format) >= 3)) {
		json_jsmndump(js);
	}

	// ストリームから来る JSON は以下のような構造。
	// {
	//   "type":"channel", "body":{
	//     "id":"ストリーム開始時に指定した ID",
	//     "type":"note", "body":{ ノート本体 }
	//   }
	// }
	// {
	//   "type":"emojiUpdated", "body":{ }
	// }
	// {
	//   "type":"announcementCreated", "body":{
	//     "announcement": { }
	//   }
	// } とかいうのも来たりする。
	//
	// main ストリームに流れてくるのはこの形式。
	// 通知 (リアクションは通知でしか来ない):
	//   "type":"channel", "body":{
	//     "id":"ストリーム開始時に指定した ID",
	//     "type":"notification", "body":{ }
	//
	//     "type":"unreadNotification", "body":{ }
	//     "type":"readAll*", "body":null
	//     "type":"driveFileCreated", "body":{ }
	//     "type":"renote", "body":{ }				// リノート (通知とは別)
	//     "type":"followed","body":{ ユーザ情報 }	// フォロー (通知とは別)
	//
	// ストリームじゃないところで取得したノートを流し込んでも
	// そのまま見えると嬉しいので、皮をむいたやつを次ステージに渡す。

	indent_depth = 0;
	int iobj = 0;
	const char *type = NULL;
	int crlf;
	// トップ階層。
	{
		int itype = json_obj_find(js, iobj, "type");
		type = json_get_cstr(js, itype);
		int ibody = json_obj_find_obj(js, iobj, "body");
		if (__predict_false(itype < 0 || ibody < 0)) {
			goto unknown;
		}
		if (strcmp(type, "channel") == 0) {
			// 下の階層へ。
			iobj = ibody;
		} else if (strcmp(type, "announcementCreated") == 0) {
			// アナウンス文。
			int iann = json_obj_find_obj(js, ibody, "announcement");
			if (iann >= 0) {
				crlf = misskey_show_announcement(js, iann);
				goto done;
			}
			goto unknown;
		} else if (strncmp(type, "emoji", 5) == 0) {
			// emoji 追加等の通知は無視。
			return;
		} else {
			goto unknown;
		}
	}

	// 2階層目。
	// "type":"channel" の "body" の下の階層。
	{
		int itype = json_obj_find(js, iobj, "type");
		if (__predict_false(itype < 0)) {
			goto unknown;
		}
		int ibody = json_obj_find_obj(js, iobj, "body");
		type = json_get_cstr(js, itype);
		if (strcmp(type, "note") == 0) {
			crlf = misskey_show_note(js, ibody);
			goto done;

		} else if (strcmp(type, "notification") == 0) {
			crlf = misskey_show_notification(js, ibody);
			goto done;

		} else if (strcmp(type, "mention") == 0 ||
		           strcmp(type, "renote") == 0 ||
		           strcmp(type, "reply") == 0 ||
		           strcmp(type, "unfollow") == 0 ||
		           strcmp(type, "follow") == 0 ||
		           strcmp(type, "followed") == 0 ||
		           strncmp(type, "read", 4) == 0 ||
		           strncmp(type, "emoji", 5) == 0 ||
		           strncmp(type, "drive", 5) == 0 ||
		           strncmp(type, "unread", 6) == 0)
		{
			Debug(diag_format, "ignore %s", type);
			return;
		} else {
			goto unknown;
		}
	}

 done:
	if (crlf > 0) {
		printf("\n");
	}
	return;

 unknown:
	if (iobj == 0) {
		if (type == NULL || type[0] == '\0') {
			printf("No message type?\n");
		} else {
			printf("Unknown message type /%s\n", type);
		}
	} else {
		const char *ptype = json_obj_find_cstr(js, 0, "type");
		printf("Unknown message type /%s/%s\n", ptype, type);
	}
}

// 1ノートを処理する。
// 戻り値は、
// 1 ならノートを表示してこの後この関数を抜けたら改行が必要。
// 0 ならノートを表示したがこの後この関数を抜けても改行は不要。
// -1 なら (NG や NSFW 等で) ノートを表示しなかったので、
//   この親もリノート行の出力は不要。その後の改行も不要。
static int
misskey_show_note(const struct json *js, int inote)
{
	if (__predict_false(diag_get_level(diag_format) >= 2)) {
		json_dump(js, inote);
	}
	assert(json_is_obj(js, inote));

	// acl

	// NG ワード

	// text は null も "" も等価?。
	// cw は cw:null なら CW なし、cw:"" なら前半パート無言で [CW] 開始、
	// のような気がする。
	// files (配列) はなければ空配列のようだ。(null は来ない?)
	//
	// ここで「公式リツイート」相当は
	//  text は null か "" かつ、
	//  cw が null かつ、
	//  files が空、
	// だろうか。

	const char *c_text = json_obj_find_cstr(js, inote, "text");
	int icw = json_obj_find(js, inote, "cw");
	if (icw >= 0 && json_is_str(js, icw) == false) {
		icw = -1;
	}
	int ifiles = json_obj_find(js, inote, "files");
	if (ifiles >= 0) {
		if (json_is_array(js, ifiles) == false || json_get_size(js, ifiles) < 1)
		{
			ifiles = -1;
		}
	}
	int irenote = json_obj_find_obj(js, inote, "renote");

	if (c_text == NULL && icw < 0 && ifiles < 0 && irenote >= 0) {
		// リノート。
		int crlf = misskey_show_note(js, irenote);

		if (crlf >= 0) {
			// リノート元。
			ustring *rnline = ustring_alloc(64);
			string *rnowner = misskey_format_renote_owner(js, inote);
			ustring_append_utf8_color(rnline, string_get(rnowner),
				COLOR_RENOTE);
			iprint(rnline);
			printf("\n");
			string_free(rnowner);
			ustring_free(rnline);
		}

		return crlf;
	}

	// ここから単独ノートか、引用付きノート。

	// --nsfw=hide なら、添付ファイルに isSensitive が一つでも含まれていれば
	// このノート自体を表示しない。
	if (opt_nsfw == NSFW_HIDE) {
		bool has_sensitive = false;
		if (ifiles >= 0) {
			JSON_ARRAY_FOR(ifile, js, ifiles) {
				has_sensitive |= json_obj_find_bool(js, ifile, "isSensitive");
			}
		}
		if (has_sensitive) {
			return -1;
		}
	}

	// 1行目は名前、アカウント名など。
	int iuser = json_obj_find_obj(js, inote, "user");
	misskey_user *user = misskey_get_user(js, inote);
	ustring *headline = ustring_alloc(64);
	ustring_append_utf8_color(headline, string_get(user->name), COLOR_USERNAME);
	ustring_append_unichar(headline, ' ');
	ustring_append_utf8_color(headline, string_get(user->id), COLOR_USERID);
	if (user->instance) {
		ustring_append_unichar(headline, ' ');
		ustring_append_utf8_color(headline, string_get(user->instance),
			COLOR_USERNAME);
	}

	// 本文。
	// cw	text	--show-cw	result			top		bottom	画像
	// ----	----	---------	-------			-----	------	----
	// -	y		n			text			text	null	y
	// -	y		y			text			text	null	y
	// y	*		n			cw [CW]			cw		null	n
	// y	*		y			cw [CW] text	cw		text	y

	// jsmn はテキストを一切加工しないので、例えば改行文字は JSON エンコードに
	// 従って '\' 'n' の2文字のまま。本文中の改行は画面でも改行してほしいので
	// ここでエスケープを処理する。一方、名前中の改行('\' 'n' の2文字) は
	// 都合がいいのでそのままにしておく (JSON パーサがテキストをデコードして
	// いた場合にはこっちを再エスケープするはずだった)。

	string *text = NULL;
	if (c_text) {
		text = json_unescape(c_text);
	}
	if (text == NULL) {
		text = string_init();
	}

	// "cw":null は CW なし、"cw":"" は前置きなしの [CW]、で意味が違う。
	// 文字列かどうかはチェック済み。
	string *cw;
	if (icw >= 0) {
		const char *c_cw = json_get_cstr(js, icw);
		cw = json_unescape(c_cw);
	} else {
		cw = NULL;
	}

	// cw, text のままだと条件が複雑なので、top と bottom ということにする。
	const string *top;
	const string *bottom = NULL;
	if (cw == NULL) {
		top = text;
	} else {
		top = cw;
		if (opt_show_cw) {
			bottom = text;
		}
	}

	ustring *textline = ustring_alloc(256);

	ustring *utop = misskey_display_text(js, inote, string_get(top));
	ustring_append(textline, utop);
	ustring_free(utop);
	if (cw) {
		ustring_append_ascii(textline, " [CW]");
		if (bottom) {
			ustring_append_unichar(textline, '\n');
		}
	}
	if (bottom) {
		ustring *ubtm = misskey_display_text(js, inote, string_get(bottom));
		ustring_append(textline, ubtm);
		ustring_free(ubtm);
	}

	misskey_show_icon(js, iuser, user->id);

	iprint(headline);
	printf("\n");
	iprint(textline);
	printf("\n");

	// これらは本文付随なので CW 以降を表示する時だけ表示する。
	if (cw == NULL || opt_show_cw) {
		// picture
		image_count = 0;
		image_next_cols = 0;
		image_max_rows = 0;
		if (ifiles >= 0) {
			JSON_ARRAY_FOR(ifile, js, ifiles) {
				print_indent(indent_depth + 1);
				// i_ がループ変数なのを知っている
				misskey_show_photo(js, ifile, i_);
				printf("\r");
			}
		}

		// 投票(poll)
		int ipoll = json_obj_find_obj(js, inote, "poll");
		if (ipoll >= 0) {
			string *pollstr = misskey_format_poll(js, ipoll);
			if (pollstr) {
				ustring *pollline = ustring_init();
				if (pollline) {
					ustring_append_utf8(pollline, string_get(pollstr));
					iprint(pollline);
					printf("\n");
					ustring_free(pollline);
				}
				string_free(pollstr);
			}
		}
	}

	// 引用部分。
	// 引用先の非表示状態はこれより親に伝搬しない。
	if (irenote >= 0) {
		indent_depth++;
		misskey_show_note(js, irenote);
		indent_depth--;
	}

	// 時刻と、あればこのノートの既 RN 数、リアクション数。
	string *time = misskey_format_time(js, inote);
	string *rnmsg = misskey_format_renote_count(js, inote);
	string *reactmsg = misskey_format_reaction_count(js, inote);

	ustring *footline = ustring_alloc(64);
	ustring_append_ascii_color(footline, string_get(time), COLOR_TIME);
	ustring_append_ascii_color(footline, string_get(rnmsg), COLOR_RENOTE);
	ustring_append_ascii_color(footline, string_get(reactmsg), COLOR_REACTION);

	iprint(footline);
	printf("\n");

	ustring_free(footline);
	string_free(time);
	string_free(rnmsg);
	string_free(reactmsg);
	ustring_free(textline);
	string_free(cw);
	string_free(text);
	ustring_free(headline);
	misskey_free_user(user);
	return 1;
}

// アナウンス文を処理する。構造が全然違う。
static int
misskey_show_announcement(const struct json *js, int inote)
{
	ustring *line = ustring_alloc(16);

	// "icon":"info" はどうしたらいいんだ…。
	printf(" *\r");

	ustring_append_ascii_color(line, "announcement", COLOR_USERNAME);
	iprint(line);
	printf("\n");

	const char *title = json_obj_find_cstr(js, inote, "title");
	const char *text  = json_obj_find_cstr(js, inote, "text");
	ustring_clear(line);
	if (title && title[0]) {
		string *unescape_title = json_unescape(title);
		ustring_append_utf8(line, string_get(unescape_title));
		ustring_append_unichar(line, '\n');
		ustring_append_unichar(line, '\n');
		string_free(unescape_title);
	}
	if (text && text[0]) {
		string *unescape_text = json_unescape(text);
		ustring_append_utf8(line, string_get(unescape_text));
		string_free(unescape_text);
	}
	iprint(line);
	printf("\n");

	const char *imageUrl = json_obj_find_cstr(js, inote, "imageUrl");
	if (imageUrl) {
		// picture
		char img_file[PATH_MAX];
		make_cache_filename(img_file, sizeof(img_file), imageUrl);

		image_count = 0;
		image_next_cols = 0;
		image_max_rows = 0;
		print_indent(1);
		show_image(img_file, imageUrl, imagesize, imagesize, false, 0);
		printf("\r");
	}

	// 時間は updatedAt と createdAt があるので順に探す。
	const char *at = json_obj_find_cstr(js, inote, "updatedAt");
	if (at == NULL) {
		at = json_obj_find_cstr(js, inote, "createdAt");
	}
	if (at) {
		time_t unixtime = decode_isotime(at);
		string *time = format_time(unixtime);
		ustring_clear(line);
		ustring_append_ascii_color(line, string_get(time), COLOR_TIME);
		iprint(line);
		printf("\n");
		string_free(time);
	}

	ustring_free(line);
	return 1;
}

// notification を処理する。
// ibody は "type":"notification", "body":{ } の部分。
static int
misskey_show_notification(const struct json *js, int ibody)
{
	// "type":"notification", "body":{
	// {
	//   "type":"reaction",
	//   "createdAt":"",
	//   "user":{ },
	//   "note":{ },
	//   "reaction":"..."
	// }
	// {
	//   "type":"renote",
	//   "createdAt":"",
	//   "user":{ },
	//   "note":{ },
	// }
	// {
	//   "type":"followed",
	//   "user":{ },
	// }

	int itype = json_obj_find(js, ibody, "type");
	if (itype < 0) {
		printf("notification but has no type?\n");
		return 0;
	}
	const char *type = json_get_cstr(js, itype);
	if (strcmp(type, "reaction") == 0) {
		// リアクション通知。
		int inote = json_obj_find_obj(js, ibody, "note");
		if (inote < 0) {
			printf("notification/reaction but has no note?\n");
			return 0;
		}
		misskey_show_note(js, inote);

		string *time = misskey_format_time(js, ibody);
		misskey_user *user = misskey_get_user(js, ibody);
		const char *reaction = json_obj_find_cstr(js, ibody, "reaction");

		string *s = string_init();
		string_append_cstr(s, string_get(time));
		string_append_char(s, ' ');
		string_append_cstr(s, reaction);
		string_append_cstr(s, " from ");
		string_append_cstr(s, string_get(user->name));
		string_append_char(s, ' ');
		string_append_cstr(s, string_get(user->id));
		if (user->instance) {
			string_append_char(s, ' ');
			string_append_cstr(s, string_get(user->instance));
		}
		ustring *u = ustring_alloc(64);
		ustring_append_utf8_color(u, string_get(s), COLOR_REACTION);
		iprint(u);
		printf("\n");
		ustring_free(u);
		misskey_free_user(user);
		string_free(time);
		return 1;
	}
	if (strcmp(type, "follow") == 0) {
		// フォローされた通知。
		// フォローされたは "channel/followed" だが、
		// その通知は "channel/notification/follow" (followed でない) らしい。
		string *time = misskey_format_time(js, ibody);
		misskey_user *user = misskey_get_user(js, ibody);

		ustring *u = ustring_alloc(128);
		ustring_append_utf8_color(u, string_get(time), COLOR_TIME);
		ustring_append_unichar(u, ' ');
		ustring_append_ascii(u, "Followed by ");
		ustring_append_utf8_color(u, string_get(user->name), COLOR_USERNAME);
		ustring_append_unichar(u, ' ');
		ustring_append_utf8_color(u, string_get(user->id), COLOR_USERID);
		if (user->instance) {
			ustring_append_unichar(u, ' ');
			ustring_append_utf8_color(u, string_get(user->instance),
				COLOR_USERNAME);
		}
		iprint(u);
		printf("\n");
		ustring_free(u);
		misskey_free_user(user);
		string_free(time);
		return 1;
	}
	if (strcmp(type, "mention") == 0 ||
		strcmp(type, "renote") == 0 ||
		strcmp(type, "reply") == 0)
	{
		// もうちょっと場合分けが必要かも。
		Debug(diag_format, "ignore notification/%s", type);
		return 0;
	}
	if (strcmp(type, "followRequestAccepted") == 0)
	{
		// いる?
		return 0;
	}

	printf("Unknown notification type \"%s\"\n", type);
	return 0;
}

// アイコン表示。
static void
misskey_show_icon(const struct json *js, int iuser, const string *userid)
{
	const struct diag *diag = diag_image;

	if (diag_get_level(diag) == 0) {
		// 改行x3 + カーソル上移動x3 を行ってあらかじめスクロールを
		// 発生させ、アイコン表示時にスクロールしないようにしてから
		// カーソル位置を保存する
		// (スクロールするとカーソル位置復元時に位置が合わない)
		printf("\n\n\n" CSI "3A" ESC "7");

		// インデント。
		if (indent_depth > 0) {
			print_indent(indent_depth);
		}
	}

	bool shown = false;
	if (__predict_true(opt_show_image)) {
		char filename[PATH_MAX];
		const char *avatar_url = json_obj_find_cstr(js, iuser, "avatarUrl");
		if (avatar_url && userid) {
			// URL の FNV1 ハッシュをキャッシュのキーにする。
			// Misskey の画像 URL は長いのと URL がネストした構造を
			// しているので単純に一部を切り出して使う方法は無理。
			snprintf(filename, sizeof(filename), "icon-%s-%u-%s-%08x",
				colorname, fontheight,
				string_get(userid), hash_fnv1a(avatar_url));
			shown = show_image(filename, avatar_url, iconsize, iconsize,
				false, -1);
		}

		if (shown == false) {
			const char *avatar_blurhash =
				json_obj_find_cstr(js, iuser, "avatarBlurhash");
			if (avatar_blurhash) {
				char url[256];
				snprintf(filename, sizeof(filename), "icon-%s-%u-%s-%08x",
					colorname, fontheight,
					string_get(userid), hash_fnv1a(avatar_blurhash));
				snprintf(url, sizeof(url), "blurhash://%s", avatar_blurhash);
				shown = show_image(filename, url, iconsize, iconsize,
					false, -1);
			}
		}
	}

	if (__predict_true(shown)) {
		if (diag_get_level(diag) == 0) {
			printf(
				// アイコン表示後、カーソル位置を復帰。
				"\r"
				// カーソル位置保存/復元に対応していない端末でも動作するように
				// カーソル位置復元前にカーソル上移動x3を行う。
				CSI "3A" ESC "8"
			);
		}
	} else {
		// アイコンを表示してない場合はここで代替アイコンを表示。
		printf(" *\r");
	}
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
misskey_show_photo(const struct json *js, int ifile, int index)
{
	char img_file[PATH_MAX];
	char urlbuf[256];
	const char *filetype_msg = "";
	const char *img_url;
	uint width = 0;
	uint height = 0;
	bool shade;
	bool shown = false;

	if (opt_show_image) {
		bool isSensitive = json_obj_find_bool(js, ifile, "isSensitive");
		if (isSensitive && opt_nsfw != NSFW_SHOW) {
			const char *blurhash = json_obj_find_cstr(js, ifile, "blurhash");
			if (blurhash == NULL || blurhash[0] == '\0' ||
				opt_nsfw == NSFW_ALT)
			{
				// 画像でないなど Blurhash がない、あるいは --nsfw=alt なら、
				// ファイルタイプだけでも表示しておくか。
				filetype_msg = " [NSFW]";
				goto next;
			}
			int iproperties = json_obj_find_obj(js, ifile, "properties");
			if (iproperties >= 0) {
				width  = json_obj_find_int(js, iproperties, "width");
				height = json_obj_find_int(js, iproperties, "height");

				// 原寸のアスペクト比を維持したまま長辺が imagesize になる
				// ようにする。
				// image_reduct() には入力画像サイズとしてこのサイズを、
				// 出力画像サイズも同じサイズを指定することで等倍で動作させる。
				if (width > height) {
					height = height * imagesize / width;
					width = imagesize;
				} else {
					width = width * imagesize / height;
					height = imagesize;
				}
			}
			if (width < 1) {
				width = imagesize;
			}
			if (height < 1) {
				height = imagesize;
			}
			snprintf(urlbuf, sizeof(urlbuf), "blurhash://%s", blurhash);
			img_url = urlbuf;
			shade = true;
		} else {
			// 元画像を表示。thumbnailUrl を使う。
			img_url = json_obj_find_cstr(js, ifile, "thumbnailUrl");
			if (img_url == NULL || img_url[0] == '\0') {
				// なければ、ファイルタイプだけでも表示しとく?
				goto next;
			}
			width  = imagesize;
			height = imagesize;
			shade = false;
		}
		make_cache_filename(img_file, sizeof(img_file), img_url);
		shown = show_image(img_file, img_url, width, height, shade, index);
	}

 next:
	if (shown == false) {
		misskey_print_filetype(js, ifile, filetype_msg);
	}
	return shown;
}

// 改行してファイルタイプだけを出力する。
static void
misskey_print_filetype(const struct json *js, int ifile, const char *msg)
{
	image_count = 0;
	image_max_rows = 0;
	image_next_cols = 0;

	const char *type = json_obj_find_cstr(js, ifile, "type");
	if (type == NULL) {
		type = "no filetype?";
	}
	printf("\r");
	print_indent(indent_depth + 1);
	printf("(%s)%s\n", type, msg);
}

// 画像 URL からキャッシュファイル名を作成して返す。
// "file-<color>-<fontheight>-<url>"(.sixel)
static void
make_cache_filename(char *filename, uint bufsize, const char *url)
{
	snprintf(filename, bufsize, "file-%s-%u-%s", colorname, fontheight, url);

	// 面倒な文字を置換しておく。
	for (char *p = filename; *p; p++) {
		if (strchr(":/()? ", *p)) {
			*p = '_';
		}
	}
}

// 本文を表示用に整形。
static ustring *
misskey_display_text(const struct json *js, int inote, const char *text)
{
	const struct diag *diag = diag_format;
	ustring *src = ustring_from_utf8(text);
	ustring *dst = ustring_alloc(strlen(text));

	if (__predict_false(diag_get_level(diag) >= 2)) {
		ustring_dump(src, "display_text src");
	}

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
	ustring **tags = NULL;
	uint tagcount = 0;
	int itags = json_obj_find(js, inote, "tags");
	if (itags >= 0 && json_is_array(js, itags)) {
		tagcount = json_get_size(js, itags);
		if (tagcount > 0) {
			tags = calloc(tagcount, sizeof(ustring *));
			JSON_ARRAY_FOR(itag, js, itags) {
				if (json_is_str(js, itag)) {
					const char *tag = json_get_cstr(js, itag);
					ustring *utag = ustring_from_utf8(tag);
					ustring_tolower_inplace(utag);
					tags[i_] = utag;
				}
			}
		}
	}
	if (diag_get_level(diag) >= 2) {
		diag_print(diag, "tagcount=%u", tagcount);
		for (uint i = 0; i < tagcount; i++) {
			printf("tags[%u] ", i);
			if (tags[i]) {
				string *s = ustring_to_utf8(tags[i]);
				printf("|%s|\n", string_get(s));
				string_free(s);
			} else {
				printf("null\n");
			}
		}
	}

	const unichar *srcarray = ustring_get(src);
	int mfmtag = 0;
	for (int pos = 0, posend = ustring_len(src); pos < posend; ) {
		unichar c = srcarray[pos];

		if (c == '<') {
			if (unichar_submatch(&srcarray[pos + 1], "plain>")) {
				// <plain> なら閉じ </plain> を探す。
				pos += 7;
				int e;
				// ループは本当は posend-7 くらいまでで十分だが、閉じタグが
				// なければ最後まで plain 扱いにするために posend まで回す。
				for (e = pos; e < posend; e++) {
					if (srcarray[e] == '<' &&
						unichar_submatch(&srcarray[e + 1], "/plain>"))
					{
						break;
					}
				}
				// この間は無加工で出力。
				for (; pos < e; pos++) {
					ustring_append_unichar(dst, srcarray[pos]);
				}
				pos += 8;
				continue;
			}
			// 他の HTML タグはとりあえず放置。

		} else if (c == '$' && ustring_at(src, pos + 1) == '[') {
			// MFM タグ開始。
			int e = pos + 2;
			// タグは全部無視するのでタグ名をスキップ。
			for (; e < posend && srcarray[e] != ' '; e++)
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
			// '@' の直前が ment2 でなく(?)、直後が ment1 ならメンション。
			unichar pc = ustring_at(src, pos - 1);
			unichar nc = ustring_at(src, pos + 1);
			bool prev_is_ment2 =
				(pc != 0 && pc < 0x80 && strchr(ment2chars, pc) != NULL);
			bool next_is_ment1 =
				(nc != 0 && nc < 0x80 && strchr(ment1chars, nc) != NULL);
			if (prev_is_ment2 == false && next_is_ment1 == true) {
				ustring_append_ascii(dst, color_begin(COLOR_USERID));
				ustring_append_unichar(dst, c);
				ustring_append_unichar(dst, nc);
				// 2文字目以降はホスト名も来る可能性がある。
				for (pos += 2; pos < posend; pos++) {
					c = srcarray[pos];
					if (c < 0x80 && strchr(ment2chars, c) != NULL) {
						ustring_append_unichar(dst, c);
					} else {
						break;
					}
				}
				ustring_append_ascii(dst, color_end(COLOR_USERID));
				continue;
			}

		} else if (c == '#') {
			// タグはこの時点で範囲(長さ)が分かるのでステート分岐不要。
			int i = 0;
			for (; i < tagcount; i++) {
				const unichar *utag = tags[i] ? ustring_get(tags[i]) : NULL;
				if (unichar_ncasecmp(&srcarray[pos + 1], utag) == 0) {
					break;
				}
			}
			if (i != tagcount) {
				// 一致したらタグ。'#' 文字自身も含めてコピーする。
				ustring_append_ascii(dst, color_begin(COLOR_TAG));
				ustring_append_unichar(dst, c);
				pos++;
				uint len = ustring_len(tags[i]);
				// tags は正規化によって何が起きてるか分からないので、
				// posend のほうを信じる。
				uint end = MIN(pos + len, (int)posend);
				Trace(diag, "tag[%d] found at pos=%u len=%u end=%u",
					i, pos, len, end);
				for (; pos < end; pos++) {
					ustring_append_unichar(dst, srcarray[pos]);
				}
				ustring_append_ascii(dst, color_end(COLOR_TAG));
				continue;
			}

		} else if (c == 'h' &&
			(unichar_submatch(&srcarray[pos], "https://") ||
			 unichar_submatch(&srcarray[pos], "http://")))
		{
			// URL
			int url_in_paren = 0;
			ustring_append_ascii(dst, color_begin(COLOR_URL));
			for (; pos < posend; pos++) {
				// URL に使える文字集合がよく分からない。
				// 括弧 "(",")" は、開き括弧なしで閉じ括弧が来ると URL 終了。
				// 一方開き括弧は URL 内に来てもよい。
				// "(http://foo/a)b" は http://foo/a が URL。
				// "http://foo/a(b)c" は http://foo/a(b)c が URL。
				// 正気か?
				c = srcarray[pos];
				if (c < 0x80 && strchr(urlchars, c) != NULL) {
					ustring_append_unichar(dst, c);
				} else if (c == '(') {
					url_in_paren++;
					ustring_append_unichar(dst, c);
				} else if (c == ')' && url_in_paren > 0) {
					url_in_paren--;
					ustring_append_unichar(dst, c);
				} else {
					break;
				}
			}
			ustring_append_ascii(dst, color_end(COLOR_URL));
			continue;
		}

		// どれでもなければここに落ちてくる。
		ustring_append_unichar(dst, c);
		pos++;
	}

	for (uint i = 0; i < tagcount; i++) {
		ustring_free(tags[i]);
	}
	free(tags);
	ustring_free(src);

	if (__predict_false(diag_get_level(diag) >= 2)) {
		ustring_dump(dst, "dst");
	}
	return dst;
}

// ustring (の中身の配列) u 以降が key と部分一致すれば true を返す。
static bool
unichar_submatch(const unichar *u, const char *key)
{
	for (uint i = 0; key[i]; i++) {
		if (u[i] == '\0') {
			return false;
		}
		if (u[i] != key[i]) {
			return false;
		}
	}
	return true;
}

// ustring (の中身の配列) u1 以降が u2 の長さ分だけ大文字小文字を無視して
// 一致すれば 0 を返す。一致しなければ大小を返すはずだがそこは適当。
// 便宜上 u2 は NULL も受け付ける (tags[] が NULL の可能性が一応あるので)。
static int
unichar_ncasecmp(const unichar *u1, const unichar *u2)
{
	assert(u1);

	if (u2 == NULL) {
		return 1;
	}

	for (uint i = 0; u2[i]; i++) {
		if (u1[i] == '\0') {
			return 1;
		}
		unichar c1 = u1[i];
		unichar c2 = u2[i];
		if ('A' <= c1 && c1 <= 'Z') {
			c1 += 0x20;
		}
		if ('A' <= c2 && c2 <= 'Z') {
			c2 += 0x20;
		}
		if (c1 != c2) {
			return 1;
		}
	}
	return 0;
}

// 投票を表示用に整形して返す。
static string *
misskey_format_poll(const struct json *js, int ipoll)
{
	// "poll" : {
	//   "choices" : [ { choice1, choice2 } ],
	//   "expiresAt" : null (or string?),
	//   "multiple" : bool,
	// }

	string *s = string_init();
	int ichoices = json_obj_find(js, ipoll, "choices");
	if (ichoices >= 0 && json_is_array(js, ichoices)) {
		// choice は {
		//   "isVoted" : bool (自分が投票したかかな?)
		//   "text" : string
		//   "votes" : number
		// }

		// 本当は列整形したいところだが表示文字数のカウントが面倒。

		// 整形。
		JSON_ARRAY_FOR(ichoice, js, ichoices) {
			bool voted = json_obj_find_bool(js, ichoice, "isVoted");
			const char *c_text = json_obj_find_cstr(js, ichoice, "text");
			string *text = json_unescape(c_text);
			int votes = json_obj_find_int(js, ichoice, "votes");

			string_append_printf(s, " [%c] %s : %u\n",
				(voted ? '*' : ' '),
				(text ? string_get(text) : ""),
				votes);
			string_free(text);
		}
	}
	// 最後の改行は除く。
	string_rtrim_inplace(s);

	return s;
}

// note オブジェクトから表示用時刻文字列を取得。
static string *
misskey_format_time(const struct json *js, int inote)
{
	int icreatedAt = json_obj_find(js, inote, "createdAt");
	if (__predict_true(icreatedAt >= 0 && json_is_str(js, icreatedAt))) {
		const char *createdat = json_get_cstr(js, icreatedAt);
		time_t unixtime = decode_isotime(createdat);
		return format_time(unixtime);
	} else {
		return string_init();
	}
}

// リノート数を表示用に整形して返す。
static string *
misskey_format_renote_count(const struct json *js, int inote)
{
	string *s;
	uint rncnt = 0;

	int irenoteCount = json_obj_find(js, inote, "renoteCount");
	if (irenoteCount >= 0) {
		rncnt = json_get_int(js, irenoteCount);
	}

	if (rncnt > 0) {
		char buf[32];
		snprintf(buf, sizeof(buf), " %uRN", rncnt);
		s = string_from_cstr(buf);
	} else {
		s = string_init();
	}
	return s;
}

// リアクション数を表示用に整形して返す。
static string *
misskey_format_reaction_count(const struct json *js, int inote)
{
	string *s;
	uint cnt = 0;

	int ireactions = json_obj_find(js, inote, "reactions");
	if (ireactions >= 0) {
		// reactions: { "name1":cnt1, "name2":cnt2, ... }
		// の cnt だけをカウントする。
		JSON_OBJ_FOR(ikey, js, ireactions) {
			int ival = ikey + 1;
			cnt += json_get_int(js, ival);
		}
	}

	if (cnt > 0) {
		char buf[32];
		snprintf(buf, sizeof(buf), " %uReact", cnt);
		s = string_from_cstr(buf);
	} else {
		s = string_init();
	}
	return s;
}

// リノート元通知を表示用に整形して返す。
static string *
misskey_format_renote_owner(const struct json *js, int inote)
{
	string *s = string_init();
	string *rn_time = misskey_format_time(js, inote);
	misskey_user *rn_user = misskey_get_user(js, inote);

	string_append_cstr(s, string_get(rn_time));
	string_append_cstr(s, " Renoted by ");
	string_append_cstr(s, string_get(rn_user->name));
	string_append_char(s, ' ');
	string_append_cstr(s, string_get(rn_user->id));
	if (rn_user->instance) {
		string_append_char(s, ' ');
		string_append_cstr(s, string_get(rn_user->instance));
	}

	string_free(rn_time);
	misskey_free_user(rn_user);
	return s;
}

// ノートのユーザ情報を返す。
static misskey_user *
misskey_get_user(const struct json *js, int inote)
{
	misskey_user *user = calloc(1, sizeof(*user));
	if (user == NULL) {
		return NULL;
	}
	user->name = string_init();
	user->id   = string_init();

	int iuser = json_obj_find_obj(js, inote, "user");
	if (iuser >= 0) {
		const char *c_name     = json_obj_find_cstr(js, iuser, "name");
		const char *c_username = json_obj_find_cstr(js, iuser, "username");
		const char *c_host     = json_obj_find_cstr(js, iuser, "host");

		// ユーザ名 は name だが、空なら username を使う仕様のようだ。
		if (c_name && c_name[0] != '\0') {
			// XXX テキスト中に制御文字が含まれてたらとかはまた後で考える。
			string *tmp = json_unescape(c_name);
			string_append_cstr(user->name, string_get(tmp));
			string_free(tmp);
		} else {
			// こっちは ID っぽいやつなのでおかしな文字はいないはず。
			string_append_cstr(user->name, c_username);
		}

		// @アカウント名 [ @外部ホスト名 ]
		string_append_char(user->id, '@');
		string_append_cstr(user->id, c_username);
		if (c_host) {
			string_append_char(user->id, '@');
			string_append_cstr(user->id, c_host);
		}

		// インスタンス名
		int iinstance = json_obj_find_obj(js, iuser, "instance");
		if (iinstance >= 0) {
			const char *c_instname = json_obj_find_cstr(js, iinstance, "name");
			if (c_instname && c_instname[0] != '\0') {
				user->instance = json_unescape(c_instname);
			}
		}
	}

	return user;
}

static void
misskey_free_user(misskey_user *user)
{
	if (user) {
		string_free(user->name);
		string_free(user->id);
		string_free(user->instance);
	}
}
