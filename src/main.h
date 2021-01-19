#pragma once

#include "Diag.h"
#include "Dictionary.h"
#include "NGWord.h"
#include "Twitter.h"
#include <array>
#include <string>

#define DEBUG_FORMAT 1

// sayaka.cpp で定義されてるやつだけど、sayaka.h はグローバルなのでちょっと
// どうすべ。
extern void cmd_tweet();
extern void cmd_stream();
extern void cmd_play();
extern void CreateTwitter();
extern void get_follow_list();
extern void get_block_list();
extern void get_mute_list();
extern void get_nort_list();
extern void init_color();

extern int  address_family;
extern bool opt_noimage;
extern int  color_mode;
extern Diag diag;
extern Diag diagHttp;
extern Diag diagImage;
extern Diag diagShow;
extern bool opt_debug_format;
extern int  opt_debug_sixel;
extern bool opt_debug;
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
extern bool bg_white;
extern std::string iconv_tocode;
extern Twitter tw;
extern bool opt_norest;
extern bool opt_show_ng;
extern std::string opt_ngword;
extern std::string opt_ngword_user;
extern std::string record_file;
extern std::string opt_filter;
extern std::string last_id;
extern int  last_id_count;
extern int  last_id_max;
extern bool in_sixel;
extern std::string opt_ciphers;
extern bool opt_full_url;
extern bool opt_progress;
extern NGWord ngword;
extern bool opt_ormode;
extern bool opt_output_palette;
extern int  opt_timeout_image;
extern bool opt_pseudo_home;
extern std::string myid;
extern bool opt_nocolor;
extern int  opt_record_mode;
extern std::string basedir;
extern std::string cachedir;
extern std::string tokenfile;
extern std::string colormapdir;

extern StringDictionary followlist;
extern StringDictionary blocklist;
extern StringDictionary mutelist;
extern StringDictionary nortlist;
