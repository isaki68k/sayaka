SRCS=	\
	sayaka.vala \
	Json.vala \
	SixelConverter.vala \
	System.OS.vala \
	System.OS.native.c \
	System.Collections.Generic.Dictionary.vala \

PKGS=	\
	--pkg posix \
	--pkg gdk-pixbuf-2.0 \

VALAFLAGS=	\
	-X -Wno-incompatible-pointer-types \

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
	rm -f sayaka ${CLEAN_FILES}
