EXTRA_DIST = tcp.h

AM_CFLAGS = \
  -DXRDP_CFG_PATH=\"${sysconfdir}/xrdp\" \
  -DXRDP_SBIN_PATH=\"${sbindir}\" \
  -DXRDP_SHARE_PATH=\"${datadir}/xrdp\" \
  -DXRDP_PID_PATH=\"${localstatedir}/run\"

INCLUDES = \
  -I$(top_srcdir)/common \
  -I$(top_srcdir)/sesman/libscp \
  -I$(top_srcdir)/sesman

bin_PROGRAMS = \
  xrdp-sesrun \
  xrdp-sestest \
  xrdp-sesadmin \
  xrdp-dis \
  xrdp-xcon

xrdp_sesrun_SOURCES = \
  sesrun.c \
  tcp.c \
  config.c

xrdp_sestest_SOURCES = \
  sestest.c

xrdp_sesadmin_SOURCES = \
  sesadmin.c

xrdp_dis_SOURCES = \
  dis.c

xrdp_xcon_SOURCES = \
  xcon.c

xrdp_sesrun_LDADD = \
  $(top_builddir)/common/libcommon.la

xrdp_sestest_LDADD = \
  $(top_builddir)/common/libcommon.la \
  $(top_builddir)/sesman/libscp/libscp.la

xrdp_sesadmin_LDADD = \
  $(top_builddir)/common/libcommon.la \
  $(top_builddir)/sesman/libscp/libscp.la

xrdp_xcon_LDADD = \
  -L/usr/X11R6/lib \
  -lX11
