EXTRA_DIST = windows

# Some programs need freetype2 to build
if USE_FREETYPE2
  MKFV1 = xrdp-mkfv1
else
  MKFV1 =
endif

AM_CPPFLAGS =  \
  -I$(top_builddir) \
  -I$(top_srcdir)/common \
  $(FREETYPE2_CFLAGS)

bin_PROGRAMS = \
  $(MKFV1) \
  xrdp-dumpfv1

xrdp_mkfv1_SOURCES = \
  mkfv1.c \
  fv1.c \
  fv1.h

xrdp_mkfv1_LDADD = \
  $(top_builddir)/common/libcommon.la \
  $(FREETYPE2_LIBS)

xrdp_dumpfv1_SOURCES = \
  dumpfv1.c \
  fv1.c \
  fv1.h

xrdp_dumpfv1_LDADD = \
  $(top_builddir)/common/libcommon.la
