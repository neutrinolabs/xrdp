AM_CPPFLAGS = \
  -DXRDP_CFG_PATH=\"${sysconfdir}/${sysconfsubdir}\" \
  -DXRDP_SOCKET_ROOT_PATH=\"${socketdir}\" \
  -I$(top_srcdir)/sesman/libsesman \
  -I$(top_srcdir)/common \
  -I$(top_srcdir)/libipm

AM_CFLAGS = $(X_CFLAGS)

bin_PROGRAMS = \
  xrdp-sesrun \
  xrdp-sesadmin \
  xrdp-dis

noinst_PROGRAMS = \
  xrdp-authtest \
  xrdp-xcon

xrdp_sesrun_SOURCES = \
  sesrun.c

xrdp_sesadmin_SOURCES = \
  sesadmin.c

xrdp_dis_SOURCES = \
  dis.c

xrdp_dis_LDADD = \
  $(top_builddir)/common/libcommon.la

xrdp_xcon_SOURCES = \
  xcon.c

xrdp_authtest_SOURCES = \
  authtest.c

xrdp_sesrun_LDADD = \
  $(top_builddir)/sesman/libsesman/libsesman.la \
  $(top_builddir)/common/libcommon.la \
  $(top_builddir)/libipm/libipm.la

xrdp_sesadmin_LDADD = \
  $(top_builddir)/sesman/libsesman/libsesman.la \
  $(top_builddir)/common/libcommon.la \
  $(top_builddir)/libipm/libipm.la

xrdp_xcon_LDFLAGS = \
  $(X_LIBS)

xrdp_xcon_LDADD = \
  $(X_PRE_LIBS) -lX11 $(X_EXTRA_LIBS)

xrdp_authtest_LDADD = \
  $(top_builddir)/sesman/libsesman/libsesman.la \
  $(top_builddir)/common/libcommon.la \
  $(top_builddir)/libipm/libipm.la \
  $(AUTHMOD_LIB)
