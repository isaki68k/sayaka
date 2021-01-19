#include "StringUtil.h"
#include "eaw_code.h"
#include "main.h"
#include "term.h"
#include <err.h>
#include <getopt.h>
#include <signal.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/ttycom.h>

static const char version[] = "3.5.x (2021/01/09)";

enum SayakaCmd {
	Noop = 0,
	Stream,
	Play,
	Tweet,
	Followlist,
	Mutelist,
	NgwordAdd,
	NgwordDel,
	NgwordList,
	Nortlist,
	Blocklist,
	Version,
};

static const int DEFAULT_FONT_WIDTH = 7;
static const int DEFAULT_FONT_HEIGHT = 14;

static std::string GetHomeDir();
static void init();
static void init_stream();
static void signal_handler(int signo);
static void sigwinch();
static void cmd_users_list(const StringDictionary& list);
static void cmd_followlist();
static void cmd_blocklist();
static void cmd_mutelist();
static void cmd_nortlist();
static void cmd_ngword_add();
static void cmd_ngword_del();
static void cmd_ngword_list();
static void cmd_version();
[[noreturn]] static void usage();

// enum は getopt() の1文字のオプションと衝突しなければいいので
// 適当に 0x80 から始めておく。
enum {
	OPT_black = 0x80,
	OPT_blocklist,
	OPT_ciphers,
	OPT_color,
	OPT_debug,
	OPT_debug_http,
	OPT_debug_image,
	OPT_debug_show,
	OPT_debug_sixel,
	OPT_eaw_a,
	OPT_eaw_n,
	OPT_euc_jp,
	OPT_filter,
	OPT_followlist,
	OPT_font,
	OPT_full_url,
	OPT_home,
	OPT_jis,
	OPT_max_cont,
	OPT_max_image_cols,
	OPT_mutelist,
	OPT_ngword_add,
	OPT_ngword_del,
	OPT_ngword_list,
	OPT_ngword_user,
	OPT_no_color,
	OPT_no_image,
	OPT_no_rest,
	OPT_nortlist,
	OPT_ormode,
	OPT_palette,
	OPT_play,
	OPT_post,
	OPT_progress,
	OPT_record,
	OPT_record_all,
	OPT_show_ng,
	OPT_timeout_image,
	OPT_token,
	OPT_version,
	OPT_white,
	OPT_x68k,
};

static const struct option longopts[] = {
	{ "black",			no_argument,		NULL,	OPT_black },
	{ "blocklist",		no_argument,		NULL,	OPT_blocklist },
	{ "ciphers",		required_argument,	NULL,	OPT_ciphers },
	{ "color",			required_argument,	NULL,	OPT_color },
	{ "debug",			required_argument,	NULL,	OPT_debug },
	{ "debug-http",		required_argument,	NULL,	OPT_debug_http },
	{ "debug-image",	required_argument,	NULL,	OPT_debug_image },
	{ "debug-show",		required_argument,	NULL,	OPT_debug_show },
	{ "debug-sixel",	required_argument,	NULL,	OPT_debug_sixel },
	{ "eaw-a",			required_argument,	NULL,	OPT_eaw_a },
	{ "eaw-n",			required_argument,	NULL,	OPT_eaw_n },
	{ "euc-jp",			no_argument,		NULL,	OPT_euc_jp },
	{ "filter",			required_argument,	NULL,	OPT_filter },
	{ "followlist",		no_argument,		NULL,	OPT_followlist },
	{ "font",			required_argument,	NULL,	OPT_font },
	{ "full-url",		no_argument,		NULL,	OPT_full_url },
	{ "home",			no_argument,		NULL,	OPT_home },
	{ "jis",			no_argument,		NULL,	OPT_jis },
	{ "max-cont",		required_argument,	NULL,	OPT_max_cont },
	{ "max-image-cols",	required_argument,	NULL,	OPT_max_image_cols },
	{ "mutelist",		no_argument,		NULL,	OPT_mutelist },
	{ "ngword-add",		required_argument,	NULL,	OPT_ngword_add },
	{ "ngword-del",		required_argument,	NULL,	OPT_ngword_del },
	{ "ngword-list",	no_argument,		NULL,	OPT_ngword_list },
	{ "ngword-user",	required_argument,	NULL,	OPT_ngword_user },
	{ "no-color",		no_argument,		NULL,	OPT_no_color },
	{ "no-image",		no_argument,		NULL,	OPT_no_image },
	{ "no-rest",		no_argument,		NULL,	OPT_no_rest },
	{ "nortlist",		no_argument,		NULL,	OPT_nortlist },
	{ "ormode",			required_argument,	NULL,	OPT_ormode },
	{ "palette",		required_argument,	NULL,	OPT_palette },
	{ "play",			no_argument,		NULL,	OPT_play },
	{ "post",			no_argument,		NULL,	OPT_post },
	{ "progress",		no_argument,		NULL,	OPT_progress },
	{ "record",			required_argument,	NULL,	OPT_record },
	{ "record-all",		required_argument,	NULL,	OPT_record_all },
	{ "show-ng",		no_argument,		NULL,	OPT_show_ng },
	{ "timeout-image",	required_argument,	NULL,	OPT_timeout_image },
	{ "token",			required_argument,	NULL,	OPT_token },
	{ "version",		no_argument,		NULL,	OPT_version },
	{ "white",			no_argument,		NULL,	OPT_white },
	{ "x68k",			no_argument,		NULL,	OPT_x68k },
	{ "help",			no_argument,		NULL,	'h' },
	{ NULL },
};

