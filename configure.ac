AC_INIT(torrent/torrent.cc)

LIBTORRENT_VERSION=0.3.3

AC_SUBST(LIBTORRENT_VERSION)

dnl Find a better way to do this
AC_DEFINE(PEER_NAME,
          "-lt0303-",
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


dnl AX_KERNEL_VERSION(major, minor, level, comparison, action-if-true, action-if-false)
AC_DEFUN(AX_KERNEL_VERSION, [
SAVE_CFLAGS=$CFLAGS
CFLAGS="-I$KINC -D__KERNEL__ -Werror"
AC_TRY_COMPILE(
  [
  #include <linux/version.h>
  #include <linux/config.h>
  ],
  [
  #if LINUX_VERSION_CODE $4 KERNEL_VERSION($1, $2, $3)
  break_me_hard(\\\);
  #endif
  ],
[$5],[$6],)
CFLAGS=$SAVE_CFLAGS
])


PKG_CHECK_MODULES(OPENSSL, openssl,
		  CXXFLAGS="$CXXFLAGS `pkg-config --cflags openssl`";
                  LIBS="$LIBS -lcrypto `pkg-config --libs-only-L openssl`",
		  AC_MSG_ERROR(Could not find openssl's crypto library))

PKG_CHECK_MODULES(STUFF, sigc++-2.0,
	          CXXFLAGS="$CXXFLAGS $STUFF_CFLAGS";
		  LIBS="$LIBS $STUFF_liBS")

AC_CHECK_LIB(curl, curl_easy_init)

AC_MSG_CHECKING([for linux 2.4.x])
AX_KERNEL_VERSION(2, 4, 99, <,
                  AC_MSG_RESULT([disable madvise see README]); AC_DEFINE(USE_MADVISE_WILLNEED, 0, Disable due to a kernel bug),
                  AC_MSG_RESULT([enable madvise]); AC_DEFINE(USE_MADVISE_WILLNEED, 1, Disable this if you wish to run on 2.4.x))


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
