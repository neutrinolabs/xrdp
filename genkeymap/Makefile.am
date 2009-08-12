
AM_CFLAGS = \
  -DXRDP_CFG_PATH=\"${sysconfdir}/xrdp\" \
  -DXRDP_SBIN_PATH=\"${sbindir}\" \
  -DXRDP_SHARE_PATH=\"${datadir}/xrdp\" \
  -DXRDP_PID_PATH=\"${localstatedir}/run\"

INCLUDES = \
  -I$(top_srcdir)/common

bin_PROGRAMS = \
  xrdp-genkeymap

xrdp_genkeymap_SOURCES = genkeymap.c

xrdp_genkeymap_LDADD = \
  $(top_srcdir)/common/libcommon.la \
  -lX11
