
AM_CPPFLAGS = \
  -DXRDP_SOCKET_ROOT_PATH=\"${socketdir}\" \
  -I$(top_srcdir)/common

module_LTLIBRARIES = \
  libipm.la

libipm_la_SOURCES = \
  libipm.h \
  libipm.c \
  libipm_send.c \
  libipm_recv.c \
  libipm_facilities.h \
  libipm_private.h \
  eicp.h \
  eicp.c \
  ercp.h \
  ercp.c \
  scp.h \
  scp.c \
  scp_sync.h \
  scp_sync.c \
  scp_application_types.h \
  scp_application_types.c

libipm_la_LIBADD = \
  $(top_builddir)/common/libcommon.la
