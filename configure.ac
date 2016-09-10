# Process this file with autoconf to produce a configure script

AC_PREREQ(2.59)
AC_INIT([libpainter], [0.1.0], [xrdp-devel@lists.sourceforge.net])
AC_CONFIG_HEADERS(config_ac.h:config_ac-h.in)
AM_INIT_AUTOMAKE([1.6 foreign])
AC_PROG_CC
AC_C_CONST
AC_PROG_LIBTOOL

# Use silent rules by default if supported by Automake
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

AM_CONDITIONAL(GOT_PREFIX, test "x${prefix}" != "xNONE"])

AC_ARG_ENABLE(tests, AS_HELP_STRING([--enable-tests],
              [Build install tests (default: no)]),
              [], [enable_tests=no])
AM_CONDITIONAL(PT_TESTS, [test x$enable_tests = xyes])

if test "x${prefix}" = "xNONE" ; then
sysconfdir="/etc";
fi

pkgconfigdir=${libdir}/pkgconfig
AC_SUBST(pkgconfigdir)

AC_CONFIG_FILES([Makefile
                 src/Makefile
                 include/Makefile
                 pkgconfig/Makefile
                 tests/Makefile
])

AC_OUTPUT([pkgconfig/libpainter.pc
])

