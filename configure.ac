AC_INIT([libpsys], [0.2.1],
	[http://groups.google.com/group/libpsys],
	[libpsys],
	[http://gitorious.org/libpsys])
AC_PREREQ([2.59])
AM_INIT_AUTOMAKE([foreign])
LT_INIT
AC_PROG_CC

#### Configure options ####

AC_ARG_ENABLE([fallback-all],
	AS_HELP_STRING([--enable-fallback-all],
	[Build all fallback backends]))

AC_ARG_ENABLE([fallback-dpkg],
	AS_HELP_STRING([--enable-fallback-dpkg],
	[Build DPKG fallback backend (libdpkg required)]))

AC_ARG_ENABLE([fallback-rpm],
	AS_HELP_STRING([--enable-fallback-rpm],
	[Build RPM fallback backend (rpmlib required)]))

#### ENABLE_FALLBACK_DPKG ####

test "$enable_fallback_dpkg" = "yes" -o "$enable_fallback_all" = "yes"
fallback_dpkg=$?
AM_CONDITIONAL([ENABLE_FALLBACK_DPKG], [test "$fallback_dpkg" = 0])
AS_IF([test "$fallback_dpkg" = 0], [
	AC_CHECK_HEADERS([dpkg/dpkg.h], [], [
		echo "libdpkg is required for building the DPKG fallback "
		echo "backend, but is not installed. You should either "
		echo "install libdpkg (see README) or build without "
		echo "--enable-fallback-dpkg."
	 	exit -1
	],
	[#define LIBDPKG_VOLATILE_API])
])

#### ENABLE_FALLBACK_RPM ####

test "$enable_fallback_rpm" = "yes" -o "$enable_fallback_all" = "yes"
fallback_rpm=$?
AM_CONDITIONAL([ENABLE_FALLBACK_RPM], [test "$fallback_rpm" = 0])
AS_IF([test "$fallback_rpm" = 0], [
	AC_CHECK_LIB(rpm, rpmReadConfigFiles, [], [
		echo "rpmlib is required for building the RPM fallback "
		echo "backend, but is not installed. You should either "
		echo "install rpmlib or build without --enable-fallback-rpm."
	 	exit -1
	])
])

#### ENABLE_FALLBACK ####
AM_CONDITIONAL([ENABLE_FALLBACK],
	[test "$enable_fallback_dpkg" = "yes" -o "$enable_fallback_rpm" = "yes"])

#### Output ####
AC_CONFIG_HEADERS([config.h])
AC_OUTPUT(
	Makefile
	fallback/Makefile
	lib/Makefile
	man/Makefile
)
