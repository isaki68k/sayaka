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
	AC_MSG_FAILURE("C++ compiler '${CXX}' doesn't support ${CXX_STD}."))
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
	AC_MSG_FAILURE([getprogname is not found.
	On Ubuntu, sudo apt install libbsd-dev])
fi

AC_CHECK_TOOL([PKG_CONFIG], [pkg-config], [:])
if test x"${PKG_CONFIG}" = x":"; then
	AC_MSG_FAILURE([pkg-config (or \$PKG_CONFIG) not found])
fi

# CHECK_PKG($1=pkgname)
AC_DEFUN([CHECK_PKG], [
	AC_MSG_CHECKING([for $1])
	if ${PKG_CONFIG} --exists $1; then
		AC_MSG_RESULT([yes])
		has_$1=yes
		H=`${PKG_CONFIG} --cflags $1`
		L=`${PKG_CONFIG} --libs $1`

		CPPFLAGS="${CPPFLAGS} ${H}"
		LIBS="${LIBS} ${L}"
	else
		AC_MSG_RESULT([no])
		has_$1=no
	fi
])

# CHECK_LIB($1=name, $2=libs, $3=include, $4=src)
AC_DEFUN([CHECK_LIB], [
	AC_MSG_CHECKING([for $1])
	for path in ${PATHS}; do
		old_CPPFLAGS=${CPPFLAGS}
		old_LIBS=${LIBS}
		case ${path} in
		 none)
			LIBS="${LIBS} $2"
			;;
		 *)
			CPPFLAGS="${CPPFLAGS} -I${path}/include"
			LIBS="${LIBS} -L${path}/lib $2"
			;;
		esac
		AC_LINK_IFELSE([AC_LANG_PROGRAM([$3], [$4])], [
			has_$1=yes
			break
		], [
			has_$1=no
		])
		CPPFLAGS=${old_CPPFLAGS}
		LIBS=${old_LIBS}
	done
	if test x"${has_$1}" = x"yes"; then
		AC_MSG_RESULT([yes])
	else
		AC_MSG_RESULT([no])
	fi
])

PATHS="none /usr/pkg /usr/local"

#
# Webp
#
CHECK_PKG([libwebp])
if test x"${has_libwebp}" = x"yes"; then
	# どうするんだこれ
	LIBS="${LIBS} -lwebpdemux"
else
	AC_MSG_FAILURE([libwebp not found.
	On NetBSD(pkgsrc), install pkgsrc/graphics/libwebp
	On Unubtu, sudo apt install libwebp-dev])
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
	#
	# libjpeg
	#
	CHECK_PKG([libjpeg])
	if test x"${has_libjpeg}" \!= x"yes"; then
		AC_MSG_FAILURE(
			[--without-stb-image was given, but pkg-config cannot find libjpeg])
	fi

	#
	# libpng
	#
	CHECK_PKG([libpng])
	if test x"${has_libpng}" \!= x"yes"; then
		AC_MSG_FAILURE(
			[--without-stb-image was given, but pkg-config cannot find libpng])
	fi

	#
	# libgif
	#
	CHECK_LIB([libgif], [-lgif], [
		#include <gif_lib.h>
	], [
		DGifOpen(NULL, NULL, NULL)
	])
	if test x"${has_libgif}" \!= x"yes"; then
		AC_MSG_FAILURE([--without-stb-image was given, but libgif not found.
	On NetBSD(pkgsrc), install pkgsrc/graphics/giflib
	On Ubuntu, sudo apt install libgif-dev])
	fi
fi

#
# wslay
#
case $host_os in
 freebsd*)	wslay_libname="wslay_shared" ;;	# why?
 *)			wslay_libname="wslay" ;;
esac
CHECK_LIB([wslay], [-l${wslay_libname}], [
	#include <wslay/wslay.h>
], [
	wslay_event_context_client_init(NULL, NULL, NULL)
])
if test "x${has_wslay}" \!= "xyes"; then
	AC_MSG_FAILURE([wslay not found.
	On NetBSD(pkgsrc), install pkgsrc/www/wslay
	On Ubuntu, sudo apt install libwslay1 libwslay-dev])
fi

AC_ARG_WITH(mbedtls,
[  --without-mbedtls       Use OpenSSL instead of mbedTLS],
	[],
	[with_mbedtls=yes]
)
if test x"${with_mbedtls}" = x"yes"; then
	#
	# mbedTLS
	#
	CHECK_LIB([mbedtls], [-lmbedtls -lmbedcrypto], [
	#include <mbedtls/net_sockets.h>
		#include <mbedtls/entropy.h>
	], [
		mbedtls_net_context net;
		mbedtls_entropy_context e;
		mbedtls_net_init(&net);
		mbedtls_entropy_init(&e);
	])
	if test "x${has_mbedtls}" = "xyes"; then
		AC_DEFINE([USE_MBEDTLS])
		AC_SUBST([MAKE_MBEDTLS], [yes])
	else
		AC_MSG_RESULT([no])
		AC_MSG_FAILURE([--with-mbedtls was given, but mbedtls not found.
	On NetBSD(pkgsrc), install pkgsrc/security/mbedtls
	On OpenBSD(ports), install ports/security/polarssl
	On Ubuntu, sudo apt install libmbedtls-dev])
	fi

	# Check mbedtls version. need >= 2.9.0(?)
	AC_CHECK_LIB(mbedtls, mbedtls_net_poll,
		has_mbedtls_net_poll=yes,
		has_mbedtls_net_poll=no)
	if test "x${has_mbedtls_net_poll}" = "xno"; then
		AC_MSG_FAILURE([--with-mbedtls was given, but mbedtls doesn't have mbedtls_net_poll().
	Please update your mbedtls to >= 2.9.0(?).])
	fi
else
	#
	# OpenSSL
	#
	CHECK_LIB([openssl], [-lssl -lcrypto], [
		#include <openssl/sha.h>
		#include <openssl/ssl.h>
	], [
		SSL_library_init();		// libssl
		SHA1_Init(NULL);		// libcrypto
	])
	if test x"${has_openssl}" \!= x"yes"; then
		AC_MSG_FAILURE([--without-mbedtls was given, but OpenSSL not found.
	On Ubuntu, sudo apt install libssl-dev])
	fi
fi

AC_ARG_ENABLE(twitter,
[  --enable-twitter        Enable twitter (playback only)],,
	[
	case "${enableval}" in
	 yes)	enable_twitter=yes	;;
	 *)		enable_twitter=no	;;
	esac],
	[enable_twitter=no])

if test x"${enable_twitter}" = x"yes"; then
	AC_DEFINE([USE_TWITTER])
	AC_SUBST([MAKE_TWITTER], [yes])
fi

# Uniq
CPPFLAGS=[`echo -n ${CPPFLAGS} | awk 'BEGIN{RS=" ";ORS=" ";} !a[$0]++ {print}'`]
LIBS=[`echo -n ${LIBS} | awk 'BEGIN{RS=" ";ORS=" ";} !a[$0]++ {print}'`]


AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([Makefile.cfg])
AC_OUTPUT

echo
echo "sayaka was configured as follows"
echo -n " Supported API              : Misskey"
if test x"${enable_twitter}" = x"yes"; then
	echo -n " Twitter"
fi
echo ""
echo -n " Image loader               : "
if test "x${with_stb_image}" = "xyes"; then
	echo "stb_image(bundled)"
else
	echo "External JPEG,PNG,GIF libraries"
fi
echo -n " SSL/TLS library            : "
if test "x${with_mbedtls}" = "xyes"; then
	echo "mbedTLS"
else
	echo "OpenSSL"
fi
