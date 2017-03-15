AM_CPPFLAGS = \
  -DXRDP_CFG_PATH=\"${sysconfdir}/xrdp\" \
  -DXRDP_SBIN_PATH=\"${sbindir}\" \
  -DXRDP_SHARE_PATH=\"${datadir}/xrdp\" \
  -DXRDP_PID_PATH=\"${localstatedir}/run\" \
  -DXRDP_SOCKET_PATH=\"${socketdir}\" \
  -I$(top_srcdir)/common \
  -I$(top_srcdir)/sesman/libscp \
  -I$(top_srcdir)/sesman

if XRDP_DEBUG
AM_CPPFLAGS += -DXRDP_DEBUG
endif

AM_CFLAGS = $(X_CFLAGS)

bin_PROGRAMS = \
  xrdp-sesrun \
  xrdp-sesadmin \
  xrdp-dis

noinst_PROGRAMS = \
  xrdp-sestest \
  xrdp-xcon

xrdp_sesrun_SOURCES = \
  config.c \
  sesrun.c \
  tcp.c \
  tcp.h

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

xrdp_xcon_LDFLAGS = \
  $(X_LIBS)

xrdp_xcon_LDADD = \
  $(X_PRE_LIBS) -lX11 $(X_EXTRA_LIBS)
