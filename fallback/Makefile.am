lib_LTLIBRARIES = libpsys_impl.la
libpsys_impl_la_LDFLAGS = \
	-ldpkg -lrpm -ldl
libpsys_impl_la_CFLAGS = \
	-I../lib -Wall -Werror -Wno-deprecated-declarations

libpsys_impl_la_SOURCES = \
	fallback.c \
	fallback_dpkg.c \
	fallback_rpm.c
