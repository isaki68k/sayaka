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
#include "ProtoMisskey.h"
#include "StringUtil.h"
#include "WSClient.h"
#include <cstdio>
#include <poll.h>

int
cmd_misskey_stream()
{
	WSClient client;

	std::string uri = "wss://misskey.io/streaming";
	if (client.Init(diagHttp, uri) == false) {
		fprintf(stderr, "client Init\n");
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

printf("CanRead=%d\n", client.CanRead());
			if (client.CanRead()) {
				char buf[4096];
				r = client.Read(buf, sizeof(buf));
				printf("buf=|%s| %d\n", buf, r);
			}
		}
	}

	return 0;
}
