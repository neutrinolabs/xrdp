
AM_CFLAGS = \
  -DXRDP_CFG_PATH=\"${sysconfdir}/xrdp\" \
  -DXRDP_SBIN_PATH=\"${sbindir}\" \
  -DXRDP_SHARE_PATH=\"${datadir}/xrdp\" \
  -DXRDP_PID_PATH=\"${localstatedir}/run\"

INCLUDES = \
  -I$(top_srcdir)/common

sbin_PROGRAMS = \
  xrdp-sessvc

xrdp_sessvc_SOURCES = \
  sessvc.c

xrdp_sessvc_LDADD = \
  $(top_builddir)/common/libcommon.la