int
main(int ac, char *av[])
{
	int c;

	diagHttp.SetClassname("HttpClient");

	SayakaCmd cmd = SayakaCmd::Noop;
	basedir     = GetHomeDir() + "/.sayaka/";
	cachedir    = basedir + "cache";
	tokenfile   = basedir + "token.json";
	colormapdir = basedir;
	ngword.SetFileName(basedir + "ngword.json");

	address_family = AF_UNSPEC;
	color_mode = 256;
	bg_white = true;
	opt_show_ng = false;
	opt_filter = "";
	last_id = "";
	last_id_count = 0;
	last_id_max = 10;
	opt_full_url = false;
	opt_progress = false;
	opt_ormode = false;
	opt_output_palette = true;
	opt_timeout_image = 3000;
	opt_eaw_a = 2;
	opt_eaw_n = 1;

	while ((c = getopt_long(ac, av, "46h", longopts, NULL)) != -1) {
		switch (c) {
		 case '4':
			address_family = AF_INET;
			break;
		 case '6':
			address_family = AF_INET6;
			break;
		 case OPT_black:
			bg_white = false;
			break;
		 case OPT_blocklist:
			cmd = SayakaCmd::Blocklist;
			break;
		 case OPT_ciphers:
			opt_ciphers = optarg;
			break;
		 case OPT_color:
			if (strcmp(optarg, "x68k") == 0) {
				color_mode = ColorFixedX68k;
			} else {
				color_mode = atoi(optarg);
			}
			break;
		 case OPT_debug:
			diag.SetLevel(atoi(optarg));
			// とりあえず後方互換
			opt_debug = (diag > 0);
			break;
		 case OPT_debug_http:
			diagHttp.SetLevel(atoi(optarg));
			break;
		 case OPT_debug_image:
			diagImage.SetLevel(atoi(optarg));
			break;
		 case OPT_debug_show:
			diagShow.SetLevel(atoi(optarg));
			break;
		 case OPT_debug_sixel:
			opt_debug_sixel = atoi(optarg);
			max_image_count = 1;
			break;
		 case OPT_eaw_a:
			opt_eaw_a = atoi(optarg);
			if (opt_eaw_a < 1 || opt_eaw_a > 2) {
				usage();
			}
			break;
		 case OPT_eaw_n:
			opt_eaw_n = atoi(optarg);
			if (opt_eaw_n < 1 || opt_eaw_n > 2) {
				usage();
			}
			break;
		 case OPT_euc_jp:
			iconv_tocode = "euc-jp";
			break;
		 case OPT_filter:
			cmd = SayakaCmd::Stream;
			opt_filter = optarg;
			break;
		 case OPT_followlist:
			cmd = SayakaCmd::Followlist;
			break;
		 case OPT_font:
		 {
			// "7x14" のような形式
			std::string str(optarg);
			auto metric = Split(str, "x");
			if (metric.size() != 2) {
				usage();
			}
			opt_fontwidth = std::stoi(metric[0]);
			opt_fontheight = std::stoi(metric[1]);
			break;
		 }
		 case OPT_full_url:
			opt_full_url = true;
			break;
		 case OPT_home:
			cmd = SayakaCmd::Stream;
			opt_pseudo_home = true;
			break;
		 case OPT_jis:
			iconv_tocode = "iso-2022-jp";
			break;
		 case OPT_max_cont:
			last_id_max = atoi(optarg);
			break;
		 case OPT_max_image_cols:
			max_image_count = atoi(optarg);
			if (max_image_count < 1) {
				max_image_count = 0;
			}
			break;
		 case OPT_mutelist:
			cmd = SayakaCmd::Mutelist;
			break;
		 case OPT_ngword_add:
			cmd = SayakaCmd::NgwordAdd;
			opt_ngword = optarg;
			break;
		 case OPT_ngword_del:
			cmd = SayakaCmd::NgwordDel;
			opt_ngword = optarg;
			break;
		 case OPT_ngword_list:
			cmd = SayakaCmd::NgwordList;
			break;
		 case OPT_ngword_user:
			opt_ngword_user = optarg;
			break;
		 case OPT_no_color:
			opt_nocolor = true;
			break;
		 case OPT_no_image:
			opt_noimage = true;
			break;
		 case OPT_no_rest:
			opt_norest = true;
			break;
		 case OPT_nortlist:
			cmd = SayakaCmd::Nortlist;
			break;
		 case OPT_ormode:
			if (strcmp(optarg, "on") == 0) {
				opt_ormode = true;
			} else if (strcmp(optarg, "off") == 0) {
				opt_ormode = false;
			} else {
				usage();
			}
			break;
		 case OPT_palette:
			if (strcmp(optarg, "on") == 0) {
				opt_output_palette = true;
			} else if (strcmp(optarg, "off") == 0) {
				opt_output_palette = false;
			} else {
				usage();
			}
			break;
		 case OPT_play:
			cmd = SayakaCmd::Play;
			break;
		 case OPT_post:
			cmd = SayakaCmd::Tweet;
			break;
		 case OPT_progress:
			opt_progress = true;
			break;
		 case OPT_record:
			if (opt_record_mode != 0) {
				usage();
			}
			opt_record_mode = 1;
			record_file = optarg;
			break;
		 case OPT_record_all:
			if (opt_record_mode != 0) {
				usage();
			}
			opt_record_mode = 2;
			record_file = optarg;
			break;
		 case OPT_show_ng:
			opt_show_ng = true;
			break;
		 case OPT_timeout_image:
			opt_timeout_image = atoi(optarg) * 1000;
			break;
		 case OPT_token:
		 {
			std::string path(optarg);
			if (path.find('/') != std::string::npos) {
				tokenfile = path;
			} else {
				tokenfile = basedir + path;
			}
			break;
		 }
		 case OPT_version:
			cmd = SayakaCmd::Version;
			break;
		 case OPT_white:
			bg_white = true;
			break;
		 case OPT_x68k:
			// 以下を指定したのと同じ
			color_mode = ColorFixedX68k;
			fontwidth = 8;
			fontheight = 16;
			iconv_tocode = "iso-2022-jp";
			bg_white = false;
			opt_progress = true;
			opt_ormode = true;
			opt_output_palette = false;
			break;
		 case 'h':
			usage();
			break;
		 default:
			// 知らない引数はエラー
			usage();
		}
	}
	if (ac > optind) {
		cmd = SayakaCmd::Stream;
		opt_filter = av[optind];
	}

	// --progress ならそれを展開したコマンドラインを表示してみるか
	if (opt_progress) {
		printf("%s", av[0]);
		for (int i = 1; i < ac; i++) {
			if (strcmp(av[i], "--x68k") == 0) {
				printf(" --color x68k --font 8x16 --jis --black"
				       " --progress --ormode on --palette on");
			} else {
				printf(" %s", av[i]);
			}
		}
		printf("\n");
	}

	// usage() は init() より前のほうがいいか。
	if (cmd == SayakaCmd::Noop) {
		usage();
	}

	if (opt_pseudo_home && !opt_filter.empty()) {
		warnx("filter keyword and --home must be exclusive.");
		usage();
	}

	diag.Debug("tokenfile = %s", tokenfile.c_str());
	init();

	// コマンド別処理
	switch (cmd) {
	 case SayakaCmd::Stream:
		init_stream();
		cmd_stream();
		break;
	 case SayakaCmd::Play:
		init_stream();
		cmd_play();
		break;
	 case SayakaCmd::Followlist:
		cmd_followlist();
		break;
	 case SayakaCmd::Mutelist:
		cmd_mutelist();
		break;
	 case SayakaCmd::NgwordAdd:
		cmd_ngword_add();
		break;
	 case SayakaCmd::NgwordDel:
		cmd_ngword_del();
		break;
	 case SayakaCmd::NgwordList:
		cmd_ngword_list();
		break;
	 case SayakaCmd::Nortlist:
		cmd_nortlist();
		break;
	 case SayakaCmd::Blocklist:
		cmd_blocklist();
		break;
	 case SayakaCmd::Tweet:
		cmd_tweet();
		break;
	 case SayakaCmd::Version:
		cmd_version();
		break;
	 default:
		usage();
	}

	return 0;
}

