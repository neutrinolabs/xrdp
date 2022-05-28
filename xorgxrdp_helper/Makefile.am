AM_CPPFLAGS = \
  -I$(top_srcdir)/common

XRDP_EXTRA_LIBS =
XRDP_EXTRA_SOURCES =

if XRDP_NVENC
AM_CPPFLAGS += -DXRDP_NVENC
AM_CPPFLAGS += $(XRDP_NVENC_CFLAGS)
XRDP_EXTRA_LIBS += $(XRDP_NVENC_LIBS)
XRDP_EXTRA_SOURCES += xorgxrdp_helper_nvenc.c xorgxrdp_helper_nvenc.h
endif

if XRDP_YAMI
AM_CPPFLAGS += -DXRDP_YAMI
AM_CPPFLAGS += $(XRDP_YAMI_CFLAGS)
XRDP_EXTRA_LIBS += $(XRDP_YAMI_LIBS)
XRDP_EXTRA_SOURCES += xorgxrdp_helper_yami.c xorgxrdp_helper_yami.h
endif

bin_PROGRAMS = \
  xorgxrdp_helper

xorgxrdp_helper_SOURCES = \
  xorgxrdp_helper.c \
  xorgxrdp_helper.h \
  xorgxrdp_helper_x11.c \
  xorgxrdp_helper_x11.h \
  $(XRDP_EXTRA_SOURCES)

xorgxrdp_helper_LDADD = \
  $(top_builddir)/common/libcommon.la \
  $(XRDP_EXTRA_LIBS) \
  -lX11 -lepoxy

