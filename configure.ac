AC_INIT(src/torrent/torrent.cc)

LIBTORRENT_VERSION=0.4.5

AC_SUBST(LIBTORRENT_VERSION)

dnl Find a better way to do this
AC_DEFINE(PEER_NAME,
          "-lt0405-",
          Identifier that is part of the default peer id)

LIBTORRENT_CURRENT=2
LIBTORRENT_REVISION=0
LIBTORRENT_AGE=0

LIBTORRENT_INTERFACE_VERSION_INFO=$LIBTORRENT_CURRENT:$LIBTORRENT_REVISION:$LIBTORRENT_AGE
LIBTORRENT_INTERFACE_VERSION_NO=$LIBTORRENT_CURRENT.$LIBTORRENT_AGE.$LIBTORRENT_REVISION

AC_SUBST(LIBTORRENT_CURRENT)
AC_SUBST(LIBTORRENT_INTERFACE_VERSION_INFO)
AC_SUBST(LIBTORRENT_INTERFACE_VERSION_NO)

AM_INIT_AUTOMAKE("libtorrent", $LIBTORRENT_VERSION)
AM_CONFIG_HEADER(config.h)

sinclude(scripts/checks.m4)

AM_DISABLE_STATIC
AM_PROG_LIBTOOL

CXXFLAGS="-O3"

AC_PROG_CXX

AC_SYS_LARGEFILE

AC_ARG_ENABLE(debug,
    [  --enable-debug          enable debug information [default=yes]],
    [
        if test "$enableval" = "yes"; then
            CXXFLAGS="$CXXFLAGS -Wall -Werror -g -DDEBUG"
        else
            CXXFLAGS="$CXXFLAGS -Wall -DNDEBUG"
        fi
    ],[
        CXXFLAGS="$CXXFLAGS -Wall -Werror -g -DDEBUG"
    ]
)

PKG_CHECK_MODULES(STUFF, sigc++-2.0,
	          CXXFLAGS="$CXXFLAGS $STUFF_CFLAGS";
		  LIBS="$LIBS $STUFF_liBS")

TORRENT_CHECK_OPENSSL()
TORRENT_CHECK_CURL()
TORRENT_MINCORE_SIGNEDNESS()
TORRENT_OTFD()

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
	libtorrent.pc
	Makefile
	src/Makefile
	src/torrent/Makefile
	src/content/Makefile
	src/data/Makefile
	src/download/Makefile
	src/net/Makefile
	src/parse/Makefile
	src/peer/Makefile
	src/tracker/Makefile
])
