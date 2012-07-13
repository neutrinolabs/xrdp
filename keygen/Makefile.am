
AM_CFLAGS = \
  -DXRDP_CFG_PATH=\"${sysconfdir}/xrdp\" \
  -DXRDP_SBIN_PATH=\"${sbindir}\" \
  -DXRDP_SHARE_PATH=\"${datadir}/xrdp\" \
  -DXRDP_PID_PATH=\"${localstatedir}/run\"

INCLUDES = \
  -I$(top_srcdir)/common

bin_PROGRAMS = \
  xrdp-keygen

xrdp_keygen_SOURCES = keygen.c

xrdp_keygen_LDADD = \
  $(top_builddir)/common/libcommon.la
