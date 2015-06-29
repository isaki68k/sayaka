SRCS=	\
	sayaka.vala \
	Json.vala \
	System.OS.vala \
	System.OS.native.c \
	System.Collections.Generic.Dictionary.vala \

PKGS=	\
	--pkg gee-0.8 \
	--pkg posix \

# disable make's builtin rules
.SUFFIXES:

sayaka:	${SRCS}
	valac ${PKGS} ${SRCS}

c:	sayaka.c
sayaka.c:	${SRCS}
	valac -C ${PKGS} ${SRCS}

.PHONY:	clean
clean:
	rm -f sayaka sayaka.c
