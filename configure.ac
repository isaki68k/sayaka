#
# sayaka
# Copyright (C) 2021 isaki@NetBSD.org
#

AC_PREREQ([2.71])
AC_INIT([sayaka],[3.5],[])
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

# C++ コンパイラのワーニングオプション
AC_LANG([C++])
AC_DEFUN([ADD_CXX_WARN_IF],[
	AC_MSG_CHECKING([whether c++ accepts $1])
	old_CXXFLAGS="${CXXFLAGS}"
	CXXFLAGS="${CXXFLAGS} -Werror $1"
	AC_COMPILE_IFELSE([AC_LANG_PROGRAM([], [])],
		AC_MSG_RESULT([yes])
		WARNFLAGS="${WARNFLAGS} $1",
		AC_MSG_RESULT([no]))
	CXXFLAGS="${old_CXXFLAGS}"
])
ADD_CXX_WARN_IF(-Wcast-qual)
ADD_CXX_WARN_IF(-Wdisabled-macro-expansion)
ADD_CXX_WARN_IF(-Wextra-semi)
ADD_CXX_WARN_IF(-Winconsistent-missing-override)
ADD_CXX_WARN_IF(-Winconsistent-missing-destructor-override)
ADD_CXX_WARN_IF(-Wlogical-op)
ADD_CXX_WARN_IF(-Wmissing-prototypes)
ADD_CXX_WARN_IF(-Wmissing-variable-declarations)
ADD_CXX_WARN_IF(-Wvla-extension)
AC_SUBST([WARNFLAGS])

# Checks for header files.
AC_CHECK_HEADERS([bsd/bsd.h])
AC_CHECK_HEADERS([endian.h])
AC_CHECK_HEADERS([sys/endian.h])
AC_CHECK_HEADERS([sys/ttycom.h])

# Ubuntu 20.04 の <sys/sysctl.h> は AC_CHECK_HEADERS() では yes になるけど
# #warning が入ってて実際には使えない
AC_MSG_CHECKING([for sys/sysctl.h])
old_CPPFLAGS="${CPPFLAGS}"
CPPFLAGS="${CPPFLAGS} -Wcpp -Werror"
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
		#include <sys/sysctl.h>
	]], [[]])],[
		AC_MSG_RESULT(yes)
		AC_DEFINE(HAVE_SYS_SYSCTL_H)
	],[
		AC_MSG_RESULT(no)
	])
CPPFLAGS="${old_CPPFLAGS}"


# Checks for typedefs, structures, and compiler characteristics.

AC_MSG_CHECKING(for __builtin_bswap16)
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[]], [[
		return __builtin_bswap16(0);
	]])],[
		AC_MSG_RESULT(yes)
		AC_DEFINE(HAVE___BUILTIN_BSWAP16)
	],[
		AC_MSG_RESULT(no)
	])

AC_MSG_CHECKING(for __builtin_bswap32)
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[]], [[
		return __builtin_bswap32(0);
	]])],[
		AC_MSG_RESULT(yes)
		AC_DEFINE(HAVE___BUILTIN_BSWAP32)
	],[
		AC_MSG_RESULT(no)
	])

AC_MSG_CHECKING(for __builtin_bswap64)
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[]], [[
		return __builtin_bswap64(0);
	]])],[
		AC_MSG_RESULT(yes)
		AC_DEFINE(HAVE___BUILTIN_BSWAP64)
	],[
		AC_MSG_RESULT(no)
	])

AC_MSG_CHECKING(for __builtin_expect)
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[]], [[
		if (__builtin_expect(1, 1)) return 0;
	]])],[
		AC_MSG_RESULT(yes)
		AC_DEFINE(HAVE___BUILTIN_EXPECT)
	],[
		AC_MSG_RESULT(no)
	])

AC_MSG_CHECKING(for __attribute__((__format__)))
old_CPPFLAGS="${CPPFLAGS}"
CPPFLAGS="${CPPFLAGS} -Wall -Werror"
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([
		extern int f(const char *f, ...)
			__attribute__((__format__(__printf__, 1, 2)));
	], [])],[
		AC_MSG_RESULT([yes])
		AC_DEFINE([HAVE___ATTRIBUTE_FORMAT])
	],[
		AC_MSG_RESULT([no])
	])
