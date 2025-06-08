#
# sayaka
# Copyright (C) 2021-2025 isaki@NetBSD.org
#

AC_PREREQ([2.71])
AC_INIT([sayaka],[ ],[])
AC_CONFIG_SRCDIR([./src/sayaka.c])
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

# コンパイラの組み込み関数
# $1=func, $2=FUNC, $3=expr
AC_DEFUN([CHECK_BUILTIN_FUNC], [
	AC_MSG_CHECKING([for $1])
	old_CFLAGS="${CFLAGS}"
	CFLAGS="${CFLAGS} -Werror"
	AC_COMPILE_IFELSE([AC_LANG_PROGRAM([], [
		$3
	])], [
		AC_MSG_RESULT(yes)
		AC_DEFINE([HAVE_$2])
	], [
		AC_MSG_RESULT(no)
	])
	CFLAGS="${old_CFLAGS}"
])
CHECK_BUILTIN_FUNC(__builtin_clz, __BUILTIN_CLZ,
	[__builtin_clz(1)])
CHECK_BUILTIN_FUNC(__builtin_expect, __BUILTIN_EXPECT,
	[__builtin_expect(1, 1)])
CHECK_BUILTIN_FUNC(__builtin_unreachable, __BUILTIN_UNREACHABLE,
	[__builtin_unreachable()])

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
	if test x"${PKG_CONFIG}" \!= x":" && ${PKG_CONFIG} --exists $1; then
		AC_MSG_RESULT([yes])
		AC_DEFINE([HAVE_$2])
		AC_SUBST([DEFINE_$2], [HAVE_$2=yes])
		eval `echo has_$1=yes | sed -e 's/-/_/g'`

		H=`${PKG_CONFIG} --cflags $1`
		L=`${PKG_CONFIG} --libs $1`
		CFLAGS="${CFLAGS} ${H}"
		LIBS="${LIBS} ${L}"
	else
		AC_MSG_RESULT([no])
		eval `echo has_$1=no | sed -e 's/-/_/g'`
	fi
])

# CHECK_LIB($1=name, $2=NAME, $3=libs, $4=include, $5=src)
AC_DEFUN([CHECK_LIB], [
	AC_MSG_CHECKING([for $1])
	for path in ${PATHS}; do
		old_CFLAGS=${CFLAGS}
		old_LIBS=${LIBS}
		case ${path} in
		 none)
			LIBS="${LIBS} $3"
			;;
		 *)
			CFLAGS="${CFLAGS} -I${path}/include"
			LIBS="${LIBS} -L${path}/lib $3"
			;;
		esac
		AC_LINK_IFELSE([AC_LANG_PROGRAM([$4], [$5])], [
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
		AC_DEFINE([HAVE_$2])
		AC_SUBST([DEFINE_$2], [HAVE_$2=yes])
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

AC_MSG_CHECKING([for __thread])
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([
		#include <stdlib.h>
	], [
		static __thread int a;
	])], [
		AC_MSG_RESULT(yes)
		AC_SUBST([DEFINE___THREAD], [HAVE___THREAD=yes])
	], [
		AC_MSG_RESULT(no)
	])


# Checks for library functions.

AC_CHECK_FUNCS([arc4random])
AC_CHECK_FUNCS([arc4random_buf])

# libbsd on Linux
AC_SEARCH_LIBS(getprogname, bsd, has_getprogname=yes, has_getprogname=no)
if test "x${has_getprogname}" = "xno"; then
	AC_MSG_FAILURE([getprogname is not found.
	On Ubuntu, sudo apt install libbsd-dev])
fi

AC_CHECK_TOOL([PKG_CONFIG], [pkg-config], [:])
case "${PKG_CONFIG}" in
 :)
	AC_MSG_FAILURE([pkg-config (or \$PKG_CONFIG) not found.
	On NetBSD(pkgsrc), install pkgsrc/devel/pkgconf
	On Ubuntu, sudo apt install pkg-config
	Or you can ignore pkg-config by 'env PKG_CONFIG=no ...'])
	;;
 no)
	PKG_CONFIG=:
	;;
 *)
	;;
esac

# giflib
AC_ARG_WITH([giflib],
	AS_HELP_STRING([--with-giflib=(auto|yes|no)],
		[Use giflib for GIF instead of stb_image (default:auto)]))
if test -z "${with_giflib}"; then
	with_giflib=auto
