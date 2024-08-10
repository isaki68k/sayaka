#
# sayaka
# Copyright (C) 2021-2024 isaki@NetBSD.org
#

AC_PREREQ([2.71])
AC_INIT([sayaka],[ ],[])
AC_CONFIG_SRCDIR([./sixelv.c])
AC_CONFIG_AUX_DIR([scripts])

# Checks for programs.

# OS
AC_CANONICAL_HOST
AC_SUBST([host_cpu])

# C コンパイラのワーニングオプション
AC_LANG([C])
AC_DEFUN([ADD_CC_WARN_IF],[
	AC_MSG_CHECKING([whether cc accepts $1])
	old_CFLAGS="${CFLAGS}"
	CFLAGS="${CFLAGS} -Werror $1"
	AC_COMPILE_IFELSE([AC_LANG_PROGRAM([], [])],
		AC_MSG_RESULT([yes])
		eval `echo cc_has$1=yes | sed -e 's/-/_/g'`
		WARNFLAGS="${WARNFLAGS} $1",
		AC_MSG_RESULT([no]))
	CFLAGS="${old_CFLAGS}"
])
ADD_CC_WARN_IF(-Wcast-qual)
ADD_CC_WARN_IF(-Wextra-semi)
ADD_CC_WARN_IF(-Wlogical-op)
ADD_CC_WARN_IF(-Wmissing-prototypes)
ADD_CC_WARN_IF(-Wmissing-variable-declarations)
# -Wshadow-local があればそっちを使う(gcc>=7)、なければ -Wshadow (clang)
ADD_CC_WARN_IF(-Wshadow-local)
if test x"${cc_has_Wshadow_local}" != xyes; then
	ADD_CC_WARN_IF(-Wshadow)
fi
ADD_CC_WARN_IF(-Wstring-conversion)
ADD_CC_WARN_IF(-Wtype-limits)
AC_SUBST([WARNFLAGS])


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
old_CFLAGS="${CFLAGS}"
CFLAGS="${CFLAGS} -Wall -Werror"
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([
		extern int f(const char *f, ...)
			__attribute__((__format__(__printf__, 1, 2)));
	], [])],[
		AC_MSG_RESULT([yes])
		AC_DEFINE([HAVE___ATTRIBUTE_FORMAT])
	],[
		AC_MSG_RESULT([no])
	])
CFLAGS="${old_CFLAGS}"


# CHECK_HEADER($1=header, $2=HEADER)
AC_DEFUN([CHECK_HEADER], [
	AC_MSG_CHECKING([for $1])
	for path in ${PATHS}; do
		old_CFLAGS=${CFLAGS}
		case ${path} in
		 none)
			;;
		 *)
			CFLAGS="${CFLAGS} -I${path}/include"
			;;
		esac
		AC_COMPILE_IFELSE([AC_LANG_PROGRAM([
			#include <$1>
		], [])], [
			has_$2=yes
			break
		], [
			has_$2=no
		])
		CFLAGS=${old_CFLAGS}
	done
	if test x"${has_$2}" = x"yes"; then
		AC_MSG_RESULT([yes])
		AC_DEFINE([HAVE_$2])
	else
		AC_MSG_RESULT([no])
	fi
])

# CHECK_PKG($1=pkgname, $2=PKGNAME)
AC_DEFUN([CHECK_PKG], [
	AC_MSG_CHECKING([for $1])
	if ${PKG_CONFIG} --exists $1; then
		AC_MSG_RESULT([yes])
		AC_DEFINE([HAVE_$2])
		AC_SUBST([DEFINE_$2], [HAVE_$2=yes])
		has_$1=yes

		H=`${PKG_CONFIG} --cflags $1`
		L=`${PKG_CONFIG} --libs $1`
		CFLAGS="${CFLAGS} ${H}"
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
		old_CFLAGS=${CFLAGS}
		old_LIBS=${LIBS}
		case ${path} in
		 none)
			LIBS="${LIBS} $2"
			;;
		 *)
			CFLAGS="${CFLAGS} -I${path}/include"
			LIBS="${LIBS} -L${path}/lib $2"
			;;
		esac
		AC_LINK_IFELSE([AC_LANG_PROGRAM([$3], [$4])], [
			has_$1=yes
			break
		], [
			has_$1=no
		])
		CFLAGS=${old_CFLAGS}
		LIBS=${old_LIBS}
	done
	if test x"${has_$1}" = x"yes"; then
		AC_MSG_RESULT([yes])
	else
		AC_MSG_RESULT([no])
	fi
])

PATHS="none /usr/pkg /usr/local"


# Checks for header files.
AC_CHECK_HEADERS([bsd/bsd.h])
AC_CHECK_HEADERS([endian.h])
AC_CHECK_HEADERS([sys/endian.h])
AC_CHECK_HEADERS([sys/ttycom.h])

