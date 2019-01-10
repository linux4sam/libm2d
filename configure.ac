AC_PREREQ([2.69])
AC_INIT([libm2d], [1.0])
AC_CONFIG_SRCDIR([src/m2d.c])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])
AC_CANONICAL_SYSTEM
PKG_PROG_PKG_CONFIG

AM_INIT_AUTOMAKE([1.10 foreign])

# Enable quiet compiles on automake 1.11.
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

# Checks for programs.
AC_PROG_CXX
AC_PROG_CC
AC_PROG_INSTALL

AC_DEFUN([LIBM2D_CC_TRY_FLAG], [
  AC_MSG_CHECKING([whether $CC supports $1])

  libm2d_save_CFLAGS="$CFLAGS"
  CFLAGS="$CFLAGS $1"

  AC_COMPILE_IFELSE([AC_LANG_SOURCE([ ])], [libm2d_cc_flag=yes], [libm2d_cc_flag=no])
  CFLAGS="$libm2d_save_CFLAGS"

  if test "x$libm2d_cc_flag" = "xyes"; then
    ifelse([$2], , :, [$2])
  else
    ifelse([$3], , :, [$3])
  fi
  AC_MSG_RESULT([$libm2d_cc_flag])
])

MAYBE_WARN="-Wall -Wextra \
-Wsign-compare -Werror-implicit-function-declaration \
-Wpointer-arith -Wwrite-strings -Wstrict-prototypes \
-Wmissing-prototypes -Wmissing-declarations -Wnested-externs \
-Wpacked -Wswitch-enum \
-Wstrict-aliasing=2 -Winit-self \
-Wdeclaration-after-statement -Wold-style-definition \
-Wno-unused-parameter \
-Wno-attributes -Wno-long-long -Winline -Wshadow \
-Wno-missing-field-initializers"

# invalidate cached value if MAYBE_WARN has changed
if test "x$libm2d_cv_warn_maybe" != "x$MAYBE_WARN"; then
	unset libm2d_cv_warn_cflags
fi
AC_CACHE_CHECK([for supported warning flags], libm2d_cv_warn_cflags, [
	echo
	WARN_CFLAGS=""

	# Some warning options are not supported by all versions of
	# gcc, so test all desired options against the current
	# compiler.
	#
	# Note that there are some order dependencies
	# here. Specifically, an option that disables a warning will
	# have no net effect if a later option then enables that
	# warnings, (perhaps implicitly). So we put some grouped
	# options (-Wall and -Wextra) up front and the -Wno options
	# last.

	for W in $MAYBE_WARN; do
		LIBM2D_CC_TRY_FLAG([$W], [WARN_CFLAGS="$WARN_CFLAGS $W"])
	done

	libm2d_cv_warn_cflags=$WARN_CFLAGS
	libm2d_cv_warn_maybe=$MAYBE_WARN

	AC_MSG_CHECKING([which warning flags were supported])])
WARN_CFLAGS="$libm2d_cv_warn_cflags"
AC_SUBST(WARN_CFLAGS)

# Checks for header files.
AC_CHECK_HEADERS([fcntl.h stdint.h stdlib.h string.h sys/ioctl.h unistd.h])

AX_PKG_CHECK_MODULES(LIBDRM, [libdrm >= 2.4.0], [], [], [AC_MSG_ERROR([
     libdrm is a hard dependency, but a sufficient version was not found.])])

AC_ARG_ENABLE([cairo],
              [AS_HELP_STRING([--disable-cairo],
                              [Disable support for cairo (default: auto)])],
              [CAIRO=$enableval], [CAIRO=yes])

if test "x$CAIRO" = xyes; then
   AX_PKG_CHECK_MODULES(CAIRO, [], [cairo >= 1.14.6], [have_cairo=yes], [have_cairo=no])
   AC_DEFINE(HAVE_CAIRO, 1, [Have cairo support])
fi
AM_CONDITIONAL(HAVE_CAIRO, [test "x$have_cairo" = xyes])

# Checks for typedefs, structures, and compiler characteristics.
AC_CHECK_HEADER_STDBOOL
AC_C_INLINE
AC_TYPE_SIZE_T
AC_TYPE_UINT32_T
AC_TYPE_UINT8_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_MMAP
AC_CHECK_FUNCS([clock_gettime getpagesize memset munmap strerror])

pkgconfigdir=${libdir}/pkgconfig
AC_SUBST(pkgconfigdir)

# Initialize libtool
LT_PREREQ([2.2])
LT_INIT()

AC_CONFIG_FILES([Makefile
		src/Makefile
		test/Makefile
		libm2d.pc
		docs/Doxyfile])
AC_OUTPUT