fi
case ${with_giflib} in
 no)
	;;
 *)
	AC_MSG_CHECKING([for giflib])
	has_giflib=no
	old_CFLAGS=${CFLAGS}
	old_LIBS=${LIBS}
	for path in ${PATHS}; do
		case ${path} in
		 none)
			LIBS="${old_LIBS} -lgif"
			;;
		 *)
			CFLAGS="${old_CFLAGS} -I${path}/include"
			LIBS="${old_LIBS} -L${path}/lib -lgif"
			;;
		esac
		AC_LINK_IFELSE([AC_LANG_PROGRAM([
			#include <gif_lib.h>
		], [
			DGifOpen(NULL, NULL, NULL);
		])], [
			has_giflib=yes
			break
		], [
		])
	done

	if test x"${has_giflib}" = x"yes"; then
		AC_MSG_RESULT([yes])
		AC_DEFINE([HAVE_GIFLIB])
		AC_SUBST([DEFINE_GIFLIB], [HAVE_GIFLIB=yes])

		dnl リンクできれば -Wl,-R が必要か調べる。
		AC_MSG_CHECKING([whether giflib requires -Wl,-R])
		AC_RUN_IFELSE([AC_LANG_SOURCE([[
			int main() {
				return 0;
			}
		]])], [
			dnl 実行できれば追加しなくていい。
			AC_MSG_RESULT([no])
		], [
			AC_MSG_RESULT([yes])
			LIBS="${old_LIBS} -L${path}/lib -Wl,-R,${path}/lib -lgif"
		])
	else
		AC_MSG_RESULT([no])
		CFLAGS="${old_CFLAGS}"
		LIBS="${old_LIBS}"

		if test x"${with_giflib}" = x"yes"; then
			AC_MSG_FAILURE(
				[--with-giflib(=yes) is specified but giflib not found.])
		fi
	fi
	;;
esac

# libjpeg はデフォルトを auto にする。
# --with-libjpeg=auto なら、libjpeg があれば使い、なければ stb_image を使う。
# --with-libjpeg=yes  なら、libjpeg がなければエラー。
# --with-libjpeg=no (--without-libjpeg) なら、stb_image を使う。
AC_ARG_WITH([libjpeg],
	AS_HELP_STRING([--with-libjpeg=(auto|yes|no)],
		[Use libjpeg for JPEG instead of stb_image (default:auto)]))
if test -z "${with_libjpeg}"; then
	with_libjpeg=auto
fi
case ${with_libjpeg} in
 no)
	;;
 *)
	CHECK_PKG([libjpeg], [LIBJPEG])

	if test x"${has_libjpeg}" \!= x"yes"; then
		if test x"${with_libjpeg}" = x"yes"; then
			AC_MSG_FAILURE(
				[--with-libjpeg(=yes) is specified but libjpeg not found.])
		fi
	fi
	;;
esac

# libpng はデフォルトを auto にする。
# --with-libpng=auto なら、libpng があれば使い、なければ stb_image を使う。
# --with-libpng=yes  なら、libpng がなければエラー。
# --with-libpng=no (--without-libpng) なら、stb_image を使う。
AC_ARG_WITH([libpng],
	AS_HELP_STRING([--with-libpng=(auto|yes|no)],
		[Use libpng for PNG instead of stb_image (default:auto)]))
if test -z "${with_libpng}"; then
	with_libpng=auto
fi
case ${with_libpng} in
 no)
	;;
 *)
	CHECK_PKG([libpng], [LIBPNG])

	if test x"${has_libpng}" \!= x"yes"; then
		if test x"${with_libpng}" = x"yes"; then
			AC_MSG_FAILURE(
				[--with-libpng(=yes) is specified but libpng not found.])
		fi
	fi
	;;
esac

# libtiff はデフォルトを auto にする。
# --with-libtiff=auto なら、libtiff があれば使い、なければ使わない。
# --with-libtiff=yes  なら、libtiff がなければエラー。
# --with-libtiff=no (--without-libtiff) ならチェックもしない。
AC_ARG_WITH([libtiff],
	AS_HELP_STRING([--with-libtiff=(auto|yes|no)],
		[Use libtiff for TIFF (default:auto)]))
if test -z "${with_libtiff}"; then
	with_libtiff=auto
fi
case ${with_libtiff} in
 no)
	;;
 *)
	CHECK_PKG([libtiff-4], [LIBTIFF])

	if test x"${has_libtiff}" \!= x"yes"; then
		if test x"${with_libtiff}" = x"yes"; then
			AC_MSG_FAILURE(
				[--with-libtiff(=yes) is specified but libtiff not found.])
		fi
	fi
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

AC_ARG_WITH([openssl], AS_HELP_STRING(
	[--with-openssl], [Use OpenSSL for HTTPS/WSS (default:yes)]))
case "${with_openssl}" in
 no)
	;;
 *)
	CHECK_LIB([openssl], [OPENSSL], [-lssl -lcrypto], [
		#include <openssl/sha.h>
		#include <openssl/ssl.h>
	], [
		SSL_library_init();		// libssl
		SHA1_Init(NULL);		// libcrypto
	])
	if test x"${has_openssl}" \!= x"yes"; then
		AC_MSG_FAILURE(
			[--with-openssl is specified but openssl not found.
	On Ubuntu, sudo apt install libssl-dev])
	fi
	;;
