SRCS=	\
	sayaka.vala \
	Diag.vala \
	HttpClient.vala \
	Json.vala \
	SixelConverter.vala \
	ParsedUri.vala \
	System.OS.vala \
	System.OS.native.c \
	System.Collections.Generic.Dictionary.vala \

PKGS=	\
	--pkg posix \
	--pkg gdk-pixbuf-2.0 \

VALAFLAGS=	\
	-X -w \

CLEAN_FILES=	\
	sayaka.c \
	Json.c \
	SixelConverter.c \
	System.OS.c \
	System.Collections.Generic.Dictionary.c \

# disable make's builtin rules
.SUFFIXES:

sayaka:	${SRCS}
	valac ${VALAFLAGS} ${PKGS} ${SRCS}

c:	sayaka.c
sayaka.c:	${SRCS}
	valac -C ${VALAFLAGS} ${PKGS} ${SRCS}

.PHONY:	clean
clean:
	rm -f sayaka sixelv ${CLEAN_FILES}

SIXELV_SRCS = \
	sixelv.vala \
	Diag.vala \
	SixelConverter.vala \
	HttpClient.vala \
	OptArgs.vala \
	ParsedUri.vala \

sixelv: ${SIXELV_SRCS}
	valac ${VALAFLAGS} ${PKGS} ${SIXELV_SRCS}
