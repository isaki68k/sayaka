#
# sayaka - twitter client
#

# Copyright (C) 2011-2014 Tetsuya Isaki
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

LOCALDIR=	/usr/local
BINDIR=		${LOCALDIR}/bin
LIBDIR=		${LOCALDIR}/share/sayaka

INSTALL_BIN=	install -c -o 0 -g 0 -m 0755
INSTALL_DIR=	install -d -o 0 -g 0 -m 0755
INSTALL_DATA=	install -c -o 0 -g 0 -m 0644

SRCS=	\
		config.php	\
		colormap16.png	\
		OAuth.php	\
		sayaka.php	\
		subr.php	\
		TwistOAuth.php	\
		twitteroauth.php

all:	sayaka

sayaka:	bin.sh
	sed -e 's#SAYAKA_BASE=.#SAYAKA_BASE=${LIBDIR}#' bin.sh > sayaka

install:
	${INSTALL_DIR}	${BINDIR}
	${INSTALL_DIR}  ${LIBDIR}
	${INSTALL_BIN}  sayaka ${BINDIR}
	${INSTALL_DATA} ${SRCS} ${LIBDIR}

clean:
	rm -f sayaka
