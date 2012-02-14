# Process this file with autoconf to produce a configure script

AC_PREREQ(2.59)
AC_INIT([xrdp], [0.6.0], [xrdp-devel@lists.sourceforge.net])
AM_CONFIG_HEADER(config_ac.h:config_ac-h.in)
AM_INIT_AUTOMAKE([1.6 foreign])
AC_PROG_CC
AC_C_CONST
AC_PROG_LIBTOOL
AC_ARG_ENABLE(nopam, AS_HELP_STRING([--enable-nopam],
              [Build no PAM support (default: no)]),
              [nopam=true], [nopam=false])
AM_CONDITIONAL(SESMAN_NOPAM, [test x$nopam = xtrue])
AC_ARG_ENABLE(kerberos, AS_HELP_STRING([--enable-kerberos],
              [Build kerberos support (default: no)]),
              [kerberos=true], [kerberos=false])
AM_CONDITIONAL(SESMAN_KERBEROS, [test x$kerberos = xtrue])
AC_ARG_ENABLE(pamuserpass, AS_HELP_STRING([--enable-pamuserpass],
              [Build pam userpass support (default: no)]),
              [pamuserpass=true], [pamuserpass=false])
AM_CONDITIONAL(SESMAN_PAMUSERPASS, [test x$pamuserpass = xtrue])
AC_ARG_ENABLE(xrdpdebug, AS_HELP_STRING([--enable-xrdpdebug],
              [Build debug (default: no)]),
              [xrdpdebug=true], [xrdpdebug=false])
AM_CONDITIONAL(XRDP_DEBUG, [test x$xrdpdebug = xtrue])
AC_ARG_ENABLE(freerdp1, AS_HELP_STRING([--enable-freerdp1],
              [Build freerdp1 module (default: no)]),
              [freerdp1=true], [freerdp1=false])
AM_CONDITIONAL(XRDP_FREERDP1, [test x$freerdp1 = xtrue])

# checking for openssl
AC_CHECK_HEADER([openssl/rc4.h], [],
  [AC_MSG_ERROR([please install libssl-dev or openssl-devel])],
  [#include <stdlib.h>])

# checking if pam should be autodetected.
if test -z "$enable_nopam"
then
  if test -z "$enable_kerberos"
  then
    AC_CHECK_HEADER([security/pam_appl.h], [],
      [AC_MSG_ERROR([please install libpam0g-dev or pam-devel])])
  fi
fi

AS_IF( [test "x$enable_freerdp1" = "xyes"] , [PKG_CHECK_MODULES(FREERDP, freerdp >= 1.0.0)] )

# checking for Xlib, Xfixes
AC_CHECK_HEADER([X11/Xlib.h], [],
  [AC_MSG_ERROR([please install libx11-dev or libX11-devel])])
AC_CHECK_HEADER([X11/extensions/Xfixes.h], [],
  [AC_MSG_ERROR([please install libx11-dev and libxfixes-dev or libXfixes-devel])],
  [#include <X11/Xlib.h>])

libdir="${libdir}/xrdp";
if test "x${prefix}" = "xNONE" ; then
sysconfdir="/etc";
localstatedir="/var";
fi
AC_CONFIG_FILES([Makefile
                 common/Makefile
                 vnc/Makefile
                 rdp/Makefile
                 libxrdp/Makefile
                 xup/Makefile
                 mc/Makefile
                 freerdp1/Makefile
                 xrdp/Makefile
                 sesman/Makefile
                 sesman/libscp/Makefile
                 sesman/tools/Makefile
                 sesman/sessvc/Makefile
                 sesman/chansrv/Makefile
                 keygen/Makefile
                 docs/Makefile
                 docs/man/Makefile
                 instfiles/Makefile
                 instfiles/pam.d/Makefile
                 genkeymap/Makefile
])
#                 fontdump/Makefile
#                 xrdp/cursors/Makefile
#                 Xserver/hw/rdp/Makefile
AC_OUTPUT

# example of how to check for a struct in a header
#AC_CHECK_MEMBER([struct in6_addr.s6_addr],
#                [],
#                [AC_DEFINE(NO_ARPA_INET_H_IP6, 1, [for IPv6])],
#                [#include <arpa/inet.h>])
