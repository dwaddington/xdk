AC_INIT([XDK],[0.1])
AC_CHECK_LIB([tinyxml],[tolower],[],AC_MSG_ERROR([libtinyxml-dev not installed]))

AC_MSG_CHECKING(whether to enable NUMA support)
AC_CHECK_LIB([numa],[numa_available], [use_numa=true], [use_numa=false])
AC_ARG_ENABLE(numa, [AS_HELP_STRING([--enable-numa],[enable numa support, requires libnuma-dev installed])])

case "$enable_numa" in
	no)
		use_numa=false
		;;
	*)	
		use_numa=true
		;;
esac


if $use_numa
then
	AC_MSG_NOTICE([use libnuma])
	AC_SUBST(numa_flag, "-D NUMA_ENABLE")
	AC_SUBST(numa_lib, "-lnuma")
	AC_CHECK_LIB([numa],[numa_available],[],AC_MSG_ERROR([libnuma-dev not installed]))
else
	AC_MSG_NOTICE([not use libnuma])
	AC_SUBST(numa_flag, "-D NUMA_DISABLE")
	AC_SUBST(numa_lib, " ")
fi



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
      mk/global.mk
])

AC_OUTPUT
