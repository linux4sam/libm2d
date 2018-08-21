AM_CFLAGS = $(WARN_CFLAGS) -I$(top_srcdir) -I$(top_srcdir)/include

lib_LTLIBRARIES = libm2d.la
libm2d_la_CFLAGS = $(AM_CFLAGS) $(LIBDRM_CFLAGS) $(CJSON_CFLAGS)
libm2d_la_SOURCES = m2d.c utils.c

libm2dincludedir = ${includedir}/m2d
libm2dinclude_HEADERS = $(top_srcdir)/include/m2d/m2d.h \
	$(top_srcdir)/include/m2d/utils.h
libm2d_la_LDFLAGS = -version-number 1:0:0 -no-undefined

pkgconfigdir = @pkgconfigdir@
pkgconfig_DATA = $(top_builddir)/libm2d.pc
