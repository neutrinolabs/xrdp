EXTRA_DIST = chansrv.h clipboard.h devredir.h sound.h

EXTRA_DEFINES =
EXTRA_INCLUDES =
EXTRA_LIBS =
EXTRA_FLAGS =

if XRDP_SIMPLESOUND
EXTRA_DEFINES += -DXRDP_SIMPLESOUND
EXTRA_LIBS += -lpthread -lpulse -lpulse-simple
endif

if XRDP_FUSE
EXTRA_DEFINES += -DXRDP_FUSE
EXTRA_LIBS += -lfuse
endif

AM_CFLAGS = \
  -DXRDP_CFG_PATH=\"${sysconfdir}/xrdp\" \
  -DXRDP_SBIN_PATH=\"${sbindir}\" \
  -DXRDP_SHARE_PATH=\"${datadir}/xrdp\" \
  -DXRDP_PID_PATH=\"${localstatedir}/run\" \
  $(EXTRA_DEFINES)

INCLUDES = \
  -I$(top_srcdir)/common \
  $(EXTRA_INCLUDES)

sbin_PROGRAMS = \
  xrdp-chansrv

xrdp_chansrv_SOURCES = \
  chansrv.c \
  sound.c \
  clipboard.c \
  clipboard_file.c \
  devredir.c \
  smartcard.c \
  rail.c \
  xcommon.c \
  drdynvc.c \
  chansrv_fuse.c \
  irp.c

xrdp_chansrv_LDFLAGS = \
  $(EXTRA_FLAGS)

xrdp_chansrv_LDADD = \
  -L/usr/X11R6/lib \
  $(top_builddir)/common/libcommon.la \
  -lX11 -lXfixes \
  $(EXTRA_LIBS)
