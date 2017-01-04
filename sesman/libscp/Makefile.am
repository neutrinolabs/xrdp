AM_CPPFLAGS = \
  -DXRDP_CFG_PATH=\"${sysconfdir}/xrdp\" \
  -DXRDP_SBIN_PATH=\"${sbindir}\" \
  -DXRDP_SHARE_PATH=\"${datadir}/xrdp\" \
  -DXRDP_PID_PATH=\"${localstatedir}/run\" \
  -I$(top_srcdir)/common

if XRDP_DEBUG
AM_CPPFLAGS += -DXRDP_DEBUG
endif

module_LTLIBRARIES = \
  libscp.la

libscp_la_SOURCES = \
  libscp.h \
  libscp_commands.h \
  libscp_commands_mng.h \
  libscp_connection.c \
  libscp_connection.h \
  libscp_init.c \
  libscp_init.h \
  libscp_lock.c \
  libscp_lock.h \
  libscp_session.c \
  libscp_session.h \
  libscp_tcp.c \
  libscp_tcp.h \
  libscp_types.h \
  libscp_types_mng.h \
  libscp_v0.c \
  libscp_v0.h \
  libscp_v1c.c \
  libscp_v1c.h \
  libscp_v1c_mng.c \
  libscp_v1c_mng.h \
  libscp_v1s.c \
  libscp_v1s.h \
  libscp_v1s_mng.c \
  libscp_v1s_mng.h \
  libscp_vX.c \
  libscp_vX.h

libscp_la_LIBADD = \
  $(top_builddir)/common/libcommon.la \
  -lpthread
