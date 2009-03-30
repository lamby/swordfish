AC_INIT([swordfish], [0.1])
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

AC_PROG_MAKE_SET
AC_PROG_CC


AC_MSG_NOTICE([Using CFLAGS="$CFLAGS"])
AC_MSG_NOTICE([Using LIBS="$LIBS"])
AC_OUTPUT(Makefile src/Makefile)
