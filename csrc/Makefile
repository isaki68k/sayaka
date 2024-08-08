# vi:set ts=8:

CFLAGS+=	-O2

.if !defined(RELEASE)
CFLAGS+=	-Wall -Werror
.endif

INCLUDES+=	-I..
INCLUDES+=	-I/usr/pkg/include

SRCS_common+=	diag.c
SRCS_common+=	fdstream.c
SRCS_common+=	image.c
SRCS_common+=	image_png.c
SRCS_common+=	image_sixel.c
SRCS_common+=	string.c
SRCS_common+=	util.c

SRCS_sixelv+=	image_bmp.c

#LIBS_common+=	-lbsd
LIBS_common+=	`pkg-config --libs libpng`

SRCS_all=		${SRCS_common} ${SRCS_sixelv}

all:	sixelv

sixelv:	sixelv.o libcommon.a libsixelv.a
	${CC} ${LDFLAGS} -o $@ $> ${LIBS_common} ${LIBS_sixelv}

libcommon.a:	${SRCS_common:.c=.o}
	rm -f $@
	ar r $@ $>

libsixelv.a:	${SRCS_sixelv:.c=.o}
	rm -f $@
	ar r $@ $>

.c.o:
	${CC} ${CFLAGS} ${CFLAGS.${.IMPSRC}} ${INCLUDES} -c ${.IMPSRC} -o $@

.PHONY:	clean
clean:
	rm -f *.o *.a

.PHONY:	depend
depend:	${SRCS_all:.c=.d}
	cat ${.ALLSRC} > .depends
	-rm -f *.d

.for S in ${SRCS_all}
${S:.c=.d}:
	${CC} ${CFLAGS} ${CFLAGS.${S}} ${INCLUDES} -E -MM ${S} > $@
.endfor

.if exists(.depends)
.include ".depends"
.endif
