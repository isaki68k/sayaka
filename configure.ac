#
# sayaka
# Copyright (C) 2021 isaki@NetBSD.org
#

AC_PREREQ([2.69])
AC_INIT([sayaka], [3.5], [])
AC_CONFIG_SRCDIR([src/sayaka.cpp])
AC_CONFIG_AUX_DIR([scripts])

# Checks for programs.
AC_PROG_CXX

# OS
AC_CANONICAL_HOST
AC_SUBST([host_cpu])

# C++ のバージョンをチェック
CXX_STD=-std=c++17
AC_LANG_PUSH([C++])
AC_MSG_CHECKING(for ${CXX_STD})
old_CXXFLAGS="${CXXFLAGS}"
CXXFLAGS="${CXX_STD} ${CXXFLAGS}"
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([], [])],
	AC_MSG_RESULT([yes]),
	AC_MSG_ERROR("*** C++ compiler '${CXX}' doesn't support ${CXX_STD}."))
CXXFLAGS="${old_CXXFLAGS}"
CXX="${CXX} ${CXX_STD}"
AC_LANG_POP([C++])

# Checks for header files.
AC_CHECK_HEADERS([bsd/bsd.h])

# Checks for typedefs, structures, and compiler characteristics.

# Checks for library functions.

AC_MSG_CHECKING(for iconv const)
AC_TRY_COMPILE(
	[
		#include <iconv.h>
	], [
		const char **a;
		iconv(0, a, 0, 0, 0);
	],
	AC_MSG_RESULT([yes])
		AC_DEFINE([HAVE_ICONV_CONST]),
	AC_MSG_RESULT([no]))

# libbsd on Linux
AC_SEARCH_LIBS(getprogname, bsd, has_getprogname=yes, has_getprogname=no)
if test "x${has_getprogname}" = "xno"; then
	AC_MSG_ERROR([*** getprogname is not found.
	On Ubuntu, sudo apt install libbsd-dev])
fi
#AC_CHECK_LIB(c, getprogname, ,)
#AC_CHECK_LIB(bsd, getprogname,
#	LIBS="${LIBS} -lbsd"
#)
# なければ停止する
#AC_COMPILE_IFELSE([AC_LANG_PROGRAM(
#	[
#		#include <stdlib.h>
#		#include "confdefs.h"
#		#if defined(HAVE_BSD_BSD_H)
#		#include <bsd/bsd.h>
#		#endif
#	], [
#		getprogname()
#	])],
#	# nothing to do on success
#	, [AC_MSG_ERROR([*** getprogname is not found.
#	On Ubuntu, sudo apt install libbsd-dev])
#	]
#)

PKG_CONFIG=pkg-config

PKGS="libjpeg libpng"
for p in ${PKGS}; do
	AC_MSG_CHECKING([for ${p}])

	if ${PKG_CONFIG} --exists ${p}; then
		AC_MSG_RESULT([yes])
		H=`${PKG_CONFIG} --cflags ${p}`
		L=`${PKG_CONFIG} --libs ${p}`

		CPPFLAGS="${CPPFLAGS} ${H}"
		LIBS="${LIBS} ${L}"
	else
		AC_MSG_RESULT([no])
		AC_MSG_ERROR([*** pkg-config cannot find ${p}])
	fi
done

# Find mbedtls
PATHS="/usr/pkg /usr/local"

AC_MSG_CHECKING([for mbedtls])
old_LIBS=${LIBS}
LIBS="${LIBS} -lmbedtls"
AC_LINK_IFELSE([AC_LANG_PROGRAM(
	[
		#include <mbedtls/net_sockets.h>
	], [
		mbedtls_net_context net;
		mbedtls_net_init(&net);
	])], has_mbedtls=yes, has_mbedtls=no
)
if test "x${has_mbedtls}" = "xyes"; then
	AC_MSG_RESULT([yes])
else
	LIBS=${old_LIBS}

	# Try PATHS
	old_CPPFLAGS=${CPPFLAGS}
	for p in ${PATHS}; do
		CPPFLAGS="${CPPFLAGS} -I${p}/include"
		case "${host_os}" in
		 netbsd*)
			LIBS="${LIBS} -Wl,-R,${p}/lib"
			;;
		esac
		LIBS="${LIBS} -L${p}/lib -lmbedtls"

		AC_LINK_IFELSE([AC_LANG_PROGRAM(
			[
				#include <mbedtls/net_sockets.h>
			], [
				mbedtls_net_context net;
				mbedtls_net_init(&net);
			])], has_mbedtls=yes, has_mbedtls=no
		)
		if test "x${has_mbedtls}" = "xyes"; then
			AC_MSG_RESULT([${p}])
			break
		else
			CPPFLAGS=${old_CPPFLAGS}
			LIBS=${old_LIBS}
		fi
	done
fi
if test "x${has_mbedtls}" = "xno"; then
	AC_MSG_RESULT([no])
	AC_MSG_ERROR([*** mbedtls not found])
fi

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([Makefile.cfg])
AC_OUTPUT
