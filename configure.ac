AC_INIT([libpsys], [0.2],
	[http://groups.google.com/group/libpsys],
	[libpsys],
	[http://gitorious.org/libpsys])
AC_PREREQ([2.59])
AM_INIT_AUTOMAKE([foreign])
LT_INIT
AC_PROG_CC

AC_ARG_ENABLE([fallback],
	AS_HELP_STRING([--enable-fallback],
	[Build fallback backend (rpmlib required)]))

AM_CONDITIONAL([FALLBACK_ENABLED], [test "$enable_fallback" = "yes"])
AS_IF([test "$enable_fallback" = "yes"], [
	AC_CHECK_LIB(rpm, rpmReadConfigFiles, [], [
		echo "rpmlib is required for building the psys fallback, "
	 	echo "but is not installed. You should either install "
	 	echo "rpmlib or configure without --enable-fallback."
	 	exit -1
	])
])

AC_CONFIG_HEADERS([config.h])
AC_OUTPUT(
	Makefile
	fallback/Makefile
	lib/Makefile
	man/Makefile
)
