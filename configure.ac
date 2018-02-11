#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([poesie], [0.1], [mdorier@anl.gov])
AM_INIT_AUTOMAKE([1.13.4 -Wall -Werror foreign subdir-objects silent-rules])
AM_SILENT_RULES([yes])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_SRCDIR([include/poesie-common.h])
AC_CONFIG_HEADERS([src/poesie-config.h])
LT_PREREQ([2.2])
# LT_INIT needs to know about AR
AM_PROG_AR
LT_INIT([dlopen])

# Checks for programs.
AC_PROG_CC
# subdir objects needs a bit of help
AM_PROG_CC_C_O
AC_PROG_INSTALL

# Checks for libraries.
# Checks for header files.

# Checks for typedefs, structures, and compiler characteristics.
AC_C_INLINE
AC_TYPE_INT64_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_REALLOC

#pkg-config and libraries supporting that
PKG_PROG_PKG_CONFIG
if test "x$PKG_CONFIG" == "x"; then
   AC_MSG_ERROR([Could not find pkg-config utility!])
fi

# coreutils checks for OSX
AC_ARG_VAR([TIMEOUT], timeout program)
AC_ARG_VAR([MKTEMP], mktemp program)

if test -z "$TIMEOUT" ; then
    AC_CHECK_PROGS(TIMEOUT, [timeout gtimeout])
    if test -z "$TIMEOUT" ; then
        AC_MSG_ERROR([Could not find timeout command (can optionally provide via the TIMEOUT variable)])
    fi
else
    AC_SUBST([TIMEOUT], ["$TIMEOUT"])
fi

if test -z "$MKTEMP" ; then
    AC_CHECK_PROGS(MKTEMP, [mktemp gmktemp])
    if test -z "$MKTEMP" ; then
        AC_MSG_ERROR([Could not find mktemp command (can optionally provide via the MKTEMP variable)])
    fi
else
    AC_SUBST([MKTEMP], ["$MKTEMP"])
fi

PKG_CHECK_MODULES([MARGO],[margo],[],
      AC_MSG_ERROR([Could not find working margo installation!]) )
LIBS="$MARGO_LIBS $LIBS"
CPPFLAGS="$MARGO_CFLAGS $CPPFLAGS"
CFLAGS="$MARGO_CFLAGS $CFLAGS"

python_backend=yes
lua_backend=no

AC_ARG_ENABLE([python],
	      AS_HELP_STRING([--enable-python], [Enable "Python" as
			      language backend (default is yes)]),
	      [python_backend=${enableval}]
)
AC_ARG_ENABLE([lua],
	      AS_HELP_STRING([--enable-lua], [Enable "lua" as language
			      backend (default is no)]),
	      [lua_backend=${enableval}]
)

if test "x${python_backend}" == xyes ; then
	AC_CHECK_HEADERS([python2.7/Python.h], ,
			 AC_ERROR("Could not find Python headers"))
	AC_DEFINE([USE_PYTHON], 1, [Use Python language backend])
	SERVER_LIBS="${SERVER_LIBS} -lpython2.7"
fi

if test "x${lua_backend}" == xyes ; then
	AC_CHECK_HEADERS([lua.h], ,
			 AC_ERROR("Could not find Lua headers"))
	AC_DEFINE([USE_LUA], 1, [use Lua language backend])
	SERVER_LIBS="${SERVER_LIBS} -llua -ldl -lm"
	SERVER_DEPS="${SERVER_DEPS}" # no pkgconfig for Lua
fi

AM_CONDITIONAL([BUILD_PYTHON], [test "x${python_backend}" == xyes])
AM_CONDITIONAL([BUILD_LUA], [test "x${lua_backend}" == xyes])

AC_SUBST(SERVER_LIBS)
AC_SUBST(SERVER_DEPS)
AC_CONFIG_FILES([Makefile maint/poesie-client.pc maint/poesie-server.pc])
AC_OUTPUT

