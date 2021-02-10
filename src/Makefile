# vi:set ts=8:

.include "../Makefile.cfg"

SRCS_common=	\
	ChunkedInputStream.cpp \
	Diag.cpp \
	Dictionary.cpp \
	FdStream.cpp \
	FileStream.cpp \
	FileUtil.cpp \
	HttpClient.cpp \
	Image.cpp \
	ImageLoaderJPEG.cpp \
	ImageLoaderPNG.cpp \
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
	eaw_code.cpp \
	eaw_data.cpp \
	fetch_image.cpp \
	sayaka.cpp \
	term.cpp \
	subr.cpp \

SRCS_sixelv=	\
	${SRCS_common} \
	sixelv.cpp \


CXX+=		-g
#CXX+=		-fsanitize=undefined

CPPFLAGS+=	-O0
CPPFLAGS+=	-Wall -Werror -Wshadow -Wno-sign-compare

all:	sayaka test sixelv

sayaka:	main.o ${SRCS_sayaka:.cpp=.o}
	${CXX} ${LDFLAGS} -o $@ $> ${LIBS}

test:	test.o ${SRCS_sayaka:.cpp=.test.o}
	${CXX} ${LDFLAGS} -o $@ $> ${LIBS}

sixelv:	sixelv.o ${SRCS_sixelv:.cpp=.o}
	${CXX} ${LDFLAGS} -o $@ $> ${LIBS}

.cpp.o:
	${CXX} ${CPPFLAGS} ${INCLUDES} -c ${.IMPSRC} -o $@

main.o:	main.cpp
sixelv.o: sixelv.cpp
.for S in ${SRCS_sayaka}
${S:.cpp=.o}:	${S}
.endfor

test.o:	test.cpp
	${CXX} ${CPPFLAGS} ${INCLUDES} -DSELFTEST -c $> -o $@

.for S in ${SRCS_sayaka}
${S:.cpp=.test.o}:	${S}
	${CXX} ${CPPFLAGS} ${INCLUDES} -DSELFTEST -c ${S} -o $@
.endfor

# Developpers only
eaw_gen:	eaw_gen.cpp
	${CXX} ${CPPFLAGS} -I/usr/pkg/include $> -o $@ -L/usr/pkg/lib -Wl,-R,/usr/pkg/lib -licuuc

# XXX
test_mtls:	mtls.cpp
	${CXX} ${CPPFLAGS} ${INCLUDES} -DTEST $> -o $@ ${LIBS}
test_term:	term.cpp
	${CXX} ${CPPFLAGS} -DTEST $> -o $@


.PHONY:	clean
clean:
	rm -f sayaka sixelv test test_mtls test_term eaw_gen *.o *.core


.PHONY:	depend
depend:	${SRCS_sayaka:.cpp=.d} ${SRCS_sayaka:.cpp=.test.d} sixelv.d
	cat ${.ALLSRC} > .depends
	-rm -f *.d

.for S in ${SRCS_sayaka}
${S:.cpp=.d}:
	${CXX} ${CPPFLAGS} ${INCLUDES} -E -MM ${S} > $@

${S:.cpp=.test.d}:
	${CXX} ${CPPFLAGS} ${INCLUDES} -E -MM -DSELFTEST -MT ${S:.cpp=.test.o} ${S} > $@
.endfor

sixelv.d:
	${CXX} ${CPPFLAGS} ${INCLUDES} -E -MM sixelv.cpp > $@

.if exists(.depends)
.include ".depends"
.endif
