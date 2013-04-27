# Process this file with autoconf to produce a configure script

AC_PREREQ(2.59)
AC_INIT([xrdp], [0.6.0], [xrdp-devel@lists.sourceforge.net])
AM_CONFIG_HEADER(config_ac.h:config_ac-h.in)
AM_INIT_AUTOMAKE([1.6 foreign])
AC_PROG_CC
AC_C_CONST
AC_PROG_LIBTOOL
AC_ARG_WITH([systemdsystemunitdir],
        AS_HELP_STRING([--with-systemdsystemunitdir=DIR], [Directory for systemd service files]),
        [], [with_systemdsystemunitdir=$($PKG_CONFIG --variable=systemdsystemunitdir systemd)])
if test "x$with_systemdsystemunitdir" != xno; then
        AC_SUBST([systemdsystemunitdir], [$with_systemdsystemunitdir])
fi
AM_CONDITIONAL(HAVE_SYSTEMD, [test -n "$with_systemdsystemunitdir" -a "x$with_systemdsystemunitdir" != xno ])

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
AC_ARG_ENABLE(jpeg, AS_HELP_STRING([--enable-jpeg],
              [Build jpeg module (default: no)]),
              [jpeg=true], [jpeg=false])
AM_CONDITIONAL(XRDP_JPEG, [test x$jpeg = xtrue])
AC_ARG_ENABLE(simplesound, AS_HELP_STRING([--enable-simplesound],
              [Build simple pulse audio interface (default: no)]),
              [simplesound=true], [simplesound=false])
AM_CONDITIONAL(XRDP_SIMPLESOUND, [test x$simplesound = xtrue])
AC_ARG_ENABLE(fuse, AS_HELP_STRING([--enable-fuse],
              [Build fuse(clipboard file / drive redir) (default: no)]),
              [fuse=true], [fuse=false])
AM_CONDITIONAL(XRDP_FUSE, [test x$fuse = xtrue])
AC_ARG_ENABLE(xrdpvr, AS_HELP_STRING([--enable-xrdpvr],
              [Build xrdpvr module (default: no)]),
              [xrdpvr=true], [xrdpvr=false])
AM_CONDITIONAL(XRDP_XRDPVR, [test x$xrdpvr = xtrue])

AM_CONDITIONAL(GOT_PREFIX, test "x${prefix}" != "xNONE"])

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

AC_CHECK_MEMBER([struct in6_addr.s6_addr],
                [],
                [AC_DEFINE(NO_ARPA_INET_H_IP6, 1, [for IPv6])],
                [#include <arpa/inet.h>])

if test "x$enable_nopam" = "xyes"
then
  AC_DEFINE([USE_NOPAM],1,[Disable PAM])
fi

AS_IF( [test "x$enable_freerdp1" = "xyes"] , [PKG_CHECK_MODULES(FREERDP, freerdp >= 1.0.0)] )

# checking for libjpeg
if ! test -z "$enable_jpeg"
then
  AC_CHECK_HEADER([jpeglib.h], [],
    [AC_MSG_ERROR([please install libjpeg-dev or libjpeg-devel])])
fi

# checking for fuse
if ! test -z "$enable_fuse"
then
  AC_CHECK_HEADER([fuse.h], [],
    [AC_MSG_ERROR([please install libfuse-dev or fuse-devel])],
    [#define _FILE_OFFSET_BITS 64])
fi

# checking for libpulse libpulse-simple
if ! test -z "$enable_simplesound"
then
  AC_CHECK_HEADER([pulse/simple.h], [],
    [AC_MSG_ERROR([please install libpulse-dev or libpulse-devel])])
fi

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
		 instfiles/init.d/Makefile
		 instfiles/default/Makefile
                 genkeymap/Makefile
                 xrdpapi/Makefile
                 xrdpvr/Makefile
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
