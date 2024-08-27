# vi:set ts=8:

.if exists(./Makefile.cfg)
.include "./Makefile.cfg"
.endif

CFLAGS+=	-O2

.if !defined(RELEASE)
CFLAGS+=	-Wall -Werror ${WARNFLAGS}
.endif

CFLAGS+=	-I..

SRCS_common+=	diag.c
SRCS_common+=	image.c
SRCS_common+=	image_blurhash.c
.if defined(HAVE_LIBPNG)
SRCS_common+=	image_png.c
.endif
SRCS_common+=	image_sixel.c
SRCS_common+=	image_stb.c
.if defined(HAVE_LIBWEBP)
SRCS_common+=	image_webp.c
.endif
SRCS_common+=	net.c
SRCS_common+=	netstream.c
SRCS_common+=	pstream.c
SRCS_common+=	string.c
SRCS_common+=	util.c

SRCS_sayaka+=	eaw_data.c
SRCS_sayaka+=	json.c
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
		sixelv.c

LIBS+=	-lm

all:	sayaka sixelv

sayaka:	sayaka.o libsayaka.a libcommon.a
	${CC} ${LDFLAGS} -o $@ $> ${LIBS} ${LIBS_sayaka}

sixelv:	sixelv.o libsixelv.a libcommon.a
	${CC} ${LDFLAGS} -o $@ $> ${LIBS} ${LIBS_sixelv}

libcommon.a:	${SRCS_common:.c=.o}
	rm -f $@
	ar r $@ $>

libsayaka.a:	${SRCS_sayaka:.c=.o}
	rm -f $@
	ar r $@ $>

libsixelv.a:	${SRCS_sixelv:.c=.o}
	rm -f $@
	ar r $@ $>

.c.o:
	${CC} ${CFLAGS} ${CFLAGS.${.IMPSRC}} -c ${.IMPSRC} -o $@

eaw_data.c:	eaw_gen
	rm -f $@
	./eaw_gen > $@

eaw_gen:	eaw_gen.c
	${CC} ${CFLAGS} ${LDFLAGS} -o $@ $> ${LIBS} -licuuc

# Test
testall:	all dump test terminal wsclient

dump:	dump.c libsayaka.a libcommon.a
	${CC} ${CFLAGS} ${LDFLAGS} -o $@ $> ${LIBS}

test:	test.c libsayaka.a libcommon.a
	${CC} ${CFLAGS} ${LDFLAGS} -o $@ $> ${LIBS}

terminal:	terminal.c libcommon.a
	${CC} ${CFLAGS} ${LDFLAGS} -DTEST -o $@ $> ${LIBS}

wsclient:	wsclient.c libsayaka.a libcommon.a
	${CC} ${CFLAGS} ${LDFLAGS} -DTEST -o $@ $> ${LIBS}

.PHONY:	clean
clean:
	rm -f sayaka sixelv eaw_gen dump test terminal wsclient *.o *.a *.core

.PHONY:	depend
depend:	${SRCS_all:.c=.d}
	cat ${.ALLSRC} > .depends
	-rm -f *.d

.for S in ${SRCS_all}
${S:.c=.d}:
	${CC} ${CFLAGS} ${CFLAGS.${S}} -E -MM ${S} > $@
.endfor

.if exists(.depends)
.include ".depends"
.endif
