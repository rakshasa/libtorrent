AC_DEFUN([RAK_CHECK_CFLAGS], [

  AC_MSG_CHECKING([for user-defined CFLAGS])

  if test "$CFLAGS" = ""; then
    unset CFLAGS
    AC_MSG_RESULT([undefined])
  else
    AC_MSG_RESULT([user-defined "$CFLAGS"])
  fi
])


AC_DEFUN([RAK_CHECK_CXXFLAGS], [

  AC_MSG_CHECKING([for user-defined CXXFLAGS])

  if test "$CXXFLAGS" = ""; then
    unset CXXFLAGS
    AC_MSG_RESULT([undefined])
  else
    AC_MSG_RESULT([user-defined "$CXXFLAGS"])
  fi
])


AC_DEFUN([RAK_ENABLE_DEBUG], [
  AC_ARG_ENABLE(debug,
    AS_HELP_STRING([--enable-debug],[enable debug information [[default=yes]]]),
    [
        if test "$enableval" = "yes"; then
            CXXFLAGS="$CXXFLAGS -g -DDEBUG"
        else
            CXXFLAGS="$CXXFLAGS -DNDEBUG"
        fi
    ],[
        CXXFLAGS="$CXXFLAGS -g -DDEBUG"
    ])
])


AC_DEFUN([RAK_ENABLE_WERROR], [
  AC_ARG_ENABLE(werror,
    AS_HELP_STRING([--enable-werror],[enable the -Werror and -Wall flags [[default -Wall only]]]),
    [
        if test "$enableval" = "yes"; then
           CXXFLAGS="$CXXFLAGS -Werror -Wall"
        fi
    ],[
        CXXFLAGS="$CXXFLAGS -Wall"
    ])
])


AC_DEFUN([RAK_ENABLE_EXTRA_DEBUG], [
  AC_ARG_ENABLE(extra-debug,
    AS_HELP_STRING([--enable-extra-debug],[enable extra debugging checks [[default=no]]]),
    [
        if test "$enableval" = "yes"; then
            AC_DEFINE(USE_EXTRA_DEBUG, 1, Enable extra debugging checks.)
        fi
    ])
])
