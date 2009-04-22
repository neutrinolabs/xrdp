INCLUDES = \
  -I$(top_srcdir)/common

sbin_PROGRAMS = \
  xrdp-chansrv

xrdp_chansrv_SOURCES = \
  chansrv.c \
  sound.c \
  clipboard.c \
  devredir.c

xrdp_chansrv_LDADD = \
  $(top_srcdir)/common/libcommon.la
