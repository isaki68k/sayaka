#
# GNU Make 用の Makefile です
#

SRCS.sayaka=	\
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

SRCS_NATIVE= \
	System.OS.native.c \

VALA_PKGS=	\
	--pkg posix \
	--pkg gdk-pixbuf-2.0 \

PKGS= \
	glib-2.0 \
	gdk-pixbuf-2.0 \
	gio-2.0

CFLAGS=	-w `pkg-config --cflags ${PKGS}`
LIBS=	`pkg-config --libs ${PKGS}`

VALA_FLAGS=	\

VALAC=	valac ${VALA_FLAGS} ${VALA_PKGS}

all:	sayaka

sayaka:	${SRCS.sayaka:.vala=.o} ${SRCS_NATIVE:.c=.o}
	${CC} -o $@ $^ ${LIBS}

${SRCS.sayaka:.vala=.o} ${SRCS_NATIVE:.c=.o}: %.o : %.c
	${CC} ${CFLAGS} -c $^ -o $@

${SRCS.sayaka:.vala=.c}: %.c : ${SRCS.sayaka}
	${VALAC} -C ${SRCS.sayaka}
