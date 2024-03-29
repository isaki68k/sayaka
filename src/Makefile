# vi:set ts=8:

.if exists(../Makefile.cfg)
.include "../Makefile.cfg"
.endif

SRCS_common+=	Base64.cpp
SRCS_common+=	Blurhash.cpp
SRCS_common+=	ChunkedInputStream.cpp
SRCS_common+=	Diag.cpp
SRCS_common+=	FdStream.cpp
SRCS_common+=	FileStream.cpp
SRCS_common+=	HttpClient.cpp
SRCS_common+=	Image.cpp
SRCS_common+=	ImageLoaderBlurhash.cpp
SRCS_common+=	ImageLoaderWebp.cpp
SRCS_common+=	ImageReductor.cpp
SRCS_common+=	MathAlphaSymbols.cpp
SRCS_common+=	MemoryStream.cpp
SRCS_common+=	Misskey.cpp
#SRCS_common+=	NGWord.cpp
SRCS_common+=	ParsedUri.cpp
SRCS_common+=	PeekableStream.cpp
SRCS_common+=	Random.cpp
SRCS_common+=	SixelConverter.cpp
SRCS_common+=	SixelConverterOR.cpp
SRCS_common+=	Stream.cpp
SRCS_common+=	StringUtil.cpp
SRCS_common+=	TLSHandle.cpp
SRCS_common+=	UString.cpp
SRCS_common+=	WSClient.cpp
SRCS_common+=	eaw_code.cpp
SRCS_common+=	eaw_data.cpp
SRCS_common+=	term.cpp
SRCS_common+=	subr.cpp

.if "${MAKE_STB_IMAGE}" == "yes"
SRCS_common+=	ImageLoaderSTB.cpp
.else
SRCS_common+=	ImageLoaderGIF.cpp
SRCS_common+=	ImageLoaderJPEG.cpp
SRCS_common+=	ImageLoaderPNG.cpp
.endif

.if "${MAKE_MBEDTLS}" == "yes"
SRCS_common += TLSHandle_mbedtls.cpp
.else
SRCS_common += TLSHandle_openssl.cpp
.endif

SRCS_sayaka=	sayaka.cpp
SRCS_sayaka+=	Display.cpp
SRCS_sayaka+=	Print.cpp

SRCS_sixelv=	sixelv.cpp

SRCS_test+=	test.cpp
SRCS_test+=	testBase64.cpp
SRCS_test+=	testChunkedInputStream.cpp
SRCS_test+=	testDiag.cpp
SRCS_test+=	testDictionary.cpp
SRCS_test+=	testImageReductor.cpp
SRCS_test+=	testMemoryStream.cpp
#SRCS_test+=	testNGWord.cpp
SRCS_test+=	testParseUri.cpp
SRCS_test+=	testSixelConverter.cpp
SRCS_test+=	testStringUtil.cpp
SRCS_test+=	testUString.cpp
SRCS_test+=	testeaw_code.cpp
SRCS_test+=	testsubr.cpp
SRCS_test+=	testterm.cpp

.if "${MAKE_TWITTER}" == "yes"
SRCS_common+=	FileUtil.cpp
SRCS_common+=	OAuth.cpp
SRCS_common+=	Regex.cpp
SRCS_common+=	Twitter.cpp
SRCS_common+=	RichString.cpp
SRCS_common+=	acl.cpp
SRCS_test+=	testFileUtil.cpp
SRCS_test+=	testOAuth.cpp
SRCS_test+=	testRichString.cpp
SRCS_test+=	testacl.cpp
.endif

SRCS_all=	\
	${SRCS_common} \
	${SRCS_sayaka} \
	${SRCS_sixelv} \
	${SRCS_test} \

CPPFLAGS+=	-O2
.if !defined(RELEASE)
CXX+=		-g
#CXX+=		-fsanitize=undefined -fno-sanitize=vptr
#CXX+=		-fsanitize=address -fno-omit-frame-pointer	# address (gcc)
CPPFLAGS+=	-Wall -Werror -Wshadow -Wno-sign-compare
CPPFLAGS+=	-Wignored-qualifiers
CPPFLAGS+=	-Wpointer-arith
CPPFLAGS+=	-Wnon-virtual-dtor
.endif

INCLUDES+=	-I..

# libpng が使ってるので無視
CPPFLAGS.ImageLoaderPNG.cpp+=	-Wno-disabled-macro-expansion

.if defined(RELEASE)
all:	sayaka sixelv
.else
all:	sayaka test sixelv
.endif

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
test_mtls:	TLSHandle_mbedtls.cpp TLSHandle.cpp
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
