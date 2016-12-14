#
# Copyright (C) 2015 Y.Sugahara (moveccr)
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
# OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
# IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
# INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
# BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
# AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#

ALL_TARGET=	\
	sayaka \
	sixelv \
	h \

SRCS_common=	\
	Diag.vala \
	Dictionary.vala \
	FileUtil.vala \
	HttpClientmTLS.vala \
	IHttpClient.vala \
	ImageReductor.vala \
	imagereductor.native.c \
	mtls.native.c \
	ParsedUri.vala \
	PeekableInputStream.vala \
	sixel.native.c \
	SixelConverter.vala \
	StringUtil.vala \
	System.OS.vala \
	System.OS.native.c \

SRCS_sayaka= ${SRCS_common} \
	sayaka.vala \
	subr.vala \
	NGWord.vala \
	Json.vala \
	OAuth.vala \
	PHP.vala \
	Twitter.vala \

SRCS_sixelv= ${SRCS_common} \
	sixelv.vala \
	OptArgs.vala \
	Stopwatch.vala \

SRCS_h=	\
	h.vala \
	Diag.vala \
	Dictionary.vala \
	StringUtil.vala \
	ParsedUri.vala \

VALA_PKGS=	\
	--pkg posix \
	--pkg gdk-pixbuf-2.0 \

PKGS= \
	glib-2.0 \
	gdk-pixbuf-2.0 \
	gio-2.0 \

# システムヘッダより先にバンドル版の mbedTLS ヘッダを探さないといけない
CFLAGS=	-I../mbedtls/include -I./
LIBS_MBEDTLS=	\
	../mbedtls/library/libmbedtls.a \
	../mbedtls/library/libmbedx509.a \
	../mbedtls/library/libmbedcrypto.a \

COPTS=	-O2 -g -std=c99
CFLAGS+=	-w `pkg-config --cflags ${PKGS}`
LIBS=	`pkg-config --libs ${PKGS}`
LIBS+=	${LIBS_MBEDTLS}

LIBS+= -ljpeg

VALA_FLAGS=	\
	mtlsv.vapi \
	glib.vapi \

VALAC=	valac

OBJDIR=		objdir
VALAMAKE=	./vala-make2 -d -e -O ${OBJDIR}

.SUFFIXES:

.PHONY:	${ALL_TARGET}

all:	sayaka

vala-make2:	vala-make2.vala
	${VALAC} -X -w --pkg posix vala-make2.vala

VALAMAKE_COMMON_OPTS= \
		-a "${VALAC}" \
		--valaopt "${VALA_PKGS} ${VALA_FLAGS}" \
		-c "${CC} ${COPTS} ${CFLAGS}" \
		-l "${CC} " \
		-L "${LIBS} " \
		${VALAMAKE_OPTS} \

sayaka:
	@${VALAMAKE} ${VALAMAKE_COMMON_OPTS} \
		-o sayaka \
		${SRCS_sayaka}

.PHONY:	clean-sayaka
clean-sayaka:
	@${VALAMAKE} --clean ${VALAMAKE_COMMON_OPTS} \
		-o sayaka \
		${SRCS_sayaka}

sixelv:
	@${VALAMAKE} ${VALAMAKE_COMMON_OPTS} \
		-o sixelv \
		${SRCS_sixelv}

.PHONY:	clean-sixelv
clean-sixelv:
	@${VALAMAKE} --clean ${VALAMAKE_COMMON_OPTS} \
		-o sixelv \
		${SRCS_sixelv}

h:
	@${VALAMAKE} ${VALAMAKE_COMMON_OPTS} \
		-o h \
		${SRCS_h}

.PHONY:	clean-h
clean-h:
	@${VALAMAKE} --clean ${VALAMAKE_COMMON_OPTS} \
		-o h  \
		${SRCS_h}

.PHONY:	clean
clean:	clean-sayaka clean-sixelv clean-h

mtlstest: mtls.native.c ${LIBS_MBEDTLS}
	${CC} ${CFLAGS} -DTEST -DDEBUG mtls.native.c -o mtlstest ${LIBS_MBEDTLS}
