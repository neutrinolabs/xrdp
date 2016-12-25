EXTRA_DIST =

EXTRA_INCLUDES =
EXTRA_LIBS =
EXTRA_FLAGS =

if GOT_PREFIX
EXTRA_INCLUDES += -I$(prefix)/include
EXTRA_FLAGS += -L$(prefix)/lib -Wl,-rpath -Wl,$(prefix)/lib
endif

AM_CPPFLAGS = \
  -I$(top_srcdir)/include \
  $(EXTRA_INCLUDES)

bin_PROGRAMS = paintertest

paintertest_SOURCES = paintertest.c

paintertest_LDADD = \
  $(top_builddir)/src/libpainter.la $(EXTRA_LIBS)

paintertest_LDFLAGS = $(EXTRA_FLAGS)