// 初期化
static void
init()
{
	struct stat st;
	int r;

	// ~/.sayaka がなければ作る
	const char *c_basedir = basedir.c_str();
	r = stat(c_basedir, &st);
	if (r < 0 && errno == ENOENT) {
		r = mkdir(c_basedir, 0755);
		if (r < 0) {
			err(1, "init: mkdir %s failed", c_basedir);
		}
		warnx("init: %s is created.", c_basedir);
	}

	// キャッシュディレクトリを作る
	const char *c_cachedir = cachedir.c_str();
	r = stat(c_cachedir, &st);
	if (r < 0 && errno == ENOENT) {
		r = mkdir(c_cachedir, 0755);
		if (r < 0) {
			err(1, "init: mkdir %s failed", c_cachedir);
		}
		warnx("init: %s is created.", c_cachedir);
	}

	// シグナルハンドラを設定
	signal(SIGINT,    signal_handler);
	signal(SIGHUP,    signal_handler);
	signal(SIGPIPE,   signal_handler);
	signal(SIGALRM,   signal_handler);
	signal(SIGXCPU,   signal_handler);
	signal(SIGXFSZ,   signal_handler);
	signal(SIGVTALRM, signal_handler);
	signal(SIGPROF,   signal_handler);
	signal(SIGUSR1,   signal_handler);
	signal(SIGUSR2,   signal_handler);
	// SIGWINCH は *BSD では SA_RESTART が立っていて
	// Linux では立っていないらしい。とりあえず立てておく。
	struct sigaction act;
	memset(&act, 0, sizeof(act));
	act.sa_handler = signal_handler;
	act.sa_flags = SA_RESTART;
	sigaction(SIGWINCH, &act, NULL);
}

