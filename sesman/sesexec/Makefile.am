AM_CPPFLAGS = \
  -DXRDP_CFG_PATH=\"${sysconfdir}/${sysconfsubdir}\" \
  -DXRDP_SBIN_PATH=\"${sbindir}\" \
  -DXRDP_LIBEXEC_PATH=\"${libexecdir}/xrdp\" \
  -DXRDP_SOCKET_ROOT_PATH=\"${socketdir}\" \
  -I$(top_srcdir)/sesman/libsesman \
  -I$(top_srcdir)/libipm \
  -I$(top_srcdir)/common

SESEXEC_EXTRA_LIBS =

if XRDP_UTMP
    AM_CPPFLAGS += -DUSE_UTMP
endif

pkglibexec_PROGRAMS = \
  xrdp-sesexec

xrdp_sesexec_SOURCES = \
  sesexec.c \
  sesexec.h \
  session_base.c \
  session_base.h \
  session_x11.c \
  session_x11.h \
  session_parameters.c \
  session_parameters.h \
  session.c \
  session.h \
  ccp_server.c \
  ccp_server.h \
  eicp_server.c \
  eicp_server.h \
  ercp_server.c \
  ercp_server.h \
  env.c \
  env.h \
  login_info.c \
  login_info.h \
  sesexec_discover.c \
  sesexec_discover.h \
  sessionrecord.c \
  sessionrecord.h \
  xauth.c \
  xauth.h \
  display_server_util.c \
  display_server_util.h

xrdp_sesexec_LDFLAGS =

xrdp_sesexec_LDADD = \
  $(top_builddir)/sesman/libsesman/libsesman.la \
  $(top_builddir)/libipm/libipm.la \
  $(top_builddir)/common/libcommon.la \
  $(SESEXEC_EXTRA_LIBS)
