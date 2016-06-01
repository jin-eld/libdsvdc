#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.66])
AC_INIT([libdsvdc], [0.6.1], [jin@dev.digitalstrom.org])
AC_CONFIG_AUX_DIR(configure_aux)
AC_CONFIG_SRCDIR([src/dsvdc.c])
AC_CONFIG_HEADERS([config.h])
AM_INIT_AUTOMAKE([1.10 -Wall subdir-objects])
AC_CONFIG_MACRO_DIR([m4])

m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

dnl
dnl            current:revision:age
dnl
dnl current:  increment if interfaces have been added, removed or changed
dnl revision: increment if source code has changed, set to zero if current is
dnl           incremented
dnl age:      increment if interfaces have been added, set to zero if
dnl           interfaces have been removed or changed
LIBDSVDC_LT_VERSION=7:0:0
AC_SUBST(LIBDSVDC_LT_VERSION)


BUILD_TESTS="yes"
BUILD_DISCOVERY="yes"

DEPSEARCH=
AC_ARG_WITH(dependency-search,
    AC_HELP_STRING([--with-dependency-search=DIR],
                   [search for dependencies in DIR, i.e. look for libraries in
                    DIR/lib and for headers in DIR/include]),
    [
        DEPSEARCH="$withval"
    ]
)
if test -n "$DEPSEARCH"; then
    CFLAGS="$CFLAGS -I$DEPSEARCH/include"
    CPPFLAGS="$CPPFLAGS -I$DEPSEARCH/include"
    LDFLAGS="$LDFLAGS -L$DEPSEARCH/lib"
    export PKG_CONFIG_PATH=$PKG_CONFIG_PATH:$DEPSEARCH/lib/pkgconfig
    export PATH=$DEPSEARCH/bin:$PATH
fi

AC_ARG_ENABLE([debug],
    [AC_HELP_STRING([--enable-debug], [build debug version]) ],
    [
        if test "x$enableval" = "xyes"; then
            CFLAGS="$CFLAGS -g -O0 -Wall"
            AC_DEFINE([DEBUG], [1], [build with debug output])
        fi
    ]
)

AC_ARG_ENABLE([tests],
    [AC_HELP_STRING([--disable-tests], [build unit tests (default: auto)]) ],
    [
        if test "x$enableval" = "xno"; then
            BUILD_TESTS="no"
        elif test "x$enableval" = "xyes"; then
            BUILD_TESTS="yes"
        fi
    ]
)

AC_ARG_ENABLE([autodiscovery],
    [AC_HELP_STRING([--disable-autodiscovery],
                    [build with autodiscovery suopport (default: auto)]) ],
    [
        if test "x$enableval" = "xno"; then
            BUILD_DISCOVERY="no"
        elif test "x$enableval" = "xyes"; then
            BUILD_DISCOVERY="yes"
        fi
    ]
)

# Checks for programs.
AC_PROG_CC
AM_PROG_CC_C_O
m4_ifdef([AM_PROG_AR], [AM_PROG_AR])
AC_PROG_LIBTOOL

# Checks for libraries.

# Checks for header files.
AC_CHECK_HEADERS([arpa/inet.h limits.h netinet/in.h sys/socket.h stdlib.h stdint.h string.h unistd.h time.h ctype.h getopt.h],
        [], [AC_MSG_ERROR([required system header not found])])

AC_CHECK_HEADER([utlist.h], [],
        [AC_MSG_ERROR([required header utlist.h not found])])

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_TYPE_UINT8_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_INT32_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_CHECK_FUNCS([memset select socket strerror tolower getopt_long],[],
               [ AC_MSG_ERROR([required library function not found])] )

AX_PTHREAD([], [ AC_MSG_ERROR([required pthread library not found]) ])

PKG_PROG_PKG_CONFIG

if test -z "$PKG_CONFIG"; then
    AC_MSG_ERROR([pkg-config not found on your system])
fi

PKG_CHECK_MODULES([PROTOBUFC], [libprotobuf-c], [],
    [
        AC_CHECK_HEADERS([google/protobuf-c/protobuf-c.h], [],
            [
                AC_MSG_ERROR([coult not find protobuf-c headers])
            ])

        AC_CHECK_LIB([protobuf-c], [protobuf_c_message_unpack], [],
            [
                AC_MSG_ERROR([coult not find protobuf-c library])
            ])
    ])

AC_PATH_PROG(PROTOC, protoc-c, "")
if (test -z "$PROTOC"); then
    AC_MSG_ERROR([Can not compile protobuf messages, protoc-c not found])
fi
AC_SUBST(PROTOC)

if test "x$BUILD_TESTS" = "xyes"; then
    PKG_CHECK_MODULES([CHECK], [check],
        [
            LIBCHECK_FOUND="yes"
        ],
        [
            AC_MSG_WARN([libcheck not found, not building unit tests: $CHECK_PKG_ERRORS])
            BUILD_TESTS="no"
        ])
fi

if (test "x$BUILD_DISCOVERY" = "xyes"); then
    PKG_CHECK_MODULES([AVAHI], [avahi-client],
        [
            AC_DEFINE([HAVE_AVAHI], [1], [avahi support available])
        ],
        [
            AC_MSG_WARN([$AVAHI_PKG_ERRORS])
            BUILD_DISCOVERY="no"
        ])
fi

dnl libdsuid
PKG_CHECK_MODULES([LIBDSUID], [libdsuid],[],
    [
        AC_MSG_ERROR([required library libdsuid not found])
    ])

AC_SUBST(LIBDSUID_CFLAGS)
AC_SUBST(LIBDSUID_LIBS)


dnl libgdbm
AC_CHECK_HEADER([gdbm.h], [],
    [
        AC_MSG_ERROR([could not find gdbm.h header])
    ])

AC_CHECK_LIB([gdbm], [gdbm_close], [],
    [
        AC_MSG_ERROR([coult not find gdbm library])
    ])

AM_CONDITIONAL(BUILD_TESTS, test "x$BUILD_TESTS" = "xyes")
AM_CONDITIONAL(BUILD_DISCOVERY, test "x$BUILD_DISCOVERY" = "xyes")

DX_DOT_FEATURE(OFF)
DX_HTML_FEATURE(ON)
DX_CHM_FEATURE(OFF)
DX_CHI_FEATURE(OFF)
DX_MAN_FEATURE(OFF)
DX_RTF_FEATURE(OFF)
DX_XML_FEATURE(OFF)
DX_PDF_FEATURE(OFF)
DX_PS_FEATURE(OFF)
DX_INIT_DOXYGEN($PACKAGE_NAME, doxygen.conf, [doc])
DX_DOXYGEN_FEATURE(ON)

CFLAGS="$CFLAGS -Wall -Wextra"

AC_CONFIG_FILES([Makefile
                 messages/Makefile
                 src/Makefile
                 tests/Makefile
                 examples/Makefile
                 libdsvdc.pc
                 ])
AC_OUTPUT
