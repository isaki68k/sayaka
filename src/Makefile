# vi:set ts=8:

SRCS=	\
	ChunkedInputStream.cpp \
	Diag.cpp \
	Dictionary.cpp \
	FileInputStream.cpp \
	FileUtil.cpp \
	HttpClient.cpp \
	MemoryInputStream.cpp \
	NGWord.cpp \
	OAuth.cpp \
	ParsedUri.cpp \
	StreamBase.cpp \
	StringUtil.cpp \
	Twitter.cpp \
	mtls.cpp \
	sayaka.cpp \
	subr.cpp \
	term.cpp \

CXX=		c++ -std=c++17
CPPFLAGS+=	-g -O0
CPPFLAGS+=	-I../mbedtls/include

LIBS +=	\
	../mbedtls/library/libmbedtls.a \
	../mbedtls/library/libmbedx509.a \
	../mbedtls/library/libmbedcrypto.a \
	-lssl

all:	test sayaka

sayaka:	${SRCS:.cpp=.o}
	${CXX} ${LDFLAGS} -o $@ $> ${LIBS}

test:	test.o ${SRCS:.cpp=.test.o}
	${CXX} ${LDFLAGS} -o $@ $> ${LIBS}

test.o:	test.cpp
	${CXX} ${CPPFLAGS} -DSELFTEST -c $> -o $@

.for S in ${SRCS}
${S:.cpp=.o}:	${S}
	${CXX} ${CPPFLAGS} -c ${S} -o $@

${S:.cpp=.test.o}:	${S}
	${CXX} ${CPPFLAGS} -DSELFTEST -c ${S} -o $@
.endfor

# XXX
test_mtls:	mtls.cpp
	${CXX} ${CPPFLAGS} -DTEST $> -o $@ ${LIBS}


.PHONY:	clean
clean:
	rm -f sayaka test *.o *.core


.PHONY:	depend
depend:	${SRCS:.cpp=.d} ${SRCS:.cpp=.test.d}
	cat ${.ALLSRC} > .depends
	-rm -f *.d

.for S in ${SRCS}
${S:.cpp=.d}:
	${CXX} ${CPPFLAGS} -E -MM ${S} > $@

${S:.cpp=.test.d}:
	${CXX} ${CPPFLAGS} -DSELFTEST -E -MM -MT ${S:.cpp=.test.o} ${S} > $@
.endfor

.if exists(.depends)
.include ".depends"
.endif