# Ubuntu 20.04 の <sys/sysctl.h> は AC_CHECK_HEADERS() では yes になるけど
# #warning が入ってて実際には使えない
AC_MSG_CHECKING([for sys/sysctl.h])
old_CFLAGS="${CFLAGS}"
CFLAGS="${CFLAGS} -Wcpp -Werror"
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
		#include <sys/sysctl.h>
	]], [[]])],[
		AC_MSG_RESULT(yes)
		AC_DEFINE(HAVE_SYS_SYSCTL_H)
	],[
		AC_MSG_RESULT(no)
	])
CFLAGS="${old_CFLAGS}"

# Checks for typedefs, structures, and compiler characteristics.

# Checks for library functions.

# libbsd on Linux
AC_SEARCH_LIBS(getprogname, bsd, has_getprogname=yes, has_getprogname=no)
if test "x${has_getprogname}" = "xno"; then
	AC_MSG_FAILURE([getprogname is not found.
	On Ubuntu, sudo apt install libbsd-dev])
fi

AC_CHECK_TOOL([PKG_CONFIG], [pkg-config], [:])
if test x"${PKG_CONFIG}" = x":"; then
	AC_MSG_FAILURE([pkg-config (or \$PKG_CONFIG) not found.
	On NetBSD(pkgsrc), install pkgsrc/devel/pkgconf
	On Ubuntu, sudo apt install pkg-config])
fi

# stb-image はデフォルト使用する。
AC_ARG_ENABLE([stb-image],
	AS_HELP_STRING([--enable-stb-image],
		[Use bundled stb_image (default:enable)]))
case "${with_stb_image}" in
 no)
	;;
 *)
	AC_DEFINE([HAVE_STB_IMAGE])
	AC_SUBST([DEFINE_STB_IMAGE], [HAVE_STB_IMAGE=yes])
	;;
esac

# libpng はデフォルト使用しない。
# --with-libpng が指定されたら、なければエラー終了する。

AC_ARG_WITH([libpng],
	AS_HELP_STRING([--with-libpng],
		[Use libpng for PNG instead of stb_image (default:no)]))
case ${with_libpng} in
 yes)
	CHECK_PKG([libpng], [LIBPNG])

	if test x"${has_libpng}" \!= x"yes"; then
		AC_MSG_FAILURE([--with-libpng is specified but libpng not found.])
	fi
	;;
 *)
	;;
esac

# WebP は stb_image にフォールバック出来ない (し Misskey では事実上必須なので)
# --without-libwebp が明示されない限り、なければエラー終了する。
AC_ARG_WITH([libwebp],
	AS_HELP_STRING([--with-libwebp], [Use libwebp (default:yes)]))
case "${with_libwebp}" in
 no)
	;;
 *)
	CHECK_PKG([libwebp], [LIBWEBP])

	if test x"${has_libwebp}" = x"yes"; then
		# どうするんだこれ
		LIBS="${LIBS} -lwebpdemux"
	else
		AC_MSG_FAILURE([--with-libwebp is specified but libwebp not found.
	On NetBSD(pkgsrc), install pkgsrc/graphics/libwebp
	On Unubtu, sudo apt install libwebp-dev])
	fi
	;;
esac

AC_ARG_WITH([libcurl],
	AS_HELP_STRING([--with-libcurl], [Use libcurl for network (default:yes)]))
case "${with_libcurl}" in
 no)
	;;
 *)
	CHECK_PKG([libcurl], [LIBCURL])

	AC_MSG_CHECKING([whether the libcurl has mbedtls backend])
	AC_RUN_IFELSE([AC_LANG_PROGRAM([
			#include <curl/curl.h>
		], [
			const curl_ssl_backend **list;
			int i;
			curl_global_sslset(CURLSSLBACKEND_NONE, NULL, &list);
			for (i = 0; list@<:@i@:>@; i++) {
				if (list@<:@i@:>@->id == CURLSSLBACKEND_MBEDTLS) {
					return 0;
				}
			}
			return 1;
		])], [
			AC_MSG_RESULT([yes])
			AC_DEFINE([LIBCURL_HAS_MBEDTLS_BACKEND])
		], [
			AC_MSG_RESULT([no])
		]
	)

	AC_MSG_CHECKING([whether the libcurl has openssl backend])
	AC_RUN_IFELSE([AC_LANG_PROGRAM([
			#include <curl/curl.h>
		], [
			const curl_ssl_backend **list;
			int i;
			curl_global_sslset(CURLSSLBACKEND_NONE, NULL, &list);
			for (i = 0; list@<:@i@:>@; i++) {
				if (list@<:@i@:>@->id == CURLSSLBACKEND_OPENSSL) {
					return 0;
				}
			}
			return 1;
		])], [
			AC_MSG_RESULT([yes])
			AC_DEFINE([LIBCURL_HAS_OPENSSL_BACKEND])
		], [
			AC_MSG_RESULT([no])
		]
	)

	;;
esac

# Uniq
CFLAGS=[`echo -n ${CFLAGS} | awk 'BEGIN{RS=" ";ORS=" ";} !a[$0]++ {print}'`]
LIBS=[`echo -n ${LIBS} | awk 'BEGIN{RS=" ";ORS=" ";} !a[$0]++ {print}'`]

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([Makefile.cfg])
AC_OUTPUT
