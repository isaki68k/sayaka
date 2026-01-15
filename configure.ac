#
# sayaka
# Copyright (C) 2021-2026 Tetsuya Isaki
#

AC_PREREQ([2.71 pkg-config])
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

AC_MSG_CHECKING(for __attribute__((__always_inline__)))
old_CFLAGS="${CFLAGS}"
CFLAGS="${CFLAGS} -Wall -Werror"
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([
		inline int f(int a) __attribute__((__always_inline__));
		int f(int a) { return a; }
	], [])],[
		AC_MSG_RESULT([yes])
		AC_DEFINE([HAVE___ATTRIBUTE_ALWAYS_INLINE])
	],[
		AC_MSG_RESULT([no])
	])
CFLAGS="${old_CFLAGS}"

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

AC_MSG_CHECKING(for __attribute__((__packed__)))
old_CFLAGS="${CFLAGS}"
CFLAGS="${CFLAGS} -Werror"
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([],
	[
		struct x { char a; int b; } __attribute__((__packed__));
	])], [
		AC_MSG_RESULT([yes])
		AC_DEFINE([HAVE___ATTRIBUTE_PACKED])
	], [
		AC_MSG_RESULT([no])
	])
CFLAGS="${old_CFLAGS}"

AC_MSG_CHECKING(for __attribute__((__unused__)))
old_CFLAGS="${CFLAGS}"
CFLAGS="${CFLAGS} -Wall -Werror"
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([],
	[
		int a __attribute__((__unused__));
	])], [
		AC_MSG_RESULT([yes])
		AC_DEFINE([HAVE___ATTRIBUTE_UNUSED])
	], [
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

# CHECK_PKG($1=PKGNAME, $2=pkgnames)
AC_DEFUN([CHECK_PKG], [
	PKG_CHECK_MODULES($1, $2, [
		HAVE_$1=yes
		AC_DEFINE([HAVE_$1])
		AC_SUBST([DEFINE_$1], [HAVE_$1=yes])
	], [
		HAVE_$1=no
	])
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
AC_CHECK_HEADERS([libkern/OSByteOrder.h])
AC_CHECK_HEADERS([machine/endian.h])
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

AC_MSG_CHECKING(for htobe16)
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([
		#if defined(HAVE_ENDIAN_H)
		#include <endian.h>
		#endif
		#if defined(HAVE_SYS_ENDIAN_H)
		#include <sys/endian.h>
		#endif
		#if defined(HAVE_MACHINE_ENDIAN_H)
		#include <machine/endian.h>
		#endif
	], [
		htobe16(0);
	])], [
		AC_MSG_RESULT([yes])
		AC_DEFINE([HAVE_HTOBE16])
	], [
		AC_MSG_RESULT([no])
	])

# libbsd on Linux
AC_SEARCH_LIBS(getprogname, bsd, has_getprogname=yes, has_getprogname=no)
if test "x${has_getprogname}" = "xno"; then
	AC_MSG_FAILURE([getprogname is not found.
	On Ubuntu, sudo apt install libbsd-dev])
fi

PKG_PROG_PKG_CONFIG

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
	CHECK_PKG([LIBJPEG], [libjpeg])
	CFLAGS="${CFLAGS} ${LIBJPEG_CFLAGS}"
	LIBS="${LIBS} ${LIBJPEG_LIBS}"

	if test x"${HAVE_LIBJPEG}" \!= x"yes"; then
		if test x"${with_libjpeg}" = x"yes"; then
			AC_MSG_FAILURE(
				[--with-libjpeg(=yes) is specified but libjpeg not found.])
		fi
	fi
	;;
esac

# libjxl はデフォルトを auto にする。
# --with-libjxl=auto なら、libjxl があれば使い、なければ使わない。
# --with-libjxl=yes  なら、libjxl がなければエラー。
# --with-libjxl=no (--without-libjxl) ならチェックもしない。
AC_ARG_WITH([libjxl],
	AS_HELP_STRING([--with-libjxl=(auto|yes|no)],
		[Use libjxl for JPEG XL (default:auto)]))
if test -z "${with_libjxl}"; then
	with_libjxl=auto
fi
case ${with_libjxl} in
 no)
	;;
 *)
	CHECK_PKG([LIBJXL], [libjxl])
	CFLAGS="${CFLAGS} ${LIBJXL_CFLAGS}"
	LIBS="${LIBS} ${LIBJXL_LIBS}"

	if test x"${HAVE_LIBJXL}" \!= x"yes"; then
		if test x"${with_libjxl}" = x"yes"; then
			AC_MSG_FAILURE(
				[--with-libjxl(=yes) is specified but libjxl not found.
	On NetBSD(pkgsrc), install pkgsrc/graphics/libjxl])
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
	CHECK_PKG([LIBPNG], [libpng])
	CFLAGS="${CFLAGS} ${LIBPNG_CFLAGS}"
	LIBS="${LIBS} ${LIBPNG_LIBS}"

	if test x"${HAVE_LIBPNG}" \!= x"yes"; then
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
	CHECK_PKG([LIBTIFF], [libtiff-4])
	CFLAGS="${CFLAGS} ${LIBTIFF_CFLAGS}"
	LIBS_SIXELV="${LIBS_SIXELV} ${LIBTIFF_LIBS}"

	if test x"${HAVE_LIBTIFF}" \!= x"yes"; then
		if test x"${with_libtiff}" = x"yes"; then
			AC_MSG_FAILURE(
				[--with-libtiff(=yes) is specified but libtiff not found.
	On NetBSD(pkgsrc), install pkgsrc/graphics/tiff])
		fi
	fi
	;;
esac

