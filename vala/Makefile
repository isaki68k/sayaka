SRCS=	\
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

PKGS=	\
	--pkg posix \
	--pkg gdk-pixbuf-2.0 \

VALAFLAGS=	\
	-g -X -w \

VALAC=	valac ${VALAFLAGS} ${PKGS}

CLEAN_FILES=${SRCS:.vala=.c}

# disable make's builtin rules
.SUFFIXES:

sayaka:	${SRCS} ${SRCS_NATIVE}
	${VALAC} ${SRCS} ${SRCS_NATIVE}

.PHONY: c
c:
	${VALAC} -g --save-temps ${SRCS} ${SRCS_NATIVE}

.PHONY:	clean
clean:
	rm -f sayaka sixelv ${CLEAN_FILES}

SIXELV_SRCS = \
	sixelv.vala \
	Diag.vala \
	Dictionary.vala \
	SixelConverter.vala \
	StringUtil.vala \
	HttpClient.vala \
	OptArgs.vala \
	ParsedUri.vala \

sixelv: ${SIXELV_SRCS}
	${VALAC} ${SIXELV_SRCS}

o_SRCS = \
	o.vala \
    ParsedUri.vala \
	HttpClient.vala \
	OAuth.vala \
	StringUtil.vala \
	Diag.vala \
	Dictionary.vala \

o: ${o_SRCS}
	${VALAC} ${o_SRCS}

h_SRCS=	\
	h.vala \
    ParsedUri.vala \
	HttpClient.vala \
	StringUtil.vala \
	Diag.vala \
	Dictionary.vala \

h: ${h_SRCS}
	${VALAC} ${h_SRCS}
