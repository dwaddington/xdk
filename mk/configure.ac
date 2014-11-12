AC_INIT([SILICA],[0.2])

AC_CHECK_LIB([numa],[numa_available],[],AC_MSG_ERROR([libnuma-dev not installed]))
AC_CHECK_LIB([tinyxml],[tolower],[],AC_MSG_ERROR([libtinyxml-dev not installed]))

# this should work but it doesn't?
#
#AC_LANG(C++)
#SAVED_LDFLAGS=$LDFLAGS
#LDFLAGS="$LDFLAGS -ltinyxml"
#AC_LINK_IFELSE(
#  [AC_LANG_PROGRAM([#include <tinyxml.h>],[int x; TiXmlDocument doc])],
#  [TEST_LIBS="$TEST_LIBS -ltinyxml"] [HAVE_TINYXML=1],
#  [AC_MSG_ERROR([libtinyxml 2.6 is not installed.])])
#LDFLAGS=$SAVED_LDFLAGS


AC_CONFIG_FILES([
      Makefile
      lib/libexo/Makefile
])

AC_OUTPUT