# Process this file with autoconf to produce a configure script

AC_PREREQ(2.59)
AC_INIT([xrdp], [0.5.0], [xrdp-devel@lists.sourceforge.net])
AC_CONFIG_HEADERS([config.h:config-h.in])
AM_INIT_AUTOMAKE([1.6 foreign])
AC_PROG_CC
AC_C_CONST
AC_PROG_LIBTOOL
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
])
#                 docs/Makefile
#                 docs/man/Makefile
#                 fontdump/Makefile
#                 keygen/Makefile
#                 sesman/tools/Makefile
#                 xrdp/cursors/Makefile
#                 Xserver/hw/rdp/Makefile
AC_OUTPUT