# libwebp はデフォルトを yes にする (Misskey では事実上必須なので)。
# ただし sixelv のみビルドしたいとか --force-blurhash があるので、なしも可。
# --with-libwebp=auto なら、libwebp があれば使い、なければ使わない。
# --with-libwebp=yes  なら、libwebp がなければエラー。
# --with-libwebp=no (--without-libwebp) ならチェックもしない。
AC_ARG_WITH([libwebp],
	AS_HELP_STRING([--with-libwebp=(auto|yes|no)],
		[Use libwebp for WebP (default:yes)]))
if test -z "${with_libwebp}"; then
	with_libwebp=yes
fi
case "${with_libwebp}" in
 no)
	;;
 *)
	CHECK_PKG([LIBWEBP], [libwebp libwebpdemux])
	CFLAGS="${CFLAGS} ${LIBWEBP_CFLAGS}"
	LIBS="${LIBS} ${LIBWEBP_LIBS}"

	if test x"${HAVE_LIBWEBP}" \!= x"yes"; then
		if test x"${with_libwebp}" = x"yes"; then
			AC_MSG_FAILURE([--with-libwebp is specified but libwebp not found.
	On NetBSD(pkgsrc), install pkgsrc/graphics/libwebp
	On Unubtu, sudo apt install libwebp-dev])
		fi
	fi
	;;
esac

# stb_image はデフォルトを auto にする。
# --with-stb-image=yes なら常に stb_image も使う。
# --with-stb-image=no (--without-stb-image) なら使わない。
# --with-stb-image=auto なら GIF,JPEG,PNG のいずれかの外部ライブラリがない
# 時に stb_image を使う。3つすべて外部ライブラリがあれば stb_image を使わない。
# stb_image でデコードできる BMP, PNM も一緒に巻き添えになるが気にしない。
AC_ARG_WITH([stb-image],
	AS_HELP_STRING([--with-stb-image=(auto|yes|no)],
		[Use bundled stb_image (defualt:auto)]))
if test -z "${with_stb_image}"; then
	with_stb_image=auto
fi
AC_MSG_CHECKING(whether to use bundled stb_image)
case "${with_stb_image}" in
 auto)
	if test x"${has_giflib}"  \!= x"yes" -o \
	        x"${HAVE_LIBJPEG}" \!= x"yes" -o \
	        x"${HAVE_LIBPNG}"  \!= x"yes"; then
		with_stb_image=yes
	else
		with_stb_image=no
	fi
	;;
 *)
	;;
esac
if test x"${with_stb_image}" = x"yes"; then
	AC_MSG_RESULT([yes])
	AC_DEFINE([WITH_STB_IMAGE])
	AC_SUBST([DEFINE_STB_IMAGE], [WITH_STB_IMAGE=yes])
else
	AC_MSG_RESULT([no])
fi

# 内蔵版 pnm はデフォルトを yes にする。
# --with-builtin-pnm=no (--without-builtin-pnm) なら使わない。
AC_ARG_WITH([builtin-pnm],
	AS_HELP_STRING([--with-builtin-pnm=(yes|no)],
		[Use built-in PNM loader (default:yes)]))
if test -z "${with_builtin_pnm}"; then
	with_builtin_pnm=yes
fi
AC_MSG_CHECKING(whether to use builtin pnm decoder)
case "${with_builtin_pnm}" in
 yes)
	AC_MSG_RESULT([yes])
	AC_DEFINE([HAVE_BUILTIN_PNM])
	AC_SUBST([DEFINE_BUILTIN_PNM], [HAVE_BUILTIN_PNM=yes])
	;;
 *)
	AC_MSG_RESULT([no])
	;;
esac

# 内蔵版柳沢 PIC はデフォルトを yes にする。
# --with-builtin-ypic=no (--without-builtin-ypic) なら使わない。
AC_ARG_WITH([builtin-ypic],
	AS_HELP_STRING([--with-builtin-ypic=(yes|no)],
		[Use built-in Yanagisawa-PIC loader (default:yes)]))
if test -z "${with_builtin_ypic}"; then
	with_builtin_ypic=yes
fi
AC_MSG_CHECKING(whether to use builtin Yanagisawa-PIC decoder)
case "${with_builtin_ypic}" in
 yes)
	AC_MSG_RESULT([yes])
	AC_DEFINE([HAVE_BUILTIN_YPIC])
	AC_SUBST([DEFINE_BUILTIN_YPIC], [HAVE_BUILTIN_YPIC=yes])
	;;
 *)
	AC_MSG_RESULT([no])
	;;
esac

AC_ARG_WITH([openssl], AS_HELP_STRING(
	[--with-openssl], [Use OpenSSL for HTTPS/WSS (default:yes)]))
case "${with_openssl}" in
 no)
	;;
 *)
	CHECK_LIB([openssl], [OPENSSL], [-lssl -lcrypto], [
		#include <openssl/evp.h>
		#include <openssl/ssl.h>
	], [
		SSL_library_init();		// libssl
		EVP_MD_CTX_new();		// libcrypto
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

AC_SUBST([LIBS_SIXELV])

# icu-uc
# これは開発用なので見付からなくても気にしない。
CHECK_PKG([ICU], [icu-uc])

# Uniq
CFLAGS=[`printf "%s" "${CFLAGS}" | awk 'BEGIN{RS=" ";ORS=" ";} !a[$0]++ {print}'`]
LIBS=[`printf "%s" "${LIBS}" | awk 'BEGIN{RS=" ";ORS=" ";} !a[$0]++ {print}'`]
LIBS_SIXELV=[`printf "%s" "${LIBS_SIXELV}" | awk 'BEGIN{RS=" ";ORS=" ";} !a[$0]++ {print}'`]

AC_CONFIG_HEADERS([${srcdir}/src/config.h])
AC_CONFIG_FILES([${srcdir}/src/Makefile.cfg])
AC_OUTPUT
