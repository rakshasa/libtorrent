AC_INIT([[libtorrent]],[[0.16.18]],[[sundell.software@gmail.com]])

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIRS([scripts])
AM_INIT_AUTOMAKE([serial-tests subdir-objects foreign])

LT_INIT([[disable-static]])

# When releasing the first 1.x.y version, we need to start with 1.1.0 (or higher) as we've already
# used 0.16.x.
AC_DEFINE([[PEER_NAME]], [["-lt1012-"]], [[Identifier that is part of the default peer id.]])
AC_DEFINE([[PEER_VERSION]], [["lt\x10\x12"]], [[4 byte client and version identifier for DHT.]])

LIBTORRENT_CURRENT=48
LIBTORRENT_REVISION=0
LIBTORRENT_AGE=0

LIBTORRENT_INTERFACE_VERSION_INFO=$LIBTORRENT_CURRENT:$LIBTORRENT_REVISION:$LIBTORRENT_AGE
LIBTORRENT_INTERFACE_VERSION_NO=$LIBTORRENT_CURRENT.$LIBTORRENT_AGE.$LIBTORRENT_REVISION

AC_SUBST(LIBTORRENT_CURRENT)
AC_SUBST(LIBTORRENT_INTERFACE_VERSION_INFO)
AC_SUBST(LIBTORRENT_INTERFACE_VERSION_NO)

AC_PROG_CXX
AX_CXX_COMPILE_STDCXX(20, noext, mandatory)

RAK_CHECK_CFLAGS
RAK_CHECK_CXXFLAGS
RAK_ENABLE_DEBUG
RAK_ENABLE_EXTRA_DEBUG
RAK_ENABLE_WERROR

TORRENT_CHECK_ATOMIC
TORRENT_CHECK_CACHELINE
TORRENT_CHECK_CUSTOM_ENDIAN64
TORRENT_CHECK_MADVISE
TORRENT_CHECK_POSIX_FADVISE

TORRENT_WITHOUT_KQUEUE
TORRENT_WITHOUT_EPOLL
TORRENT_WITH_ADDRESS_SPACE
TORRENT_WITH_INOTIFY

AC_ARG_ENABLE(attribute-visibility,
  AS_HELP_STRING([--disable-attribute-visibility],[disable symbol visibility attribute [[default=enable]]]),
  [
    if test "$enableval" = "yes"; then
      CC_ATTRIBUTE_VISIBILITY
    fi
  ],[
    CC_ATTRIBUTE_VISIBILITY
  ])

AC_ARG_ENABLE(execinfo,
  AS_HELP_STRING([--disable-execinfo],[disable libexecinfo [[default=enable]]]),
  [
    if test "$enableval" = "yes"; then
      AX_EXECINFO
    fi
  ],[
    AX_EXECINFO
  ])

if test "x$use_kqueue" != "xyes" && test "x$use_epoll" != "xyes"; then
  AC_MSG_ERROR([Must enable at least one of either kqueue or epoll])
fi

AX_PTHREAD
AC_CHECK_FUNCS([fallocate posix_fallocate accept4 pipe2 epoll_create1 kqueue1 inotify_init1])

TORRENT_ENABLE_CUSTOM_STACK_SIZE

PKG_CHECK_MODULES([LIBCURL], [libcurl],, [LIBCURL_CHECK_CONFIG])
PKG_CHECK_MODULES([CPPUNIT], [cppunit],, [no_cppunit="yes"])
PKG_CHECK_MODULES([ZLIB], [zlib])

CFLAGS="$LIBCURL_CFLAGS $PTHREAD_CFLAGS $ZLIB_CFLAGS $CFLAGS"
CXXFLAGS="$LIBCURL_CFLAGS $LIBCURL_CPPFLAGS $PTHREAD_CFLAGS $ZLIB_CFLAGS $CXXFLAGS"
LIBS="$LIBCURL_LIBS $ATOMIC_LIBS $PTHREAD_LIBS $ZLIB_LIBS $LIBS"

TORRENT_CHECK_OPENSSL

TORRENT_DISABLE_PTHREAD_SETNAME_NP
TORRENT_MINCORE

TORRENT_ENABLE_INSTRUMENTATION

LIBTORRENT_LIBS="-ltorrent"
LIBTORRENT_CFLAGS=""

AC_SUBST(LIBTORRENT_LIBS)
AC_SUBST(LIBTORRENT_CFLAGS)

AC_DEFINE(HAVE_CONFIG_H, 1, true if config.h was included)

AC_CONFIG_FILES([
    libtorrent.pc
    Makefile
    src/Makefile
    src/torrent/Makefile
    test/Makefile
])

AC_OUTPUT
