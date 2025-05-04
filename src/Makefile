# vi:set ts=8:

.if exists(./Makefile.cfg)
.include "./Makefile.cfg"
.endif

DBG=

.if !defined(RELEASE)
CFLAGS+=	-Og
CFLAGS+=	-Wall -Werror ${WARNFLAGS}
.endif

CFLAGS+=	-I..

CFLAGS.image.o.sayaka.c	=
CFLAGS.image.o.sixelv.c	= -DSIXELV
.if !defined(HAVE___THREAD)
CFLAGS.image_stb.c	= -DSTBI_NO_THREAD_LOCALS
.endif

#PROFOPT=	-g -fsanitize=leak # gcc
CFLAGS+=	${PROFOPT}
LDFLAGS+=	${PROFOPT}

SRCS_common+=	diag.c
SRCS_common+=	httpclient.c
SRCS_common+=	image_blurhash.c
.if defined(HAVE_GIFLIB)
SRCS_common+=	image_gif.c
.endif
.if defined(HAVE_LIBJPEG)
SRCS_common+=	image_jpeg.c
.endif
.if defined(HAVE_LIBPNG)
SRCS_common+=	image_png.c
.endif
SRCS_common+=	image_sixel.c
SRCS_common+=	image_stb.c
.if defined(HAVE_LIBWEBP)
SRCS_common+=	image_webp.c
.endif
SRCS_common+=	net.c
SRCS_common+=	pstream.c
SRCS_common+=	string.c
SRCS_common+=	util.c

SRCS_sayaka+=	eaw_data.c
SRCS_sayaka+=	json.c
SRCS_sayaka+=	mathalpha.c
SRCS_sayaka+=	misskey.c
SRCS_sayaka+=	print.c
SRCS_sayaka+=	subr.c
SRCS_sayaka+=	terminal.c
SRCS_sayaka+=	ustring.c
SRCS_sayaka+=	wsclient.c

SRCS_sixelv+=	image_bmp.c

SRCS_all=	${SRCS_common}	\
		${SRCS_sayaka}	\
		${SRCS_sixelv}	\
		sayaka.c	\
		sixelv.c	\
		dump.c	\
		test.c	\

LIBS+=	-lm

PROGS=	sayaka sixelv dump httpclient test terminal wsclient

all:	${PROGS}

sayaka:	libcommon.a libsayaka.a sayaka.o
	${CC} ${LDFLAGS} -o $@ sayaka.o libsayaka.a libcommon.a ${LIBS}

sixelv:	libcommon.a libsixelv.a sixelv.o
	${CC} ${LDFLAGS} -o $@ sixelv.o libsixelv.a libcommon.a ${LIBS}

libcommon.a:	${SRCS_common:.c=.o}
	rm -f $@
	ar r $@ $>

libsayaka.a:	${SRCS_sayaka:.c=.o} image.o.sayaka.o
	rm -f $@
	ar r $@ $>

libsixelv.a:	${SRCS_sixelv:.c=.o} image.o.sixelv.o
	rm -f $@
	ar r $@ $>

.c.o:
	${CC} ${CFLAGS} ${CFLAGS.${.IMPSRC}} -c ${.IMPSRC} -o $@

image.o.sayaka.o:	image.c
	${CC} ${CFLAGS} ${CFLAGS.${@:.o=.c}} -c image.c -o $@
image.o.sixelv.o:	image.c
	${CC} ${CFLAGS} ${CFLAGS.${@:.o=.c}} -c image.c -o $@

# Not necessary for normal builds.  It's only for developpers.
.if defined(REGEN_EAW)
eaw_data.c:	eaw_gen
	rm -f $@
	./eaw_gen > $@

eaw_gen:	eaw_gen.c
	${CC} ${CFLAGS_ICU} -o $@ $> ${LIBS_ICU}

.endif

dump:	libcommon.a libsayaka.a dump.o
	${CC} ${CFLAGS} ${LDFLAGS} -o $@ dump.o libsayaka.a libcommon.a ${LIBS}

httpclient:	libcommon.a httpclient.c
	${CC} ${CFLAGS} ${LDFLAGS} -DTEST -o $@ httpclient.c libcommon.a ${LIBS}

test:	libcommon.a libsayaka.a test.o
	${CC} ${CFLAGS} ${LDFLAGS} -o $@ test.o libsayaka.a libcommon.a ${LIBS}

terminal:	libcommon.a terminal.c
	${CC} ${CFLAGS} ${LDFLAGS} -DTEST -o $@ terminal.c libcommon.a ${LIBS}

wsclient:	libcommon.a libsayaka.a wsclient.c
	${CC} ${CFLAGS} ${LDFLAGS} -DTEST -o $@ \
		wsclient.c libsayaka.a libcommon.a ${LIBS}

.PHONY:	clean
clean:
	rm -f ${PROGS} eaw_gen *.o *.a *.d *.core

.PHONY: distclean
distclean:	clean
	rm -f Makefile.cfg config.h .depends

.PHONY:	depend
depend:	${SRCS_all:.c=.d} image.o.sayaka.d image.o.sixelv.d
	cat ${.ALLSRC} > .depends
	-rm -f *.d

.for S in ${SRCS_all}
${S:.c=.d}:
	${CC} ${CFLAGS} ${CFLAGS.${S}} -E -MM ${S} > $@
.endfor

image.o.sayaka.d:	image.c
	${CC} ${CFLAGS} ${CFLAGS.${@:.d=.c}} -E -MM -MT ${@:.d=.o} image.c > $@

image.o.sixelv.d:	image.c
	${CC} ${CFLAGS} ${CFLAGS.${@:.d=.c}} -E -MM -MT ${@:.d=.o} image.c > $@

.if exists(.depends)
.include ".depends"
.endif
