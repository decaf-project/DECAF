#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.59)
AC_INIT(llconf, 0.4.6, <oku@debian.org>)
AC_CONFIG_SRCDIR([src/entry.c])
AC_CONFIG_HEADER([config.h])
AM_INIT_AUTOMAKE()
AC_PROG_LIBTOOL()

# Checks for programs.
AC_PROG_CC

# Checks for libraries.

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([stdlib.h string.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST

# Checks for library functions.
AC_FUNC_MALLOC
AC_CHECK_FUNCS([memset strdup strerror])
AC_CONFIG_FILES([Makefile llconf.pc src/Makefile src/parsers/Makefile examples/Makefile doc/Makefile debian/Makefile])
AC_OUTPUT 
