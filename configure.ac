#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.67])
dnl Version is read from the VERSION file at the project root — single
dnl source of truth, also consumed by CMakeLists.txt and installed as a
dnl doc file.  m4_esyscmd_s strips the trailing newline.
AC_INIT([abcmidi], m4_esyscmd_s([cat VERSION]), [seymour.shlien@crc.ca])
AC_CONFIG_SRCDIR([abc.h])
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
cflags_save="$CFLAGS"
AC_PROG_CC
CFLAGS="$cflags_save" 

dnl This mechanism allows one to enable debug compilations...
AC_ARG_ENABLE(debug,
              [  --enable-debug          Enable debugging information],
              USE_DEBUG="$enableval", USE_DEBUG="no")
if test $USE_DEBUG = yes ; then   
  CFLAGS="$CFLAGS -g"
else
  CFLAGS="$CFLAGS -O2"
fi

AC_PROG_INSTALL

# Checks for libraries.
# FIXME: Replace `main' with a function in `-lm':
AC_CHECK_LIB([m], [main])

# Checks for header files.
AC_CHECK_HEADERS([stdlib.h string.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_SIZE_T

# Checks for library functions.
AC_FUNC_ERROR_AT_LINE
AC_FUNC_MALLOC
AC_CHECK_FUNCS([strcasecmp strchr])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
