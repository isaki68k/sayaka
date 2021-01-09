CXX=		c++ -std=c++17 -g -O0
CPPFLAGS=	-I../mbedtls/include
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

LIBS_MBEDTLS=	\
	../mbedtls/library/libmbedtls.a \
	../mbedtls/library/libmbedx509.a \
	../mbedtls/library/libmbedcrypto.a \

LIBS +=	\
	-lssl

all:	test sayaka

test:	test.o \
		ChunkedInputStream.test.o \
		Dictionary.test.o \
		FileUtil.test.o \
		NGWord.test.o \
		MemoryInputStream.test.o \
		OAuth.test.o \
		ParsedUri.test.o \
		StringUtil.test.o \
		Twitter.test.o \
		subr.test.o \
		Diag.o \
		HttpClient.o \
		StreamBase.o \
		mtls.o
	c++ -o $@ $> ${LIBS_MBEDTLS} ${LIBS}

test.o:	test.cpp
	${CXX} ${CPPFLAGS} -DSELFTEST -c $> -o $@

ChunkedInputStream.test.o:	ChunkedInputStream.cpp
	${CXX} ${CPPFLAGS} -DSELFTEST -c $> -o $@

Dictionary.test.o:	Dictionary.cpp
	${CXX} ${CPPFLAGS} -DSELFTEST -c $> -o $@

FileUtil.test.o:	FileUtil.cpp
	${CXX} ${CPPFLAGS} -DSELFTEST -c $> -o $@

MemoryInputStream.test.o:	MemoryInputStream.cpp
	${CXX} ${CPPFLAGS} -DSELFTEST -c $> -o $@

NGWord.test.o: NGWord.cpp
	${CXX} ${CPPFLAGS} -DSELFTEST -c $> -o $@

OAuth.test.o: OAuth.cpp
	${CXX} ${CPPFLAGS} -DSELFTEST -c $> -o $@

ParsedUri.test.o:	ParsedUri.cpp
	${CXX} ${CPPFLAGS} -DSELFTEST -c $> -o $@

StringUtil.test.o:	StringUtil.cpp
	${CXX} ${CPPFLAGS} -DSELFTEST -c $> -o $@

subr.test.o:	subr.cpp
	${CXX} ${CPPFLAGS} -DSELFTEST -c $> -o $@

Twitter.test.o:	Twitter.cpp
	${CXX} ${CPPFLAGS} -DSELFTEST -c $> -o $@

sayaka:	${SRCS:.cpp=.o}
	${CXX} ${LDFLAGS} -o $@ $> ${LIBS_MBEDTLS} ${LIBS}

.cpp.o:
	${CXX} ${CPPFLAGS} -c $> -o $@

test_mtls:	mtls.cpp
	${CXX} ${CPPFLAGS} -DTEST $> -o $@ ${LIBS_MBEDTLS}

test_nonce:	oauth.cpp
	${CXX} ${CPPFLAGS} -DTEST_NONCE $> -o $@ ${LIBS_MBEDTLS}

clean:
	rm -f *.o sayaka test
