
EXTRA_DIST = readme.txt

EXTRA_INCLUDES =
EXTRA_LIBS =
EXTRA_FLAGS =

INCLUDES = \
  -I$(top_srcdir)/include \
  $(EXTRA_INCLUDES)

bin_PROGRAMS = rfxcodectest

rfxcodectest_SOURCES = rfxcodectest.c

rfxcodectest_LDADD = \
  $(top_builddir)/src/librfxencode.la $(EXTRA_LIBS)

rfxcodectest_LDFLAGS = $(EXTRA_FLAGS)

