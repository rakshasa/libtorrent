AC_INIT(torrent/torrent.cc)

LIBTORRENT_VERSION=0.3.7

AC_SUBST(LIBTORRENT_VERSION)

dnl Find a better way to do this
AC_DEFINE(PEER_NAME,
          "-lt0307-",
          Identifier that is part of the default peer id)

LIBTORRENT_CURRENT=0
LIBTORRENT_REVISION=1
LIBTORRENT_AGE=0

LIBTORRENT_VERSION_INFO=$LIBTORRENT_CURRENT:$LIBTORRENT_REVISION:$LIBTORRENT_AGE
LIBTORRENT_VERSION_NO=$LIBTORRENT_CURRENT.$LIBTORRENT_AGE.$LIBTORRENT_REVISION

AC_SUBST(LIBTORRENT_CURRENT)
AC_SUBST(LIBTORRENT_VERSION_INFO)
AC_SUBST(LIBTORRENT_VERSION_NO)

AM_INIT_AUTOMAKE("libtorrent", $LIBTORRENT_VERSION)
AM_CONFIG_HEADER(config.h)

sinclude(checks.m4)

AM_DISABLE_STATIC
AM_PROG_LIBTOOL

CXXFLAGS="-O3"

AC_PROG_CXX

AC_SYS_LARGEFILE

AC_ARG_ENABLE(debug,
    [  --enable-debug          enable debug information [default=yes]],
    [
        if test "$enableval" = "yes"; then
            CXXFLAGS="$CXXFLAGS -Wall -g -DDEBUG"
        else
            CXXFLAGS="$CXXFLAGS -Wall -DNDEBUG"
        fi
    ],[
        CXXFLAGS="$CXXFLAGS -Wall -g -DDEBUG"
    ]
)

PKG_CHECK_MODULES(STUFF, sigc++-2.0,
	          CXXFLAGS="$CXXFLAGS $STUFF_CFLAGS";
		  LIBS="$LIBS $STUFF_liBS")

TORRENT_CHECK_OPENSSL()

dnl ********************************************************
AC_CHECK_LIB(curl, curl_easy_init)
TORRENT_CHECK_CURL()

AC_LANG_PUSH(C++)

dnl **************************************************************
AC_MSG_CHECKING(for proper overloaded template function disambiguation)

AC_COMPILE_IFELSE(
    [[template <typename T> void f(T&) {}
      template <typename T> void f(T*) {}
      int main() { int *i = 0; f(*i); f(i); }
    ]],
    [
      AC_MSG_RESULT(yes)
    ], [
      AC_MSG_RESULT(no)
      AC_MSG_ERROR([your compiler does not properly handle overloaded template function disambiguation])
    ]
)

dnl *******************************************
AC_MSG_CHECKING(signedness of mincore parameter)

AC_COMPILE_IFELSE(
    [[#include <sys/types.h>
      #include <sys/mman.h>
      void f() { mincore((void*)0, 0, (unsigned char*)0); }
    ]],
    [
     AC_DEFINE(USE_MINCORE_UNSIGNED, 1, use unsigned char* in mincore)
     AC_MSG_RESULT(unsigned)
    ],
[
AC_COMPILE_IFELSE(
    [[#include <sys/types.h>
      #include <sys/mman.h>
      void f() { mincore((void*)0, 0, (char*)0); }
    ]],
    [
     AC_DEFINE(USE_MINCORE_UNSIGNED, 0, use char* in mincore)
     AC_MSG_RESULT(signed)
    ],
    [
     AC_MSG_ERROR([mincore signedness test failed])
    ])
])

AC_LANG_POP(C++)

AC_ARG_ENABLE(madvise,
    [  --enable-madvise        enable madvise optimization [default=no]],
    [
        if test "$enableval" = "yes"; then
            AC_DEFINE(USE_MADVISE_WILLNEED, 1, we can use madvise(... MADV_WILLNEED))
	    VAR_USE_MADVISE=1
        else
            AC_DEFINE(USE_MADVISE_WILLNEED, 0, we cannot use madvise(... MADV_WILLNEED))
	    VAR_USE_MADVISE=0
        fi
    ],[
        AC_DEFINE(USE_MADVISE_WILLNEED, 1, we can use madvise(... MADV_WILLNEED))
        VAR_USE_MADVISE=1
    ]
)

LIBTORRENT_LIBS="-ltorrent"
AC_SUBST(LIBTORRENT_LIBS)

LIBTORRENT_CFLAGS=""
AC_SUBST(LIBTORRENT_CFLAGS)

AC_DEFINE(HAVE_CONFIG_H, 1, true if config.h was included)

AC_OUTPUT([
	Makefile
	libtorrent.pc
	torrent/Makefile
])

AC_MSG_RESULT()

if test $VAR_USE_MADVISE = "0"; then
  AC_MSG_RESULT([  ( ) madvise(... MADV_WILLNEED) optimizing. (Known to kill linux 2.4.20)])
else
  AC_MSG_RESULT([  (X) madvise(... MADV_WILLNEED) optimizing. (Known to kill linux 2.4.20)])
fi

AC_MSG_RESULT()