// ホームディレクトリを std::string で返す
static std::string
GetHomeDir()
{
	char *home;

	home = getenv("HOME");
	if (home) {
		return home;
	} else {
		return "";
	}
}

// ストリームモードのための準備
void
init_stream()
{
	// 端末が SIXEL をサポートしてなければ画像オフ
	if (terminal_support_sixel() == false) {
		if (opt_noimage == false) {
			printf("Terminal doesn't support sixel, switch to --no-image\n");
		}
		opt_noimage = true;
	}

	// 色の初期化
	init_color();

	// 一度手動で呼び出して桁数を取得
	sigwinch();

	// NG ワード取得
	ngword.ParseFile();
}

static void
signal_handler(int signo)
{
	switch (signo) {
	 case SIGINT:
		// SIXEL 出力中なら中断する (CAN + ST)
		if (in_sixel) {
			printf(CAN ESC "\\");
			fflush(stdout);
		} else {
			exit(0);
		}
		break;

	 case SIGWINCH:
		sigwinch();
		break;

	 default:
		warnx("caught signal %d", signo);
		break;
	}
}

// SIGWINCH の処理
static void
sigwinch()
{
	int ws_cols = 0;
	int ws_width = 0;
	int ws_height = 0;

	struct winsize ws;
	int r = ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws);
	if (r != 0) {
		warn("TIOCGWINSZ failed");
	} else {
		ws_cols = ws.ws_col;

		if (ws.ws_col != 0) {
			ws_width = ws.ws_xpixel / ws.ws_col;
		}
		if (ws.ws_row != 0) {
			ws_height = ws.ws_ypixel / ws.ws_row;
		}
	}

	const char *msg_cols;
	const char *msg_width;
	const char *msg_height;

	// 画面幅は常に更新
	if (ws_cols > 0) {
		screen_cols = ws_cols;
		msg_cols = " (from ioctl)";
	} else {
		screen_cols = 0;
		msg_cols = " (not detected)";
	}

	// フォント幅と高さは指定されてない時だけ取得した値を使う
	auto use_default_font = false;
	if (opt_fontwidth > 0) {
		fontwidth = opt_fontwidth;
	} else {
		if (ws_width > 0) {
			fontwidth = ws_width;
			msg_width = " (from ioctl)";
		} else {
			fontwidth = DEFAULT_FONT_WIDTH;
			msg_width = " (DEFAULT)";
			use_default_font = true;
		}
	}
	if (opt_fontheight > 0) {
		fontheight = opt_fontheight;
	} else {
		if (ws_height > 0) {
			fontheight = ws_height;
			msg_height = " (from ioctl)";
		} else {
			fontheight = DEFAULT_FONT_HEIGHT;
			msg_height = " (DEFAULT)";
			use_default_font = true;
		}
	}
	if (use_default_font) {
		printf("sayaka: Fontsize not detected. "
			"Application default %dx%d is used.", fontwidth, fontheight);
	}

	// フォントの高さからアイコンサイズを決定する。
	//
	// SIXEL 表示後のカーソル位置は、
	// o xterm 等では SIXEL 最終ラスタを含む行の次の行、
	// o VT382 等では SIXEL 最終ラスタの次のラスタを含む行
	// になる。
	// アイコンは2行以上3行未満にする必要があり、
	// かつ6の倍数だと SIXEL 的に都合がいい。
	iconsize  = ((fontheight * 3 - 1) / 6) * 6;
	// 画像サイズにはアイコンのような行制約はないので計算は適当。
	// XXX まだ縦横について考慮してない
	imagesize = ((fontheight * 9 - 1) / 6) * 6;

	// そこからインデント幅を決定
	indent_cols = ((int)(iconsize / fontwidth)) + 1;

	diag.Debug("screen columns=%d%s", screen_cols, msg_cols);
	diag.Debug("font height=%d%s", fontheight, msg_height);
	diag.Debug("font width=%d%s", fontwidth, msg_width);
	diag.Debug("iconsize=%d", iconsize);
	diag.Debug("indent columns=%d", indent_cols);
	diag.Debug("imagesize=%d", imagesize);
}
// ユーザ一覧を表示するコマンド(共通部分)
static void
cmd_users_list(const StringDictionary& list)
{
	for (const auto& kv : list) {
		const auto& key = kv.first;
		printf("%s\n", key.c_str());
	}
}

