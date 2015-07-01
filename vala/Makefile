SRCS=	\
	sayaka.vala \
	Json.vala \
	System.OS.vala \
	System.OS.native.c \
	System.Collections.Generic.Dictionary.vala \

PKGS=	\
	--pkg posix \

VALAFLAGS=	\
	-X -Wno-incompatible-pointer-types \

# disable make's builtin rules
.SUFFIXES:

sayaka:	${SRCS}
	valac ${VALAFLAGS} ${PKGS} ${SRCS}

c:	sayaka.c
sayaka.c:	${SRCS}
	valac -C ${VALAFLAGS} ${PKGS} ${SRCS}

.PHONY:	clean
clean:
	rm -f sayaka sayaka.c