CPPFLAGS="${old_CPPFLAGS}"



# Checks for library functions.

AC_SEARCH_LIBS(iconv, iconv,
	AC_DEFINE([HAVE_ICONV]),
)

AC_MSG_CHECKING(whether iconv needs const)
old_CPPFLAGS="${CPPFLAGS}"
CPPFLAGS="${CPPFLAGS} -Werror"
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
		#include <iconv.h>
	]], [[
		const char **a;
		iconv(0, a, 0, 0, 0);
	]])],[
		AC_MSG_RESULT(yes)
		AC_DEFINE(HAVE_ICONV_CONST)
	],[
		AC_MSG_RESULT(no)
	])
CPPFLAGS="${old_CPPFLAGS}"

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

if test -z "${PKG_CONFIG}"; then
	PKG_CONFIG=pkg-config
fi
${PKG_CONFIG} --version 2>&1 > /dev/null
if test $? \!= 0 ; then
	AC_MSG_ERROR([*** pkg-config (or \$PKG_CONFIG) not found])
fi

# stb-image はデフォルト採用で、without で外すことも可能。
AC_ARG_WITH([stb-image],
[  --without-stb-image     Use external JPG,PNG,GIF libs instead of stb_image],
	[],
	[with_stb_image=yes]
)

if test "x${with_stb_image}" = "xyes"; then
	AC_DEFINE([USE_STB_IMAGE])
	AC_SUBST([MAKE_STB_IMAGE], [yes])
else

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
		AC_MSG_ERROR([*** --without-stb-image was given, but pkg-config cannot find ${p}])
	fi
done

AC_MSG_CHECKING([for libgif])
old_LIBS=${LIBS}
LIBS="${LIBS} -lgif"
AC_LINK_IFELSE([AC_LANG_PROGRAM(
	[
		#include <gif_lib.h>
	], [
		DGifOpen(NULL, NULL, NULL);
	])], [
		AC_MSG_RESULT([yes])
	], [
		AC_MSG_RESULT([no])
		LIBS=${old_LIBS}
		AC_MSG_ERROR([*** libgif not found.])
	]
)

fi

AC_ARG_ENABLE(mbedtls,
[  --enable-mbedtls        Use mbedTLS instead of OpenSSL], [
	AC_DEFINE([USE_MBEDTLS])
	AC_SUBST([MAKE_MBEDTLS], [yes])
], [])

if test "${enable_mbedtls}x" = "yesx"; then

# Find mbedtls
PATHS="/usr/pkg /usr/local"

# libmbedtls と libmbedcrypto を同時に探す
AC_MSG_CHECKING([for mbedtls])
old_LIBS=${LIBS}
LIBS="${LIBS} -lmbedtls -lmbedcrypto"
AC_LINK_IFELSE([AC_LANG_PROGRAM(
	[
		#include <mbedtls/net_sockets.h>
		#include <mbedtls/entropy.h>
	], [
		mbedtls_net_context net;
		mbedtls_entropy_context e;
		mbedtls_net_init(&net);
		mbedtls_entropy_init(&e);
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
		LIBS="${LIBS} -L${p}/lib -lmbedtls -lmbedcrypto"

		AC_LINK_IFELSE([AC_LANG_PROGRAM(
			[
				#include <mbedtls/net_sockets.h>
				#include <mbedtls/entropy.h>
			], [
				mbedtls_net_context net;
				mbedtls_entropy_context e;
				mbedtls_net_init(&net);
				mbedtls_entropy_init(&e);
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
	AC_MSG_ERROR([*** mbedtls not found.
	On Ubuntu, sudo apt install libmbedtls-dev])
fi

# Check mbedtls version. need >= 2.9.0(?)
AC_CHECK_LIB(mbedtls, mbedtls_net_poll,
	has_mbedtls_net_poll=yes,
	has_mbedtls_net_poll=no)
if test "x${has_mbedtls_net_poll}" = "xno"; then
	AC_MSG_ERROR([*** mbedtls doesn't have mbedtls_net_poll().
	Please update your mbedtls to >= 2.9.0(?).])
fi

else
	# Use openssl
	LIBS="${LIBS} -lcrypto -lssl"
fi

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([Makefile.cfg])
AC_OUTPUT
