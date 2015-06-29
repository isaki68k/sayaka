SRCS=	\
	sayaka.vala \
	myjson.vala \
	System.OS.vala \
	System.OS.native.c \

PKGS=	\
	--pkg json-glib-1.0 \
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
