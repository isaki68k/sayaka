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

#include "header.h"
#include "Diag.h"
#include "Dictionary.h"
#include "JsonFwd.h"
#if 0
#include "NGWord.h"
#endif
#include <string>

#define DEBUG_FORMAT 1

enum Color {
	Username,
	UserId,
	Time,
	Source,

	Retweet,
	Favorite,
	Url,
	Tag,
	Verified,
	Protected,
	NG,
	Max,
};

enum bgtheme {
	BG_NONE = -1,
	BG_DARK = 0,
	BG_LIGHT = 1,
};

enum class Proto {
	None = 0,
	Twitter,
	Misskey,
};

enum class StreamMode {
	Home,
	Local,
	Global,
};

class UString;

static const int ColorFixedX68k = -1;

extern void cmd_play();

extern void record(const char *str);
extern void record(const Json& obj);

extern int  address_family;
extern int  color_mode;
extern bool opt_protect;
extern Diag diag;
extern Diag diagHttp;
extern Diag diagImage;
extern Diag diagShow;
extern bool opt_debug_format;
extern int  opt_debug_sixel;
extern int  screen_cols;
extern int  fontwidth;
extern int  fontheight;
extern int  iconsize;
extern int  imagesize;
extern int  indent_cols;
extern int  indent_depth;
#if 0
extern bool opt_show_ng;
extern std::string opt_ngword;
extern std::string opt_ngword_user;
extern NGWordList ngword_list;
#endif
extern std::string last_id;
extern int  last_id_count;
extern int  last_id_max;
extern std::string opt_ciphers;
extern bool opt_progress;
extern bool opt_ormode;
extern bool opt_output_palette;
extern int  opt_record_mode;
extern bool opt_nocombine;
extern bool opt_show_cw;
extern bool opt_show_nsfw;
extern std::string basedir;
extern std::string cachedir;
extern Proto opt_proto;
extern StreamMode opt_stream;
extern std::string opt_server;

#if defined(USE_TWITTER)
extern std::string myid;
extern StringDictionary followlist;
extern StringDictionary blocklist;
extern StringDictionary mutelist;
extern StringDictionary nortlist;
#endif
