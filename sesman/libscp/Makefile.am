
AM_CFLAGS = \
  -DXRDP_CFG_PATH=\"${sysconfdir}/xrdp\" \
  -DXRDP_SBIN_PATH=\"${sbindir}\" \
  -DXRDP_SHARE_PATH=\"${datadir}/xrdp\" \
  -DXRDP_PID_PATH=\"${localstatedir}/run\"

INCLUDES = \
  -I$(top_srcdir)/common

lib_LTLIBRARIES = \
  libscp.la

libscp_la_SOURCES = \
  libscp_connection.c \
  libscp_init.c \
  libscp_lock.c \
  libscp_session.c \
  libscp_tcp.c \
  libscp_v0.c \
  libscp_v1c.c \
  libscp_v1s.c \
  libscp_v1c_mng.c \
  libscp_v1s_mng.c \
  libscp_vX.c

libscp_la_LIBADD = \
  $(top_srcdir)/common/libcommon.la
