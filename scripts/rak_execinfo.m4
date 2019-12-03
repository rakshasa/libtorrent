AC_DEFUN([RAK_DISABLE_BACKTRACE], [
  AC_ARG_ENABLE(backtrace,
    AC_HELP_STRING([--disable-backtrace], [disable backtrace information [[default=no]]]),
    [
        if test "$enableval" = "yes"; then
            AX_EXECINFO
        fi
    ],[
        AX_EXECINFO
    ])
])
