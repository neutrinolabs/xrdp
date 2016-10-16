# Process this file with autoconf to produce a configure script

AC_PREREQ(2.59)
AC_INIT([rfxcodec], [0.1.0], [jay.sorg@gmail.com])
AC_CONFIG_HEADERS(config_ac.h:config_ac-h.in)
AM_INIT_AUTOMAKE([1.6 foreign])
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES])
AC_PROG_CC
AC_C_CONST
AC_PROG_LIBTOOL

# SIMD is optional
AC_ARG_WITH([simd],
    AC_HELP_STRING([--without-simd],[Omit SIMD extensions.]))
if test "x${with_simd}" != "xno"; then
  # Check if we're on a supported CPU
  AC_MSG_CHECKING([if we have SIMD optimisations for cpu type])
  case "$host_cpu" in
    x86_64 | amd64)
      AC_MSG_RESULT([yes (x86_64)])
      AC_PROG_NASM
      simd_arch=x86_64
    ;;
    i*86 | x86 | ia32)
      AC_MSG_RESULT([yes (i386)])
      AC_PROG_NASM
      simd_arch=i386
    ;;
    *)
      AC_MSG_RESULT([no ("$host_cpu")])
      AC_MSG_WARN([SIMD support not available for this CPU.  Performance will suffer.])
      with_simd=no;
    ;;
  esac
  if test "x${with_simd}" != "xno"; then
    AC_DEFINE([WITH_SIMD], [1], [Use accelerated SIMD routines.])
  fi
fi

AM_CONDITIONAL(WITH_SIMD_AMD64, [test x$simd_arch = xx86_64])
AM_CONDITIONAL(WITH_SIMD_X86, [test x$simd_arch = xi386])

AC_CONFIG_FILES([Makefile
                 src/Makefile
                 tests/Makefile
])

AC_OUTPUT

