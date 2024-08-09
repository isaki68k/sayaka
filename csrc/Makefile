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
SRCS_common+=	fdstream.c
SRCS_common+=	image.c
.if defined(HAVE_LIBPNG)
SRCS_common+=	image_png.c
.endif
SRCS_common+=	image_sixel.c
.if defined(HAVE_STB_IMAGE)
SRCS_common+=	image_stb.c
.endif
.if defined(HAVE_LIBCURL)
SRCS_common+=	netstream.c
.endif
SRCS_common+=	string.c
SRCS_common+=	util.c

SRCS_sixelv+=	image_bmp.c

SRCS_all=		${SRCS_common} ${SRCS_sixelv}

all:	sixelv

sixelv:	sixelv.o libsixelv.a libcommon.a
	${CC} ${LDFLAGS} -o $@ $> ${LIBS} ${LIBS_sixelv}

libcommon.a:	${SRCS_common:.c=.o}
	rm -f $@
	ar r $@ $>

libsixelv.a:	${SRCS_sixelv:.c=.o}
	rm -f $@
	ar r $@ $>

.c.o:
	${CC} ${CFLAGS} ${CFLAGS.${.IMPSRC}} -c ${.IMPSRC} -o $@

.PHONY:	clean
clean:
	rm -f *.o *.a

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