esac

# iconv
AC_ARG_WITH([iconv],
	AS_HELP_STRING([--with-iconv],
		[Use iconv to convert output charset (default:yes)]))
case "${with_iconv}" in
 no)
	;;
 *)
	# まずベースシステムの iconv が使えるか調べる。
	# configure のここまでの処理によって各種ライブラリのためにインクルード
	# パスが求められているはず (なのでこの iconv を最後に行う)。
	# もし OS の iconv と外部ライブラリの iconv がどちらも存在する場合、
	# ヘッダファイルはその検索順から外部ライブラリのものが使われるが -l で
	# 外部ライブラリを指定しないので、リンクに失敗する。これを回避して
	# OS 標準のものが使えるよう、外部ライブラリは -DLIBICONV_PLUG が
	# 与えられると、自分ではなく OS の iconv を使うように名前を向けてくれる
	# 機能があるようだ。正気か?
	# なのでまずは (外部ライブラリがいてもいなくても) -DLIBICONV_PLUG を
	# 指定して OS の iconv がリンク出来るか試す。
	# リンクできれば OS が iconv を持っている (素の NetBSD/FreeBSD とか)。
	# 出来なければ OS が iconv を持っていない (OpenBSD とか)。
	AC_MSG_CHECKING([for iconv in base system])
	has_iconv=no
	AC_LINK_IFELSE([AC_LANG_PROGRAM([[
			#define LIBICONV_PLUG
			#include <iconv.h>
		]], [[
			iconv_t cd = iconv_open("", "");
		]])], [
			AC_MSG_RESULT([yes])
			has_iconv=base
			AC_DEFINE([HAVE_ICONV])
			CFLAGS="${CFLAGS} -DLIBICONV_PLUG"
		], [
			AC_MSG_RESULT([no])
		])

	if test x"${has_iconv}" = x"no"; then
		# ベースシステムが iconv を持っていないので、外部ライブラリを試す。
		# インクルードパスはおそらくもう外部ライブラリを見ているので
		# -DLIBICONV_PLUG なしで -liconv を指定してリンクできれば iconv あり。
		# リンクできなければ iconv なし (素の OpenBSD とか)。
		# 外部ライブラリを優先して使いたいケースは未考慮。
		CHECK_LIB([iconv], [ICONV], [-liconv], [
			#include <iconv.h>
		], [
			iconv_t cd = iconv_open("", "");
		])
		# has_iconv=yes/no がセットされる
	fi

	if test x"${has_iconv}" = x"no"; then
		AC_MSG_FAILURE([iconv not found.
	On OpenBSD, install ports/converter/libiconv
	Or specify --without-iconv])
	fi

	# OS によって第2引数(src)の型が違う。どうしてこうなった…
	AC_MSG_CHECKING(whether iconv needs const)
	old_CFLAGS="${CFLAGS}"
	CFLAGS="${CFLAGS} -Werror"
	AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
			#include <iconv.h>
		]], [[
			const char *a;
			iconv(0, &a, 0, 0, 0);
		]])],[
			AC_MSG_RESULT(yes)
			AC_DEFINE(ICONV_HAS_CONST_SRC)
		],[
			AC_MSG_RESULT(no)
		])
	CFLAGS="${old_CFLAGS}"
	;;
esac

# icu-uc
# これはメインで使うものではないので変数も分ける。
# そのため CHECK_PKG を横目に見ながら似たのを用意する。
# 見付からなくても気にしない。
AC_MSG_CHECKING([for icu-uc])
if test x"${PKG_CONFIG}" \!= x":" && ${PKG_CONFIG} --exists icu-uc; then
	AC_MSG_RESULT([yes])

	CFLAGS_ICU=`${PKG_CONFIG} --cflags icu-uc`
	LIBS_ICU=`${PKG_CONFIG} --libs icu-uc`
	AC_SUBST([CFLAGS_ICU])
	AC_SUBST([LIBS_ICU])
else
	AC_MSG_RESULT([no])
fi


# Uniq
CFLAGS=[`echo -n ${CFLAGS} | awk 'BEGIN{RS=" ";ORS=" ";} !a[$0]++ {print}'`]
LIBS=[`echo -n ${LIBS} | awk 'BEGIN{RS=" ";ORS=" ";} !a[$0]++ {print}'`]

AC_CONFIG_HEADERS([${srcdir}/src/config.h])
AC_CONFIG_FILES([${srcdir}/src/Makefile.cfg])
AC_OUTPUT
