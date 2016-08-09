EXTRA_DEFINES =
EXTRA_INCLUDES =
EXTRA_LIBS =
EXTRA_FLAGS =

if XRDP_FUSE
EXTRA_DEFINES += -DXRDP_FUSE
EXTRA_LIBS += -lfuse
endif

if XRDP_OPUS
EXTRA_DEFINES += -DXRDP_OPUS
EXTRA_LIBS += -lopus
endif

if XRDP_MP3LAME
EXTRA_DEFINES += -DXRDP_MP3LAME
EXTRA_LIBS += -lmp3lame
endif

AM_CPPFLAGS = \
  -DXRDP_CFG_PATH=\"${sysconfdir}/xrdp\" \
  -DXRDP_SBIN_PATH=\"${sbindir}\" \
  -DXRDP_SHARE_PATH=\"${datadir}/xrdp\" \
  -DXRDP_PID_PATH=\"${localstatedir}/run\" \
  $(EXTRA_DEFINES) \
  -I$(top_srcdir)/common \
  $(EXTRA_INCLUDES)

AM_CFLAGS = $(X_CFLAGS)

sbin_PROGRAMS = \
  xrdp-chansrv

xrdp_chansrv_SOURCES = \
  chansrv.c \
  chansrv.h \
  chansrv_common.c \
  chansrv_common.h \
  chansrv_fuse.c \
  chansrv_fuse.h \
  clipboard.c \
  clipboard.h \
  clipboard_common.h \
  clipboard_file.c \
  clipboard_file.h \
  devredir.c \
  devredir.h \
  drdynvc.c \
  drdynvc.h \
  fifo.c \
  fifo.h \
  irp.c \
  irp.h \
  mlog.h \
  rail.c \
  rail.h \
  smartcard.c \
  smartcard.h \
  smartcard_pcsc.c \
  smartcard_pcsc.h \
  sound.c \
  sound.h \
  xcommon.c \
  xcommon.h

xrdp_chansrv_LDFLAGS = \
  $(X_LIBS) \
  $(EXTRA_FLAGS)

xrdp_chansrv_LDADD = \
  $(top_builddir)/common/libcommon.la \
  $(X_PRE_LIBS) -lXfixes -lXrandr -lX11 $(X_EXTRA_LIBS) \
  $(EXTRA_LIBS)
