#
# 共通 Makefile です。
# 変数宣言だけ行ってください。
#

ALL_TARGET=	\
	sayaka \
	sixelv \
	h \
	o \

SRCS_sayaka=	\
	sayaka.vala \
	Diag.vala \
	Dictionary.vala \
	FileUtil.vala \
	HttpClient.vala \
	Json.vala \
	OAuth.vala \
	SixelConverter.vala \
	StringUtil.vala \
	Twitter.vala \
	ParsedUri.vala \
	System.OS.vala \

SRCS_sixelv=	\
	sixelv.vala \
	Diag.vala \
	Dictionary.vala \
	SixelConverter.vala \
	StringUtil.vala \
	HttpClient.vala \
	OptArgs.vala \
	ParsedUri.vala \
	System.OS.vala \

SRCS_h=	\
	h.vala \
	Diag.vala \
	Dictionary.vala \
	HttpClient.vala \
	StringUtil.vala \
	ParsedUri.vala \

SRCS_o=\
	o.vala \
    ParsedUri.vala \
	HttpClient.vala \
	OAuth.vala \
	StringUtil.vala \
	Diag.vala \
	Dictionary.vala \

SRCS_NATIVE= \
	System.OS.native.c \

VALA_PKGS=	\
	--pkg posix \
	--pkg gdk-pixbuf-2.0 \

PKGS= \
	glib-2.0 \
	gdk-pixbuf-2.0 \
	gio-2.0

COPTS=	-O2
CFLAGS=	-w `pkg-config --cflags ${PKGS}`
LIBS=	`pkg-config --libs ${PKGS}`

VALA_FLAGS=	\

VALAC=	valac ${VALA_FLAGS} ${VALA_PKGS}


# gnumake か bsdmake かを判別して include ファイルの拡張子を切り替える。
# gnumake では MAKE_COMMAND という変数が定義される。
# bsdmake ではこの変数はないので、判別できる(?)

ISGNU=bsd${MAKE_COMMAND:%=gnu}
MAKEEXT=${ISGNU:bsdgnu=gnu}

# include 文の引数をクォートしなければ、どちらでも動く

include Makefile.${MAKEEXT}

