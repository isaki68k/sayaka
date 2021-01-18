# vi:set ts=8:

SRCS_common=	\
	ChunkedInputStream.cpp \
	Diag.cpp \
	Dictionary.cpp \
	FdStream.cpp \
	FileStream.cpp \
	FileUtil.cpp \
	HttpClient.cpp \
	ImageReductor.cpp \
	MemoryStream.cpp \
	ParsedUri.cpp \
	SixelConverter.cpp \
	SixelConverterOR.cpp \
	StreamBase.cpp \
	StringUtil.cpp \
	UString.cpp \
	mtls.cpp \

SRCS_sayaka=	\
	${SRCS_common} \
	NGWord.cpp \
	OAuth.cpp \
	RichString.cpp \
	Twitter.cpp \
	acl.cpp \
	fetch_image.cpp \
	sayaka.cpp \
	term.cpp \
	subr.cpp \

SRCS_sixelv=	\
	${SRCS_common} \
	sixelv.cpp \


PKGS=		glib-2.0 gdk-pixbuf-2.0 gio-unix-2.0

CXX=		c++ -std=c++17
CPPFLAGS+=	-g -O0
CPPFLAGS+=	-Wall -Werror -Wshadow -Wno-sign-compare
CPPFLAGS+=	-I/usr/pkg/include
CPPFLAGS+=	`pkg-config --cflags ${PKGS}`

LIBS +=	\
	-lssl \
	-L/usr/pkg/lib -Wl,-R,/usr/pkg/lib \
	-licuuc \
	-lmbedtls \
	`pkg-config --libs ${PKGS}` \
	-ljpeg \

all:	sayaka test sixelv

sayaka:	main.o ${SRCS_sayaka:.cpp=.o}
	${CXX} ${LDFLAGS} -o $@ $> ${LIBS}

test:	test.o ${SRCS_sayaka:.cpp=.test.o}
	${CXX} ${LDFLAGS} -o $@ $> ${LIBS}

sixelv:	sixelv.o ${SRCS_sixelv:.cpp=.o}
	${CXX} ${LDFLAGS} -o $@ $> ${LIBS}

.cpp.o:
	${CXX} ${CPPFLAGS} -c ${.IMPSRC} -o $@

main.o:	main.cpp
sixelv.o: sixelv.cpp
.for S in ${SRCS_sayaka}
${S:.cpp=.o}:	${S}
.endfor

test.o:	test.cpp
	${CXX} ${CPPFLAGS} -DSELFTEST -c $> -o $@

.for S in ${SRCS_sayaka}
${S:.cpp=.test.o}:	${S}
	${CXX} ${CPPFLAGS} -DSELFTEST -c ${S} -o $@
.endfor

# XXX
test_mtls:	mtls.cpp
	${CXX} ${CPPFLAGS} -DTEST ${.MEMBER} -o $@ ${LIBS}


.PHONY:	clean
clean:
	rm -f sayaka sixelv test test_mtls *.o *.core


.PHONY:	depend
depend:	${SRCS_sayaka:.cpp=.d} ${SRCS_sayaka:.cpp=.test.d} sixelv.d
	cat ${.ALLSRC} > .depends
	-rm -f *.d

.for S in ${SRCS_sayaka}
${S:.cpp=.d}:
	${CXX} ${CPPFLAGS} -E -MM ${S} > $@

${S:.cpp=.test.d}:
	${CXX} ${CPPFLAGS} -E -MM -DSELFTEST -MT ${S:.cpp=.test.o} ${S} > $@
.endfor

sixelv.d:
	${CXX} ${CPPFLAGS} -E -MM sixelv.cpp > $@

.if exists(.depends)
.include ".depends"
.endif
