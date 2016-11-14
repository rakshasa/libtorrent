dnl function for enabling/disabling udns support
AC_DEFUN([TORRENT_WITH_UDNS], [
  AC_ARG_WITH(
    [udns],
    AS_HELP_STRING([--without-udns], [Don't use udns, falling back to synchronous DNS resolution.])
  )
dnl neither ubuntu nor fedora ships a pkgconfig file for udns
  AS_IF(
    [test "x$with_udns"  != "xno"],
    [AC_CHECK_HEADERS([udns.h], [have_udns=yes], [have_udns=no])],
    [have_udns=no]
  )
  AS_IF(
    [test "x$have_udns" = "xyes"],
    [
      AC_DEFINE(USE_UDNS, 1, Define to build with udns support.)
      LIBS="$LIBS -ludns"
    ],
    [
      AS_IF(
        [test "x$with_udns" = "xyes"],
        [AC_MSG_ERROR([udns requested but not found])]
      )
    ]
  )
])