// フォローユーザの一覧を取得して表示するコマンド
static void
cmd_followlist()
{
	CreateTwitter();
	get_follow_list();
	cmd_users_list(followlist);
}

// ブロックユーザの一覧を取得して表示するコマンド
static void
cmd_blocklist()
{
	CreateTwitter();
	get_block_list();
	cmd_users_list(blocklist);
}

// ミュートユーザの一覧を取得して表示するコマンド
static void
cmd_mutelist()
{
	CreateTwitter();
	get_mute_list();
	cmd_users_list(mutelist);
}

// RT 非表示ユーザの一覧を取得して表示するコマンド
static void
cmd_nortlist()
{
	CreateTwitter();
	get_nort_list();
	cmd_users_list(nortlist);
}

// NG ワードを追加するコマンド
static void
cmd_ngword_add()
{
	ngword.CmdAdd(opt_ngword, opt_ngword_user);
}

// NG ワードを削除するコマンド
static void
cmd_ngword_del()
{
	ngword.CmdDel(opt_ngword);
}

// NG ワード一覧を表示するコマンド
static void
cmd_ngword_list()
{
	ngword.CmdList();
}

static void
cmd_version()
{
	printf("sayaka version %s\n", version);
}

static void
usage()
{
	printf(
R"(usage: sayaka [<options>...] --home
       sayaka [<options>...] <keyword>
	--color <n> : color mode { 2 .. 256 or x68k }. default 256.
	--font <w>x<h> : font width x height. default 7x14.
	--filter <keyword>
	--full-url : display full URL even if the URL is abbreviated.
	--home : pseudo home timeline using filter stream
	--white / --black : darken/lighten the text color. (default: --white)
	--no-color : disable all text color sequences
	--no-image / --noimg
	--jis
	--eucjp
	--play : read JSON from stdin.
	--post : post tweet from stdin (utf-8 is expected).
	--progress: show start up progress.
	--record <file> : record JSON to file.
	--record-all <file> : record all received JSON to file.
	--show-ng
	--token <file> : token file (default: ~/.sayaka/token.json)
	--version
	--x68k : preset options for x68k.

	-4
	-6
	--blocklist
	--ciphers <ciphers>
	--debug
	--debug-http <0-2>
	--debug-show <0-2>
	--debug-image <0-1>
	--debug-sixel <0-2>
	--followlist
	--max-cont <n>
	--max-image-cols <n>
	--mutelist
	--ngword-add
	--ngword-del
	--ngword-list
	--ngword-user
	--no-rest
	--nortlist
	--ormode <on|off> (default off)
	--palette <on|off> (default on)
)"
	);
	exit(0);
}
