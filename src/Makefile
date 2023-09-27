# vi:set ts=8:

.include "../Makefile.cfg"

SRCS_common=	\
	ChunkedInputStream.cpp \
	Diag.cpp \
	FdStream.cpp \
	FileStream.cpp \
	FileUtil.cpp \
	HttpClient.cpp \
	Image.cpp \
	ImageReductor.cpp \
	MathAlphaSymbols.cpp \
	MemoryStream.cpp \
	Misskey.cpp \
	NGWord.cpp \
	OAuth.cpp \
	ParsedUri.cpp \
	Regex.cpp \
	RichString.cpp \
	SixelConverter.cpp \
	SixelConverterOR.cpp \
	StreamBase.cpp \
	StringUtil.cpp \
	TLSHandle.cpp \
	Twitter.cpp \
	UString.cpp \
	WSClient.cpp \
	acl.cpp \
	eaw_code.cpp \
	eaw_data.cpp \
	fetch_image.cpp \
	term.cpp \
	subr.cpp \

.if "${MAKE_STB_IMAGE}" == "yes"
SRCS_common += \
	ImageLoaderSTB.cpp
.else
SRCS_common += \
	ImageLoaderGIF.cpp \
	ImageLoaderJPEG.cpp \
	ImageLoaderPNG.cpp \

.endif

.if "${MAKE_MBEDTLS}" == "yes"
SRCS_common += TLSHandle_mtls.cpp
.else
SRCS_common += TLSHandle_openssl.cpp
.endif

SRCS_sayaka=	\
	sayaka.cpp \

SRCS_sixelv=	\
	sixelv.cpp \

SRCS_test=	\
	testChunkedInputStream.cpp \
	testDiag.cpp \
	testDictionary.cpp \
	testFileUtil.cpp \
	testImageReductor.cpp \
	testMemoryStream.cpp \
	testNGWord.cpp \
	testOAuth.cpp \
	testParseUri.cpp \
	testRichString.cpp \
	testSixelConverter.cpp \
	testStringUtil.cpp \
	testUString.cpp \
	testacl.cpp \
	testeaw_code.cpp \
	testsubr.cpp \
	testterm.cpp \
	test.cpp

SRCS_all=	\
	${SRCS_common} \
	${SRCS_sayaka} \
	${SRCS_sixelv} \
	${SRCS_test} \

#CXX+=		-g
#CXX+=		-fsanitize=undefined

CPPFLAGS+=	-O2
CPPFLAGS+=	-Wall -Werror -Wshadow -Wno-sign-compare
CPPFLAGS+=	-Wignored-qualifiers
CPPFLAGS+=	-Wpointer-arith
CPPFLAGS+=	-Wnon-virtual-dtor
INCLUDES+=	-I..

# libpng が使ってるので無視
CPPFLAGS.ImageLoaderPNG.cpp+=	-Wno-disabled-macro-expansion

all:	sayaka test sixelv

sayaka:	${SRCS_sayaka:.cpp=.o} libsayaka.a
	${CXX} ${LDFLAGS} -o $@ $> ${LIBS}

test:	test.o ${SRCS_test:.cpp=.o} libsayaka.a
	${CXX} ${LDFLAGS} -o $@ $> ${LIBS}

sixelv:	${SRCS_sixelv:.cpp=.o} libsayaka.a
	${CXX} ${LDFLAGS} -o $@ $> ${LIBS}

libsayaka.a:	${SRCS_common:.cpp=.o}
	rm -f $@
	ar r $@ $>

.cpp.o:
	${CXX} ${CPPFLAGS} ${CPPFLAGS.${.IMPSRC}} ${INCLUDES} -c ${.IMPSRC} -o $@

.for S in ${SRCS_all}
${S:.cpp=.o}:	${S}
.endfor

# Developpers only
eaw_gen:	eaw_gen.cpp
	${CXX} ${CPPFLAGS} -I/usr/pkg/include $> -o $@ -L/usr/pkg/lib -Wl,-R,/usr/pkg/lib -licuuc

# XXX
test_mtls:	mtls.cpp
	${CXX} ${CPPFLAGS} ${INCLUDES} -DTEST $> -o $@ ${LIBS}

test_term:	test_term.o libsayaka.a
	${CXX} ${LDFLAGS} -o $@ $>

test_term.o:	term.cpp
	${CXX} ${CPPFLAGS} ${INCLUDES} -DTEST -c $> -o $@


.PHONY:	clean
clean:
	rm -f sayaka sixelv test test_mtls test_term eaw_gen libsayaka.a *.o *.core


.PHONY:	depend
depend:	${SRCS_all:.cpp=.d}
	cat ${.ALLSRC} > .depends
	-rm -f *.d

.for S in ${SRCS_all}
${S:.cpp=.d}:
	${CXX} ${CPPFLAGS} ${INCLUDES} -E -MM ${S} > $@
.endfor

.if exists(.depends)
.include ".depends"
.endif
