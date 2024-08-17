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

static bool misskey_stream(struct wsclient *);
static void misskey_recv_cb(const string *);

void
cmd_misskey_stream(const char *server)
{
	const struct diag *diag = diag_net;
	char url[strlen(server) + 20];

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

		struct wsclient *ws = wsclient_create(diag);
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
}

// Misskey Streaming の接続後メインループ。定期的に切れるようだ。
// 相手からの Connection Close なら true を返す。
// エラー (おそらく復旧不可能)なら false を返す。
static bool
misskey_stream(struct wsclient *ws)
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
	printf("%s\n", string_get(msg));
}
