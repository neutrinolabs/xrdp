EXTRA_DIST = \
  Doxyfile

AM_CPPFLAGS = \
  -DXRDP_SYSCONF_PATH=\"${sysconfdir}\" \
  -DXRDP_CFG_PATH=\"${sysconfdir}/xrdp\" \
  -DXRDP_SBIN_PATH=\"${sbindir}\" \
  -DXRDP_SHARE_PATH=\"${datadir}/xrdp\" \
  -DXRDP_PID_PATH=\"${localstatedir}/run\" \
  -DXRDP_SOCKET_PATH=\"${socketdir}\" \
  -DSESMAN_RUNTIME_PATH=\"${sesmanruntimedir}\" \
  -I$(top_srcdir)/common \
  -I$(top_srcdir)/libipm

if SESMAN_BSD
AUTH_C = verify_user_bsd.c
AUTH_LIB =
else
if SESMAN_PAMUSERPASS
AUTH_C = verify_user_pam_userpass.c
AUTH_LIB = -lpam -lpam_userpass
else
if SESMAN_KERBEROS
AUTH_C = verify_user_kerberos.c
AUTH_LIB = -lkrb5
else
if SESMAN_NOPAM
AUTH_C = verify_user.c
AUTH_LIB = -lcrypt
else
AUTH_C = verify_user_pam.c
AUTH_LIB = -lpam
endif
endif
endif
endif

sbin_PROGRAMS = \
  xrdp-sesman

xrdp_sesman_SOURCES = \
  access.c \
  access.h \
  auth.h \
  config.c \
  config.h \
  env.c \
  env.h \
  lock_uds.c \
  lock_uds.h \
  scp_process.c \
  scp_process.h \
  sesman.c \
  sesman.h \
  session.c \
  session.h \
  sig.c \
  sig.h \
  xauth.c \
  xauth.h \
  $(AUTH_C)

xrdp_sesman_LDADD = \
  $(top_builddir)/libipm/libipm.la \
  $(top_builddir)/common/libcommon.la \
  $(AUTH_LIB) \
  -lpthread

sesmansysconfdir=$(sysconfdir)/xrdp

SUBST_VARS = sed \
   -e 's|@sesmansysconfdir[@]|$(sesmansysconfdir)|g'

subst_verbose = $(subst_verbose_@AM_V@)
subst_verbose_ = $(subst_verbose_@AM_DEFAULT_V@)
subst_verbose_0 = @echo "  SUBST    $@";

SUFFIXES = .in
.in:
	$(subst_verbose)$(SUBST_VARS) $< > $@

dist_sesmansysconf_DATA = \
  sesman.ini

dist_sesmansysconf_SCRIPTS = \
  startwm.sh \
  reconnectwm.sh

SUBDIRS = \
  tools \
  chansrv
