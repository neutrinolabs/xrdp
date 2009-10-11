# Process this file with autoconf to produce a configure script

AC_PREREQ(2.59)
AC_INIT([xrdp], [0.5.0], [xrdp-devel@lists.sourceforge.net])
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

# checking for openssl
AC_CHECK_HEADER([openssl/rc4.h], [], [AC_MSG_ERROR([please install libssl-dev or openssl-devel])])

# checking if pam should be autodetected.
if test -z "$enable_nopam"
then
  if test -z "$enable_kerberos"
  then
    AC_CHECK_HEADER([security/pam_appl.h], [], [AC_MSG_ERROR([please install libpam0g-dev or pam-devel])])
  fi
fi

# checking for Xlib, Xfixes
AC_CHECK_HEADERS([X11/Xlib.h X11/extensions/Xfixes.h], [],
  [AC_MSG_ERROR([please install libx11-dev and libxfixes-dev])],
  [[#ifdef HAVE_XLIB_H
    #include <X11/Xlib.h>
    #endif]])

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
