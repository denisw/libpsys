lib_LTLIBRARIES = libpsys_impl.la
libpsys_impl_la_CFLAGS = -I../lib -Wall -Werror 
libpsys_impl_la_LDFLAGS = -ldl

libpsys_impl_la_SOURCES = \
	fallback.c \
	fallback_private.h

if ENABLE_FALLBACK_DPKG
libpsys_impl_la_LDFLAGS += -ldpkg
libpsys_impl_la_SOURCES += fallback_dpkg.c
endif

if ENABLE_FALLBACK_RPM
libpsys_impl_la_LDFLAGS += -lrpm
libpsys_impl_la_CFLAGS += -Wno-deprecated-declarations
libpsys_impl_la_SOURCES += fallback_rpm.c
endif
