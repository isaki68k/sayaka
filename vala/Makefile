SRCS=	\
	sayaka.vala \
	myjson.vala \
	System.OS.vala \
	System.OS.native.c \

GEE=	--pkg gee-0.8
POSIX=	--pkg posix

sayaka:	${SRCS}
	valac --pkg json-glib-1.0 ${GEE} ${POSIX} $>
