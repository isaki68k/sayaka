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
#include "Json.h"
#include "NGWord.h"
#include <array>
#include <cstdint>
#include <string>
#include <vector>

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

extern void init_color();
extern void print_(const UString& utext);
extern UString ColorBegin(Color col);
extern UString ColorEnd(Color col);
extern UString coloring(const std::string& text, Color col);
extern bool show_image(const std::string& img_file, const std::string& img_url,
	int resize_width, int index);
extern void record(const Json& obj);

extern int  address_family;
extern UseSixel use_sixel;
extern int  color_mode;
extern bool opt_protect;
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
extern bool opt_show_ng;
extern std::string opt_ngword;
extern std::string opt_ngword_user;
extern std::string record_file;
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
extern std::string myid;
extern bool opt_nocolor;
extern int  opt_record_mode;
extern bool opt_mathalpha;
extern bool opt_nocombine;
extern std::string basedir;
extern std::string cachedir;
extern std::string colormapdir;
extern Proto proto;

extern StringDictionary followlist;
extern StringDictionary blocklist;
extern StringDictionary mutelist;
extern StringDictionary nortlist;
