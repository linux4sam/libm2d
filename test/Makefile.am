AM_CFLAGS = $(WARN_CFLAGS) -I$(top_srcdir) -I$(top_srcdir)/include

bin_PROGRAMS = copytest filltest roptest blendtest

copytest_SOURCES = copytest.c
copytest_CFLAGS = $(AM_CFLAGS) $(LIBDRM_CFLAGS) $(CAIRO_CFLAGS)
copytest_LDADD = $(top_builddir)/src/libm2d.la $(LIBDRM_LIBS) $(CAIRO_LIBS)
copytestdir = $(prefix)/share/@PACKAGE@
copytest_DATA = $(top_srcdir)/test/copy.png
copytest_LDFLAGS = $(AM_LDFLAGS)

filltest_SOURCES = filltest.c
filltest_CFLAGS = $(AM_CFLAGS) $(LIBDRM_CFLAGS)
filltest_LDADD = $(top_builddir)/src/libm2d.la $(LIBDRM_LIBS)
filltestdir = $(prefix)/share/@PACKAGE@
filltest_LDFLAGS = $(AM_LDFLAGS)

roptest_SOURCES = roptest.c
roptest_CFLAGS = $(AM_CFLAGS) $(LIBDRM_CFLAGS) $(CAIRO_CFLAGS)
roptest_LDADD = $(top_builddir)/src/libm2d.la $(LIBDRM_LIBS) $(CAIRO_LIBS)
roptestdir = $(prefix)/share/@PACKAGE@
roptest_DATA = $(wildcard $(top_srcdir)/test/rop*.png)
roptest_LDFLAGS = $(AM_LDFLAGS)

blendtest_SOURCES = blendtest.c
blendtest_CFLAGS = $(AM_CFLAGS) $(LIBDRM_CFLAGS) $(CAIRO_CFLAGS)
blendtest_LDADD = $(top_builddir)/src/libm2d.la $(LIBDRM_LIBS) $(CAIRO_LIBS)
blendtestdir = $(prefix)/share/@PACKAGE@
blendtest_DATA = $(wildcard $(top_srcdir)/test/blend*.png)
blendtest_LDFLAGS = $(AM_LDFLAGS)
