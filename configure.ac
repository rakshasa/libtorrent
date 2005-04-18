AC_INIT(libtorrent, 0.6.1, jaris@ifi.uio.no)

dnl Find a better way to do this
AC_DEFINE(PEER_NAME,
          "-lt0601-",
          Identifier that is part of the default peer id)

LIBTORRENT_CURRENT=2
LIBTORRENT_REVISION=0
LIBTORRENT_AGE=0

LIBTORRENT_INTERFACE_VERSION_INFO=$LIBTORRENT_CURRENT:$LIBTORRENT_REVISION:$LIBTORRENT_AGE
LIBTORRENT_INTERFACE_VERSION_NO=$LIBTORRENT_CURRENT.$LIBTORRENT_AGE.$LIBTORRENT_REVISION

AC_SUBST(LIBTORRENT_CURRENT)
AC_SUBST(LIBTORRENT_INTERFACE_VERSION_INFO)
AC_SUBST(LIBTORRENT_INTERFACE_VERSION_NO)

AM_INIT_AUTOMAKE
AM_CONFIG_HEADER(config.h)

sinclude(scripts/checks.m4)

AM_DISABLE_STATIC
AM_PROG_LIBTOOL

AC_ARG_ENABLE(debug,
    [  --enable-debug          enable debug information [default=yes]],
    [
        if test "$enableval" = "yes"; then
            CXXFLAGS="$CXXFLAGS -O3 -Wall -g -DDEBUG"
        else
            CXXFLAGS="$CXXFLAGS -O3 -Wall -DNDEBUG"
        fi
    ],[
        CXXFLAGS="$CXXFLAGS -O3 -Wall -g -DDEBUG"
    ]
)

AC_ARG_ENABLE(werror,
    [  --enable-werror         enable the -Werror flag [default=no]],
    [
        if test "$enableval" = "yes"; then
            CXXFLAGS="$CXXFLAGS -Werror"
        fi
    ]
)

AC_PROG_CXX
AC_SYS_LARGEFILE

AC_C_BIGENDIAN(
    AC_DEFINE(IS_BIG_ENDIAN, 1, Big endian),
    AC_DEFINE(IS_LITTLE_ENDIAN, 1, Little endian),
    AC_MSG_ERROR([Could not determine endianness])
)

PKG_CHECK_MODULES(STUFF, sigc++-2.0,
	          CXXFLAGS="$CXXFLAGS $STUFF_CFLAGS";
		  LIBS="$LIBS $STUFF_LIBS")

dnl TORRENT_CHECK_OPENSSL()
TORRENT_MINCORE_SIGNEDNESS()
TORRENT_OTFD()

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
	src/utils/Makefile
])
