# Process this file with autoconf to produce a configure script

AC_PREREQ(2.59)
AC_INIT([libpainter], [0.1.0], [xrdp-devel@lists.sourceforge.net])
AC_CONFIG_HEADERS(config_ac.h:config_ac-h.in)
AM_INIT_AUTOMAKE([1.6 foreign])
AC_PROG_CC
AC_C_CONST
AC_PROG_LIBTOOL

AM_CONDITIONAL(GOT_PREFIX, test "x${prefix}" != "xNONE"])

if test "x${prefix}" = "xNONE" ; then
sysconfdir="/etc";
fi

AC_CONFIG_FILES([Makefile
                 src/Makefile
                 tests/Makefile
])

AC_OUTPUT

