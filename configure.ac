AC_INIT([swordfish], [0.12])
AC_CONFIG_HEADER(include/config.h:include/config.h.in)
AM_INIT_AUTOMAKE([1.8])

CFLAGS="$CFLAGS \
-Wall -Wextra -Wstrict-prototypes -Wshadow \
-Wbad-function-cast -Wcast-align -Wwrite-strings \
-Waggregate-return -Wno-unused-parameter \
-Wmissing-format-attribute -Wnested-externs \
-fno-common -Wchar-subscripts -Wcomment \
-Wimplicit -Wsequence-point -Wreturn-type \
-Wno-system-headers -Wformat \
"

dnl ------------------------------------------------------
dnl libevent detection.  swiped from memcached.

LIBEVENT_URL=http://www.monkey.org/~provos/libevent/

AC_CACHE_CHECK([for libevent directory], ac_cv_libevent_dir, [
  saved_LIBS="$LIBS"
  saved_LDFLAGS="$LDFLAGS"
  le_found=no
  for ledir in $trylibeventdir "" $prefix /usr/local ; do
    LDFLAGS="$saved_LDFLAGS"
    LIBS="$saved_LIBS -levent"

    # Skip the directory if it isn't there.
    if test ! -z "$ledir" -a ! -d "$ledir" ; then
       continue;
    fi
    if test ! -z "$ledir" ; then
      if test -d "$ledir/lib" ; then
        LDFLAGS="-L$ledir/lib $LDFLAGS"
      else
        LDFLAGS="-L$ledir $LDFLAGS"
      fi
    fi
    # Can I compile and link it?
    AC_TRY_LINK([#include <sys/time.h>
#include <sys/types.h>
#include <event.h>], [ event_init(); ],
       [ libevent_linked=yes ], [ libevent_linked=no ])
    if test $libevent_linked = yes; then
       if test ! -z "$ledir" ; then
         ac_cv_libevent_dir=$ledir
       else
         ac_cv_libevent_dir="(system)"
       fi
       le_found=yes
       break
    fi
  done
  LIBS="$saved_LIBS"
  LDFLAGS="$saved_LDFLAGS"
  if test $le_found = no ; then
    AC_MSG_ERROR([libevent is required.  You can get it from $LIBEVENT_URL

      If it's already installed, specify its path using --with-libevent=/dir/
])
  fi
])
LIBS="$LIBS -levent"
if test $ac_cv_libevent_dir != "(system)"; then
  if test -d "$ac_cv_libevent_dir/lib" ; then
    LDFLAGS="-L$ac_cv_libevent_dir/lib $LDFLAGS"
    le_libdir="$ac_cv_libevent_dir/lib"
  else
    LDFLAGS="-L$ac_cv_libevent_dir $LDFLAGS"
    le_libdir="$ac_cv_libevent_dir"
  fi
fi

PKG_CHECK_MODULES(TOKYOCABINET, tokyocabinet)

AC_PROG_MAKE_SET
AC_PROG_CC

CFLAGS="$CFLAGS -Werror" 

AC_MSG_NOTICE([Using CFLAGS="$CFLAGS"])
AC_MSG_NOTICE([Using LIBS="$LIBS"])
AC_OUTPUT(Makefile src/Makefile man/Makefile)
