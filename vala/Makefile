#
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
	System.OS.native.c \

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
	System.OS.native.c \

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

VALAC=	valac

OBJDIR=		objdir
VALAMAKE=	./vala-make2 -d -e -O ${OBJDIR}

.SUFFIXES:

.PHONY:	${ALL_TARGET}

all:	sayaka sixelv

vala-make2:	vala-make2.vala
	${VALAC} -X -w --pkg posix vala-make2.vala

VALAMAKE_COMMON_OPTS= \
		-a "${VALAC}" \
		--valaopt "${VALA_PKGS} ${VALA_FLAGS}" \
		-c "${CC} ${COPTS} ${CFLAGS}" \
		-l "${CC} " \
		-L "${LIBS} " \

sayaka:
	@${VALAMAKE} ${VALAMAKE_COMMON_OPTS} \
		-o sayaka \
		${SRCS_sayaka}

.PHONY:	clean-sayaka
clean-sayaka:
	@${VALAMAKE} --clean ${VALAMAKE_COMMON_OPTS} \
		-o sayaka \
		${SRCS_sayaka}

sixelv:
	@${VALAMAKE} ${VALAMAKE_COMMON_OPTS} \
		-o sixelv \
		${SRCS_sixelv}

.PHONY:	clean-sixelv
clean-sixelv:
	@${VALAMAKE} --clean ${VALAMAKE_COMMON_OPTS} \
		-o sixelv \
		${SRCS_sixelv}

h:
	@${VALAMAKE} ${VALAMAKE_COMMON_OPTS} \
		-o h \
		${SRCS_h}

.PHONY:	clean-h
clean-h:
	@${VALAMAKE} --clean ${VALAMAKE_COMMON_OPTS} \
		-o h  \
		${SRCS_h}

o:
	@${VALAMAKE} ${VALAMAKE_COMMON_OPTS} \
		-o o \
		${SRCS_o}

.PHONY:	clean-o
clean-o:
	@${VALAMAKE} --clean ${VALAMAKE_COMMON_OPTS} \
		-o o \
		${SRCS_o}

.PHONY:	clean
clean:	clean-sayaka clean-sixelv clean-h clean-o

