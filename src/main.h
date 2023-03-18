/*
 * Copyright (C) 2014-2021 Tetsuya Isaki
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

#pragma once

#include "Diag.h"
#include "Dictionary.h"
#include "NGWord.h"
#include "OAuth.h"
#include <array>
#include <string>
#include <vector>

#define DEBUG_FORMAT 1

#define AUTHORIZE_URL		"https://twitter.com/oauth/authorize"
#define ACCESS_TOKEN_URL	"https://api.twitter.com/oauth/access_token"
#define REQUEST_TOKEN_URL	"https://api.twitter.com/oauth/request_token"
#define APIROOT				"https://api.twitter.com/1.1/"
#define STREAM_APIROOT		"https://stream.twitter.com/1.1/"

enum bgcolor {
	BG_NONE = -1,
	BG_BLACK = 0,
	BG_WHITE = 1,
};

// use_sixel
enum class UseSixel {
	AutoDetect = -1,
	No = 0,
	Yes = 1,
};

// sayaka.cpp で定義されてるやつだけど、sayaka.h はグローバルなのでちょっと
// どうすべ。
extern void cmd_tweet();
extern void cmd_stream();
extern void cmd_play();
extern StringDictionary get_follow_list();
extern StringDictionary get_block_list();
extern StringDictionary get_mute_list();
extern StringDictionary get_nort_list();
extern void init_color();

extern int  address_family;
extern UseSixel use_sixel;
extern int  color_mode;
extern Diag diag;
extern Diag diagHttp;
extern Diag diagImage;
extern Diag diagShow;
extern bool opt_debug_format;
extern int  opt_debug_sixel;
extern int  screen_cols;
extern int  opt_fontwidth;
extern int  opt_fontheight;
extern int  fontwidth;
extern int  fontheight;
extern int  iconsize;
extern int  imagesize;
extern int  indent_cols;
extern int  indent_depth;
extern int  max_image_count;
extern int  image_count;
extern int  image_next_cols;
extern int  image_max_rows;
extern enum bgcolor bgcolor;
extern std::string output_codeset;
extern OAuth oauth;
extern bool opt_show_ng;
extern std::string opt_ngword;
extern std::string opt_ngword_user;
extern std::string record_file;
extern std::vector<std::string> opt_filter;
extern std::string last_id;
extern int  last_id_count;
extern int  last_id_max;
extern bool in_sixel;
extern std::string opt_ciphers;
extern bool opt_full_url;
extern bool opt_progress;
extern NGWordList ngword_list;
extern bool opt_ormode;
extern bool opt_output_palette;
extern int  opt_timeout_image;
extern bool opt_pseudo_home;
extern std::string myid;
extern bool opt_nocolor;
extern int  opt_record_mode;
extern int  opt_reconnect;
extern bool opt_mathalpha;
extern std::string basedir;
extern std::string cachedir;
extern std::string tokenfile;
extern std::string colormapdir;

extern void InitOAuth();

// 起動経過を表示 (遅マシン用)
static inline void
progress(const char *msg)
{
	if (__predict_false(diag >= 1) || __predict_false(opt_progress)) {
		fputs(msg, stdout);
		fflush(stdout);
	}
}
