AC_DEFUN([TORRENT_WITH_SYSROOT], [
  AC_ARG_WITH(sysroot,
    AC_HELP_STRING([--with-sysroot=PATH], [compile and link with a specific sysroot]),
    [
      AC_MSG_CHECKING(for sysroot)

      if test "$withval" = "no"; then
        AC_MSG_RESULT(no)

      elif test "$withval" = "yes"; then
        AC_MSG_RESULT(not a path)
        AC_MSG_ERROR(The sysroot option must point to a directory, like f.ex "/Developer/SDKs/MacOSX10.4u.sdk".)
      else
        AC_MSG_RESULT($withval)

        CXXFLAGS="$CXXFLAGS -isysroot $withval"
        LDFLAGS="$LDFLAGS -Wl,-syslibroot,$withval"
      fi
    ])
])


AC_DEFUN([TORRENT_ENABLE_ARCH], [
  AC_ARG_ENABLE(arch,
    AC_HELP_STRING([--enable-arch=ARCH], [comma seprated list of architectures to compile for]),
    [
      AC_MSG_CHECKING(for target architectures)

      if test "$enableval" = "yes"; then
        AC_MSG_ERROR(no arch supplied)

      elif test "$enableval" = "no"; then
        AC_MSG_RESULT(using default)

      else
        AC_MSG_RESULT($enableval)

        for i in `IFS=,; echo $enableval`; do
          CFLAGS="$CFLAGS -march=$i"
          CXXFLAGS="$CXXFLAGS -march=$i"
          LDFLAGS="$LDFLAGS -march=$i"
        done
      fi
    ])
])


AC_DEFUN([TORRENT_MINCORE_SIGNEDNESS], [
  AC_LANG_PUSH(C++)
  AC_MSG_CHECKING(signedness of mincore parameter)

  AC_COMPILE_IFELSE([AC_LANG_SOURCE([
      #include <sys/types.h>
      #include <sys/mman.h>
      #include <unistd.h>
      void f() { mincore((char*)0, 0, (unsigned char*)0); }
      ])],
    [
      AC_DEFINE(USE_MINCORE, 1, Use mincore)
      AC_DEFINE(USE_MINCORE_UNSIGNED, 1, use unsigned char* in mincore)
      AC_MSG_RESULT(unsigned)
    ],
    [
      AC_COMPILE_IFELSE([AC_LANG_SOURCE([
          #include <sys/types.h>
          #include <sys/mman.h>
          #include <unistd.h>
          void f() { mincore((char*)0, 0, (char*)0); }
          ])],
        [
          AC_DEFINE(USE_MINCORE, 1, Use mincore)
          AC_DEFINE(USE_MINCORE_UNSIGNED, 0, use char* in mincore)
          AC_MSG_RESULT(signed)
        ],
        [
          AC_MSG_ERROR([failed, do *not* attempt fix this with --disable-mincore unless you are running Win32.])
      ])
  ])

  AC_LANG_POP(C++)
])

AC_DEFUN([TORRENT_MINCORE], [
  AC_ARG_ENABLE(mincore,
    AC_HELP_STRING([--disable-mincore], [disable mincore check [[default=enable]]]),
    [
      if test "$enableval" = "yes"; then
        TORRENT_MINCORE_SIGNEDNESS()
      else
	AC_MSG_CHECKING(for mincore)
	AC_MSG_RESULT(disabled)
      fi
    ],[
        TORRENT_MINCORE_SIGNEDNESS()
    ])
])

AC_DEFUN([TORRENT_CHECK_MADVISE], [
  AC_MSG_CHECKING(for madvise)

  AC_COMPILE_IFELSE([AC_LANG_SOURCE([
      #include <sys/types.h>
      #include <sys/mman.h>
        void f() { static char test@<:@1024@:>@; madvise((void *)test, sizeof(test), MADV_NORMAL); }
      ])],
    [
      AC_MSG_RESULT(yes)
      AC_DEFINE(USE_MADVISE, 1, Use madvise)
    ], [
      AC_MSG_RESULT(no)
  ])
])

AC_DEFUN([TORRENT_CHECK_POPCOUNT], [
  AC_MSG_CHECKING(for __builtin_popcount)

  AC_COMPILE_IFELSE([AC_LANG_SOURCE([
      int f() { return __builtin_popcount(0); }
    ])],
    [
      AC_MSG_RESULT(yes)
      AC_DEFINE(USE_BUILTIN_POPCOUNT, 1, Use __builtin_popcount.)
    ], [
      AC_MSG_RESULT(no)
  ])
])

AC_DEFUN([TORRENT_CHECK_EXECINFO], [
  AC_MSG_CHECKING(for execinfo.h)

  AC_RUN_IFELSE([AC_LANG_SOURCE([
      #include <execinfo.h>
      int main() { backtrace((void**)0, 0); backtrace_symbols((char**)0, 0); return 0;}
      ])],
    [
      AC_MSG_RESULT(yes)
      AC_DEFINE(USE_EXECINFO, 1, Use execinfo.h)
    ], [
      AC_MSG_RESULT(no)
  ])
])

AC_DEFUN([TORRENT_CHECK_ALIGNED], [
  AC_MSG_CHECKING(the byte alignment)

  AC_RUN_IFELSE([AC_LANG_SOURCE([
      #include <inttypes.h>
      int main() {
        char buf@<:@8@:>@ = { 0, 0, 0, 0, 1, 0, 0, 0 };
	int i;
        for (i = 1; i < 4; ++i)
	  if (*(uint32_t*)(buf + i) == 0) return -1;
	return 0;
	}
      ])],
    [
      AC_MSG_RESULT(none needed)
    ], [
      AC_DEFINE(USE_ALIGNED, 1, Require byte alignment)
      AC_MSG_RESULT(required)
  ])
])


AC_DEFUN([TORRENT_ENABLE_ALIGNED], [
  AC_ARG_ENABLE(aligned,
    AC_HELP_STRING([--enable-aligned], [enable alignment safe code [[default=check]]]),
    [
        if test "$enableval" = "yes"; then
          AC_DEFINE(USE_ALIGNED, 1, Require byte alignment)
        fi
    ],[
        TORRENT_CHECK_ALIGNED
  ])
])


AC_DEFUN([TORRENT_DISABLE_INSTRUMENTATION], [
  AC_MSG_CHECKING([if instrumentation should be included])

  AC_ARG_ENABLE(instrumentation,
    AC_HELP_STRING([--disable-instrumentation], [disable instrumentation [[default=enabled]]]),
    [
      if test "$enableval" = "yes"; then
        AC_DEFINE(LT_INSTRUMENTATION, 1, enable instrumentation)
	AC_MSG_RESULT(yes)
      else
	AC_MSG_RESULT(no)
      fi
    ],[
      AC_DEFINE(LT_INSTRUMENTATION, 1, enable instrumentation)
      AC_MSG_RESULT(yes)
    ])
])


AC_DEFUN([TORRENT_ENABLE_INTERRUPT_SOCKET], [
  AC_ARG_ENABLE(interrupt-socket,
    AC_HELP_STRING([--enable-interrupt-socket], [enable interrupt socket [[default=no]]]),
    [
      if test "$enableval" = "yes"; then
        AC_DEFINE(USE_INTERRUPT_SOCKET, 1, Use interrupt socket instead of pthread_kill)
      fi
    ]
  )
])
