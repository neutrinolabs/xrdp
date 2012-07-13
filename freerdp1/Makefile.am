EXTRA_DIST = xrdp-freerdp.h

AM_CFLAGS = \
  -DXRDP_CFG_PATH=\"${sysconfdir}/xrdp\" \
  -DXRDP_SBIN_PATH=\"${sbindir}\" \
  -DXRDP_SHARE_PATH=\"${datadir}/xrdp\" \
  -DXRDP_PID_PATH=\"${localstatedir}/run\"

INCLUDES = \
  -I$(top_srcdir)/common \
  $(FREERDP_CFLAGS)

lib_LTLIBRARIES = \
  libxrdpfreerdp1.la

libxrdpfreerdp1_la_SOURCES = xrdp-freerdp.c xrdp-color.c

libxrdpfreerdp1_la_LIBADD = \
  $(top_builddir)/common/libcommon.la \
  $(FREERDP_LIBS)
