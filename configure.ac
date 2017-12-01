#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_INIT([OpenSync], [0.0.1.1], [neol_d2022@outlook.com])
AM_INIT_AUTOMAKE
AC_CONFIG_SRCDIR([childthreads.h])
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL

# Checks for libraries.
AC_CHECK_LIB([pthread], [pthread_rwlock_init])
AC_CHECK_LIB([ws2_32], [main])

# Checks for header files.
AC_CHECK_HEADERS([stddef.h stdint.h stdlib.h string.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_TYPE_SIZE_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_TYPE_UINT8_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_CHECK_FUNCS([memset strchr mkdir])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
