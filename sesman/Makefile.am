EXTRA_DIST = sesman.ini startwm.sh sesman.h access.h auth.h config.h env.h lock.h scp.h scp_v0.h scp_v1.h scp_v1_mng.h session.h sig.h thread.h

AM_CFLAGS = \
  -DXRDP_CFG_PATH=\"${sysconfdir}/xrdp\" \
  -DXRDP_SBIN_PATH=\"${sbindir}\" \
  -DXRDP_SHARE_PATH=\"${datadir}/xrdp\" \
  -DXRDP_PID_PATH=\"${localstatedir}/run\"

INCLUDES = \
  -I$(top_srcdir)/common \
  -I$(top_srcdir)/sesman/libscp

if SESMAN_NOPAM
AUTH_C = verify_user.c
AUTH_LIB = -lcrypt
else
if SESMAN_PAMUSERPASS
AUTH_C = verify_user_pam_userpass.c
AUTH_LIB = -lpam -lpam_userpass
else
if SESMAN_KERBEROS
AUTH_C = verify_user_kerberos.c
AUTH_LIB = -lkrb5
else
AUTH_C = verify_user_pam.c
AUTH_LIB = -lpam
endif
endif
endif

sbin_PROGRAMS = \
  xrdp-sesman

xrdp_sesman_SOURCES = \
  scp.c \
  scp_v0.c \
  scp_v1.c \
  scp_v1_mng.c \
  sesman.c \
  session.c \
  sig.c \
  thread.c \
  lock.c \
  access.c \
  config.c \
  env.c \
  $(AUTH_C)

xrdp_sesman_LDADD = \
  $(top_builddir)/common/libcommon.la \
  $(top_builddir)/sesman/libscp/libscp.la \
  $(AUTH_LIB) \
  -lpthread

sesmansysconfdir=$(sysconfdir)/xrdp

sesmansysconf_DATA = \
  sesman.ini \
  startwm.sh

SUBDIRS = \
  libscp \
  tools \
  sessvc \
  chansrv

# must be tab below
install-data-hook:
	chmod 755 $(DESTDIR)$(sysconfdir)/xrdp/startwm.sh
