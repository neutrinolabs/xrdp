EXTRA_DIST = chansrv.h clipboard.h devredir.h sound.h

AM_CFLAGS = \
  -DXRDP_CFG_PATH=\"${sysconfdir}/xrdp\" \
  -DXRDP_SBIN_PATH=\"${sbindir}\" \
  -DXRDP_SHARE_PATH=\"${datadir}/xrdp\" \
  -DXRDP_PID_PATH=\"${localstatedir}/run\"

INCLUDES = \
  -I$(top_srcdir)/common

sbin_PROGRAMS = \
  xrdp-chansrv

xrdp_chansrv_SOURCES = \
  chansrv.c \
  sound.c \
  clipboard.c \
  devredir.c

xrdp_chansrv_LDADD = \
  -L/usr/X11R6/lib \
  $(top_srcdir)/common/libcommon.la \
  -lX11 -lXfixes
