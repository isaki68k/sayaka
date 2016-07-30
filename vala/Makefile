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

SRCS_sayaka=	\
	sayaka.vala \
	Diag.vala \
	Dictionary.vala \
	FileUtil.vala \
	HttpClient.vala \
	Json.vala \
	OAuth.vala \
	SixelConverter.vala \
	StringUtil.vala \
	Twitter.vala \
	ParsedUri.vala \
	System.OS.vala \
	System.OS.native.c \

SRCS_sixelv=	\
	sixelv.vala \
	Diag.vala \
	Dictionary.vala \
	SixelConverter.vala \
	StringUtil.vala \
	HttpClient.vala \
	OptArgs.vala \
	ParsedUri.vala \
	System.OS.vala \
	System.OS.native.c \

SRCS_h=	\
	h.vala \
	Curl.vala \
	Diag.vala \
	Dictionary.vala \
	HttpClient.vala \
	StringUtil.vala \
	ParsedUri.vala \

VALA_PKGS=	\
	--pkg posix \
	--pkg gdk-pixbuf-2.0 \

PKGS= \
	glib-2.0 \
	gdk-pixbuf-2.0 \
	gio-2.0 \
	libcurl \

COPTS=	-O2
CFLAGS=	-w `pkg-config --cflags ${PKGS}`
LIBS=	`pkg-config --libs ${PKGS}`

VALA_FLAGS=	\
	libcurl.vapi \

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

sayaka:	vala-make2
	@${VALAMAKE} ${VALAMAKE_COMMON_OPTS} \
		-o sayaka \
		${SRCS_sayaka}

.PHONY:	clean-sayaka
clean-sayaka:	vala-make2
	@${VALAMAKE} --clean ${VALAMAKE_COMMON_OPTS} \
		-o sayaka \
		${SRCS_sayaka}

sixelv:	vala-make2
	@${VALAMAKE} ${VALAMAKE_COMMON_OPTS} \
		-o sixelv \
		${SRCS_sixelv}

.PHONY:	clean-sixelv
clean-sixelv:	vala-make2
	@${VALAMAKE} --clean ${VALAMAKE_COMMON_OPTS} \
		-o sixelv \
		${SRCS_sixelv}

h:	vala-make2
	@${VALAMAKE} ${VALAMAKE_COMMON_OPTS} \
		-o h \
		${SRCS_h}

.PHONY:	clean-h
clean-h:	vala-make2
	@${VALAMAKE} --clean ${VALAMAKE_COMMON_OPTS} \
		-o h  \
		${SRCS_h}

.PHONY:	clean
clean:	clean-sayaka clean-sixelv clean-h

