
EXTRA_DIST = readme.txt

EXTRA_INCLUDES =
EXTRA_LIBS =
EXTRA_FLAGS =

if GOT_PREFIX
EXTRA_INCLUDES += -I$(prefix)/include
EXTRA_FLAGS += -L$(prefix)/lib -Wl,-rpath -Wl,$(prefix)/lib
endif

INCLUDES = \
  -I$(top_srcdir)/include \
  $(EXTRA_INCLUDES)

bin_PROGRAMS = rfxcodectest

rfxcodectest_SOURCES = rfxcodectest.c

rfxcodectest_LDADD = \
  $(top_builddir)/src/librfxencode.la $(EXTRA_LIBS)

rfxcodectest_LDFLAGS = $(EXTRA_FLAGS)

