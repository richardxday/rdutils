AC_PREREQ([2.68])
AC_INIT([RDUTILS],[0.1.0.0])

AS_NANO(RDUTILS_CVS=no,RDUTILS_CVS=yes)

AM_INIT_AUTOMAKE([1.11 silent-rules])
AM_SILENT_RULES([yes])

# Version number that will be appended to the library filename
RDUTILS_MAJORMINOR=0.1
AC_SUBST(RDUTILS_MAJORMINOR)

AC_CONFIG_HEADERS([config.h])

# CURRENT, REVISION, AGE
# - library source changed but interface the same -> increment REVISION
# - interfaces added/removed/changed -> increment CURRENT, set REVISION = 0
# - interfaces added -> increment AGE also
# - interfaces removed -> set AGE = 0
#    (AGE is the number of previous values of CURRENT that are compatible)

LT_PREREQ([2.2.6])
LT_INIT(disable-static win32-dll)

AC_PROG_CXX
AM_PROG_CC_C_O

AC_CONFIG_SRCDIR([src])

AC_CONFIG_MACRO_DIR([m4])
ACLOCAL_AMFLAGS="-I m4 $ACLOCAL_AMFLAGS"
AC_SUBST(ACLOCAL_AMFLAGS)

DX_DOXYGEN_FEATURE(ON)
DX_INIT_DOXYGEN(rdutils, doxygen.cfg)

# Check if we should enable GPL code  (./configure --enable-gpl)
AC_MSG_CHECKING(whether to include code under gpl)
AC_ARG_ENABLE(gpl, AS_HELP_STRING([--disable-gpl], [disable code under GPL]), ENABLE_GPL="no", ENABLE_GPL="yes")
AM_CONDITIONAL(ENABLE_GPL, test "x${ENABLE_GPL}" = "xyes")
if test "x${ENABLE_GPL}" = "xyes"; then
  AC_MSG_RESULT(yes)
else
  AC_MSG_RESULT(no)
fi

# Check if we should disable optimization  (./configure --disable-opt)
AC_MSG_CHECKING(whether to disable optimization)
AC_ARG_ENABLE(opt, AS_HELP_STRING([--disable-opt], [disable optimzation]), DISABLE_OPTIMIZATION="yes", DISABLE_OPTIMIZATION="no")
if test "x${DISABLE_OPTIMIZATION}" = "xyes"; then
  CFLAGS="$CFLAGS -O0"
  CXXFLAGS="$CXXFLAGS -O0"
  AC_MSG_RESULT(yes)
else
  AC_MSG_RESULT(no)
fi
AC_SUBST(CFLAGS)
AC_SUBST(CXXFLAGS)

dnl rdlib is required
RDLIB_VER="0.1.0.0"
PKG_CHECK_MODULES(RDLIB, rdlib-0.1 >= $RDLIB_VER, HAVE_RDLIB=yes, HAVE_RDLIB=no)
if test "x${HAVE_RDLIB}" != xyes ; then
  AC_MSG_ERROR([rdlib-0.1 >= $RDLIB_VER is required])
fi

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_SIZE_T

AC_APPEND_SUPPORTED_CFLAGS(RDUTILS_CFLAGS, [-Wall])
AC_APPEND_SUPPORTED_CFLAGS(RDUTILS_CFLAGS, [-Werror])
AC_APPEND_SUPPORTED_CFLAGS(RDUTILS_CFLAGS, [-Wextra])
AC_APPEND_SUPPORTED_CFLAGS(RDUTILS_CFLAGS, [-msse2])
AC_APPEND_SUPPORTED_CFLAGS(RDUTILS_CFLAGS, [-fsigned-char])

RDUTILS_LDFLAGS=""
AC_APPEND_SUPPORTED_LDFLAGS(RDUTILS_LDFLAGS, [-Wl,--no-undefined])

AC_SUBST(RDUTILS_CFLAGS)
AC_SUBST(RDUTILS_LDFLAGS)

AC_SUBST(RDUTILS_PKG_DEPS)

AC_CONFIG_FILES([
Makefile
src/Makefile
])
AC_OUTPUT
