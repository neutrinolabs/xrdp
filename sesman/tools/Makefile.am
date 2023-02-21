AM_CPPFLAGS = \
  -DXRDP_SYSCONF_PATH=\"${sysconfdir}\" \
  -DXRDP_CFG_PATH=\"${sysconfdir}/xrdp\" \
  -DXRDP_SBIN_PATH=\"${sbindir}\" \
  -DXRDP_SHARE_PATH=\"${datadir}/xrdp\" \
  -DXRDP_PID_PATH=\"${localstatedir}/run\" \
  -DXRDP_SOCKET_PATH=\"${socketdir}\" \
  -DXRDP_PAMCONF_PATH=\"${pamconfdir}\" \
  -DXRDP_AUTHMOD_SRC=\"../${AUTHMOD_SRC}\" \
  -I$(top_srcdir)/common \
  -I$(top_srcdir)/libipm \
  -I$(top_srcdir)/sesman

AM_CFLAGS = $(X_CFLAGS)

bin_PROGRAMS = \
  xrdp-sesrun \
  xrdp-sesadmin \
  xrdp-dis

noinst_PROGRAMS = \
  xrdp-authtest \
  xrdp-xcon

xrdp_sesrun_SOURCES = \
  sesrun.c \
  config.c \
  tools_common.h \
  tools_common.c

xrdp_sesadmin_SOURCES = \
  sesadmin.c \
  tools_common.h \
  tools_common.c

xrdp_dis_SOURCES = \
  dis.c

xrdp_dis_LDADD = \
  $(top_builddir)/common/libcommon.la

xrdp_xcon_SOURCES = \
  xcon.c

xrdp_authtest_SOURCES = \
  authmod.c \
  authtest.c

xrdp_sesrun_LDADD = \
  $(top_builddir)/common/libcommon.la \
  $(top_builddir)/libipm/libipm.la

xrdp_sesadmin_LDADD = \
  $(top_builddir)/common/libcommon.la \
  $(top_builddir)/libipm/libipm.la

xrdp_xcon_LDFLAGS = \
  $(X_LIBS)

xrdp_xcon_LDADD = \
  $(X_PRE_LIBS) -lX11 $(X_EXTRA_LIBS)

xrdp_authtest_LDADD = \
  $(top_builddir)/common/libcommon.la \
  $(top_builddir)/libipm/libipm.la \
  $(AUTHMOD_LIB)
