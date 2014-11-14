# Process this file with autoconf to produce a configure script

AC_PREREQ(2.59)
AC_INIT([xrdpmod], [0.1.0], [xrdp-devel@lists.sourceforge.net])
AC_CONFIG_HEADERS(config_ac.h:config_ac-h.in)
AM_INIT_AUTOMAKE([1.6 foreign])
AC_PROG_CC
AC_C_CONST
AC_PROG_LIBTOOL

AM_CONDITIONAL(GOT_PREFIX, test "x${prefix}" != "xNONE"])

AC_CHECK_HEADER([xorg/xorg-server.h], [],
  [AC_MSG_ERROR([please install xserver-xorg-dev or xorg-x11-server-sdk])])

PKG_CHECK_MODULES([XORG_SERVER], [xorg-server >= 0])
AC_SUBST([XORG_SERVER_CFLAGS])
AC_SUBST([XORG_SERVER_LIBS])

moduledir=`pkg-config xorg-server --variable=moduledir`
AC_SUBST([moduledir])

if test "x${prefix}" = "xNONE" ; then
sysconfdir="/etc";
fi

AC_CONFIG_FILES([Makefile
                 module/Makefile
                 xrdpdev/Makefile
                 xrdpkeyb/Makefile
                 xrdpmouse/Makefile
])

AC_OUTPUT

