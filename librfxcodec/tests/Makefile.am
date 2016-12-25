EXTRA_DIST = readme.txt

AM_CPPFLAGS = \
  -I$(top_srcdir)/include

bin_PROGRAMS = rfxcodectest

rfxcodectest_SOURCES = rfxcodectest.c

rfxcodectest_LDADD = \
  $(top_builddir)/src/librfxencode.la
