AC_INIT(torrent/torrent.cc)

LIBTORRENT_VERSION=0.3.2

AC_SUBST(LIBTORRENT_VERSION)

dnl Find a better way to do this
AC_DEFINE(PEER_NAME,
          "-lt0302-",
          Identifier that is part of the default peer id)

LIBTORRENT_CURRENT=0
LIBTORRENT_REVISION=1
LIBTORRENT_AGE=0

LIBTORRENT_VERSION_INFO=$LIBTORRENT_CURRENT:$LIBTORRENT_REVISION:$LIBTORRENT_AGE
LIBTORRENT_VERSION_NO=$LIBTORRENT_CURRENT.$LIBTORRENT_AGE.$LIBTORRENT_REVISION

AC_SUBST(LIBTORRENT_CURRENT)
AC_SUBST(ATLAS_VERSION_INFO)
AC_SUBST(ATLAS_VERSION_NO)

AM_INIT_AUTOMAKE("libtorrent", $LIBTORRENT_VERSION)
AM_CONFIG_HEADER(config.h)

AM_DISABLE_STATIC
AM_PROG_LIBTOOL

CXXFLAGS="-O3 -g"

AC_PROG_CXX

AC_SYS_LARGEFILE

AC_ARG_ENABLE(debug,
    [  --enable-debug          enable debug information [default=yes]],
    [
        if test "$enableval" = "yes"; then
            CXXFLAGS="$CXXFLAGS -Wall -DDEBUG"
        else
            CXXFLAGS="$CXXFLAGS -Wall -DNDEBUG"
        fi
    ],[
        CXXFLAGS="$CXXFLAGS -Wall -DDEBUG"
    ]
)

PKG_CHECK_MODULES(OPENSSL, openssl,
		  CXXFLAGS="$CXXFLAGS `pkg-config --cflags openssl`";
                  LIBS="$LIBS -lcrypto `pkg-config --libs-only-L openssl`",
		  AC_MSG_ERROR(Could not find openssl's crypto library))

PKG_CHECK_MODULES(STUFF, sigc++-2.0,
	          CXXFLAGS="$CXXFLAGS $STUFF_CFLAGS";
		  LIBS="$LIBS $STUFF_liBS")

AC_CHECK_LIB(curl, curl_easy_init)

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
