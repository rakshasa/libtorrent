AC_INIT(torrent/torrent.cc)

LIBTORRENT_VERSION=0.3.4

AC_SUBST(LIBTORRENT_VERSION)

dnl Find a better way to do this
AC_DEFINE(PEER_NAME,
          "-lt0304-",
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

PKG_CHECK_MODULES(OPENSSL, openssl,
		  CXXFLAGS="$CXXFLAGS `pkg-config --cflags openssl`";
                  LIBS="$LIBS -lcrypto `pkg-config --libs-only-L openssl`",
		  AC_MSG_ERROR(Could not find openssl's crypto library))

PKG_CHECK_MODULES(STUFF, sigc++-2.0,
	          CXXFLAGS="$CXXFLAGS $STUFF_CFLAGS";
		  LIBS="$LIBS $STUFF_liBS")

AC_CHECK_LIB(curl, curl_easy_init)

AC_DEFUN(MY_CURL,[
 AC_CACHE_VAL(my_cv_curl_vers,[
 my_cv_curl_vers=NONE
 dnl check is the plain-text version of the required version
 check="7.12.0"
 dnl check_hex must be UPPERCASE if any hex letters are present
 check_hex="070C00"
 
 AC_MSG_CHECKING([for curl >= $check])
 
 if eval curl-config --version 2>/dev/null >/dev/null; then
   ver=`curl-config --version | sed -e "s/libcurl //g"`
   hex_ver=`curl-config --vernum | tr 'a-f' 'A-F'`
   ok=`echo "ibase=16; if($hex_ver>=$check_hex) $hex_ver else 0" | bc`
 
   if test x$ok != x0; then
     my_cv_curl_vers="$ver"
     AC_MSG_RESULT([$my_cv_curl_vers])
   else
     AC_MSG_RESULT(FAILED)
     AC_MSG_ERROR([$ver is too old. Need version $check or higher.])
   fi
 else
   AC_MSG_RESULT(FAILED)
   AC_MSG_ERROR([curl-config was not found])
 fi
 ])
])

MY_CURL()

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
