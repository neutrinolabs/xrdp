EXTRA_DIST = xrdp-freerdp.h

AM_CFLAGS = \
  -DXRDP_CFG_PATH=\"${sysconfdir}/xrdp\" \
  -DXRDP_SBIN_PATH=\"${sbindir}\" \
  -DXRDP_SHARE_PATH=\"${datadir}/xrdp\" \
  -DXRDP_PID_PATH=\"${localstatedir}/run\"

INCLUDES = \
  -I$(top_srcdir)/common \
  -I$(top_srcdir)/../freerdp/include/freerdp

lib_LTLIBRARIES = \
  libxrdpfreerdp.la

libxrdpfreerdp_la_SOURCES = xrdp-freerdp.c

libxrdpfreerdp_la_LIBADD = \
  $(top_srcdir)/common/libcommon.la
