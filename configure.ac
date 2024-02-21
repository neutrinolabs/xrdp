# Process this file with autoconf to produce a configure script

AC_PREREQ([2.69])
AC_INIT([xrdp], [0.10.80], [xrdp-devel@googlegroups.com])
AC_DEFINE([VERSION_YEAR], 2024, [Copyright year])
AC_CONFIG_HEADERS(config_ac.h:config_ac-h.in)
AM_INIT_AUTOMAKE([1.7.2 foreign])
AC_CONFIG_MACRO_DIR([m4])
AC_USE_SYSTEM_EXTENSIONS
AC_PROG_CC
AC_PROG_CXX
AC_C_CONST
LT_INIT

PKG_PROG_PKG_CONFIG
if test "x$PKG_CONFIG" = "x"; then
  AC_MSG_ERROR([please install pkg-config])
fi

AC_CONFIG_SUBDIRS([libpainter librfxcodec])

# Use silent rules by default if supported by Automake
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

case $host_os in
	*linux*)
		linux=yes
		;;
	*kfreebsd*)
		linux=yes # only used in instfiles/ so that’s ok for us for now
		;;
	*freebsd*)
		freebsd=yes
		;;
	*netbsd*)
		netbsd=yes
		;;
	*openbsd*)
		openbsd=yes
		;;
	*darwin*)
		macos=yes
		;;
esac

AM_CONDITIONAL(LINUX, [test "x$linux" = xyes])
AM_CONDITIONAL(FREEBSD, [test "x$freebsd" = xyes])
AM_CONDITIONAL(OPENBSD, [test "x$openbsd" = xyes])
AM_CONDITIONAL(NETBSD, [test "x$netbsd" = xyes])
AM_CONDITIONAL(MACOS, [test "x$macos" = xyes])

AC_CHECK_SIZEOF([int])
AC_CHECK_SIZEOF([long])
AC_CHECK_SIZEOF([void *])

# runstatedir not available for autoconf <= 2.69
if test "x$runstatedir" = "x" ; then
    runstatedir='${localstatedir}/run'
fi

AC_ARG_WITH([socketdir],
  [AS_HELP_STRING([--with-socketdir=DIR],
                  [Use directory for UNIX sockets for XRDP sessions (default: RUNSTATEDIR/xrdp)])],
                  [], [with_socketdir="$runstatedir/xrdp"])
AC_SUBST([socketdir], [$with_socketdir])

AC_ARG_WITH([systemdsystemunitdir],
        AS_HELP_STRING([--with-systemdsystemunitdir=DIR], [Directory for systemd service files, no to disable]),
        [], [
if test "x$linux" = xyes; then
  with_systemdsystemunitdir=$($PKG_CONFIG --variable=systemdsystemunitdir systemd)
fi
])

if test "x$with_systemdsystemunitdir" != xno; then
        AC_SUBST([systemdsystemunitdir], [$with_systemdsystemunitdir])
fi
AM_CONDITIONAL(HAVE_SYSTEMD, [test -n "$with_systemdsystemunitdir" -a "x$with_systemdsystemunitdir" != xno ])

AC_ARG_ENABLE(tests,
              AS_HELP_STRING([--enable-tests],
              [Ensure dependencies for the tests are installed]),
              [ensure_tests_deps=yes], [])
AC_ARG_ENABLE(pam, AS_HELP_STRING([--enable-pam],
              [Build PAM support (default: yes)]),
              [], [enable_pam=yes])
AM_CONDITIONAL(SESMAN_NOPAM, [test x$enable_pam != xyes])
AC_ARG_ENABLE(vsock, AS_HELP_STRING([--enable-vsock],
              [Build AF_VSOCK support (default: no)]),
              [], [enable_vsock=no])
AC_ARG_ENABLE(ipv6, AS_HELP_STRING([--enable-ipv6],
              [Build IPv6 support (default: no, experimental)]),
              [], [enable_ipv6=no])
AC_ARG_ENABLE(ipv6only, AS_HELP_STRING([--enable-ipv6only],
              [Build IPv6-only (default: no)]),
              [], [enable_ipv6only=no])
AC_ARG_ENABLE(kerberos, AS_HELP_STRING([--enable-kerberos],
              [Build kerberos support (prefer --enable-pam if available) (default: no)]),
              [], [enable_kerberos=no])
AC_ARG_ENABLE(bsd, AS_HELP_STRING([--enable-bsd],
              [Build BSD auth support (default: no)]),
              [bsd=true], [bsd=false])
AM_CONDITIONAL(SESMAN_BSD, [test x$bsd = xtrue])
AM_CONDITIONAL(SESMAN_KERBEROS, [test x$enable_kerberos = xyes])
AC_ARG_ENABLE(pamuserpass, AS_HELP_STRING([--enable-pamuserpass],
              [Build PAM userpass support (default: no)]),
              [], [enable_pamuserpass=no])
AM_CONDITIONAL(SESMAN_PAMUSERPASS, [test x$enable_pamuserpass = xyes])
AC_ARG_ENABLE(pam-config, AS_HELP_STRING([--enable-pam-config=CONF],
              [Select PAM config to install: arch, debian, redhat, suse, freebsd, macos, unix
               (default: autodetect)]))

# Development options. devel_all is first as this provides a default for
# the others
AC_ARG_ENABLE(devel_all, AS_HELP_STRING([--enable-devel-all],
              [Enable all development options (default: no)]),
              [devel_all=$enableval], [devel_all=no])
AC_ARG_ENABLE(devel_debug, AS_HELP_STRING([--enable-devel-debug],
              [Build exes with no optimisation and debugging symbols (default: no)]),
              [devel_debug=$enableval], [devel_debug=$devel_all])
AM_CONDITIONAL(DEVEL_DEBUG, [test x$devel_debug = xyes ])
AC_ARG_ENABLE(devel_logging, AS_HELP_STRING([--enable-devel-logging],
              [Enable development logging (default: no)]),
              [devel_logging=$enableval], [devel_logging=$devel_all])
AC_ARG_ENABLE(devel_streamcheck, AS_HELP_STRING([--enable-devel-streamcheck],
              [Add range-check/abort to stream primitives (default: no)]),
              [devel_streamcheck=$enableval], [devel_streamcheck=$devel_all])

AC_ARG_ENABLE(neutrinordp, AS_HELP_STRING([--enable-neutrinordp],
              [Build neutrinordp module (default: no)]),
              [], [enable_neutrinordp=no])
AM_CONDITIONAL(XRDP_NEUTRINORDP, [test x$enable_neutrinordp = xyes])

AC_ARG_ENABLE(ulalaca, AS_HELP_STRING([--enable-ulalaca],
              [Build ulalaca module (experimental) (default: no)]),
              [], [enable_ulalaca=no])
AM_CONDITIONAL(XRDP_ULALACA, [test x$enable_ulalaca = xyes])

AC_ARG_ENABLE(jpeg, AS_HELP_STRING([--enable-jpeg],
              [Build jpeg module (default: no)]),
              [], [enable_jpeg=no])
AM_CONDITIONAL(XRDP_JPEG, [test x$enable_jpeg = xyes])
AC_ARG_ENABLE(tjpeg, AS_HELP_STRING([--enable-tjpeg],
              [Build turbo jpeg module (default: no)]),
              [], [enable_tjpeg=no])
AM_CONDITIONAL(XRDP_TJPEG, [test x$enable_tjpeg = xyes])
AC_ARG_ENABLE(fuse, AS_HELP_STRING([--enable-fuse],
              [Build fuse(clipboard file / drive redir) (default: no)]),
              [], [enable_fuse=no])
AM_CONDITIONAL(XRDP_FUSE, [test x$enable_fuse = xyes])
AC_ARG_ENABLE(xrdpvr, AS_HELP_STRING([--enable-xrdpvr],
              [Build xrdpvr module (default: no)]),
              [], [enable_xrdpvr=no])
AM_CONDITIONAL(XRDP_XRDPVR, [test x$enable_xrdpvr = xyes])
AC_ARG_ENABLE(fdkaac, AS_HELP_STRING([--enable-fdkaac],
              [Build aac(audio codec) (default: no)]),
              [], [enable_fdkaac=no])
AM_CONDITIONAL(XRDP_FDK_AAC, [test x$enable_fdkaac = xyes])
AC_ARG_ENABLE(opus, AS_HELP_STRING([--enable-opus],
              [Build opus(audio codec) (default: no)]),
              [], [enable_opus=no])
AM_CONDITIONAL(XRDP_OPUS, [test x$enable_opus = xyes])
AC_ARG_ENABLE(mp3lame, AS_HELP_STRING([--enable-mp3lame],
              [Build lame mp3(audio codec) (default: no)]),
              [], [enable_mp3lame=no])
AM_CONDITIONAL(XRDP_MP3LAME, [test x$enable_mp3lame = xyes])
AC_ARG_ENABLE(pixman, AS_HELP_STRING([--enable-pixman],
              [Use pixman library (default: no)]),
              [], [enable_pixman=no])
AM_CONDITIONAL(XRDP_PIXMAN, [test x$enable_pixman = xyes])

AC_ARG_ENABLE(painter, AS_HELP_STRING([--disable-painter],
              [Do not use included painter library (default: no)]),
              [], [enable_painter=yes])
AM_CONDITIONAL(XRDP_PAINTER, [test x$enable_painter = xyes])

AC_ARG_ENABLE(rfxcodec, AS_HELP_STRING([--disable-rfxcodec],
              [Do not use included librfxcodec library (default: no)]),
              [], [enable_rfxcodec=yes])
AM_CONDITIONAL(XRDP_RFXCODEC, [test x$enable_rfxcodec = xyes])

AC_ARG_ENABLE(rdpsndaudin, AS_HELP_STRING([--enable-rdpsndaudin],
              [Use rdpsnd audio in (default: no)]),
              [], [enable_rdpsndaudin=no])
AM_CONDITIONAL(XRDP_RDPSNDAUDIN, [test x$enable_rdpsndaudin = xyes])

AC_ARG_ENABLE(utmp, AS_HELP_STRING([--enable-utmp],
              [Update utmp (default: no)]),
              [], [enable_utmp=no])
AM_CONDITIONAL(XRDP_UTMP, [test x$enable_utmp = xyes])

AC_ARG_WITH(imlib2, AS_HELP_STRING([--with-imlib2=ARG], [imlib2 library to use for non-BMP backgrounds (ARG=yes/no/<abs-path>)]),,)

AC_ARG_WITH(freetype2, AS_HELP_STRING([--with-freetype2=ARG], [freetype2 library to use for rendering fonts (ARG=yes/no/<abs-path>)]),,)

# Obsolete options
AC_ARG_ENABLE(xrdpdebug, AS_HELP_STRING([--enable-xrdpdebug],
              [This option is no longer supported - use --enable-devel-all]))

if test "x$enable_xrdpdebug" != x; then
  AC_MSG_ERROR([--enable-xrdpdebug must be replaced with one or more --enable-devel-* options])
fi

# configure compiler options and CFLAGS
AX_GCC_FUNC_ATTRIBUTE([format])
AX_TYPE_SOCKLEN_T
AX_CFLAGS_WARN_ALL
AX_APPEND_COMPILE_FLAGS([-Wwrite-strings])

AM_COND_IF([LINUX],
  [AX_APPEND_COMPILE_FLAGS([-Werror])]) # bsd has warnings that have not been fixed yet

AM_COND_IF([DEVEL_DEBUG],
  [AX_APPEND_COMPILE_FLAGS([-g -O0])],
  [AX_APPEND_COMPILE_FLAGS([-O2])])

# Function setusercontext() is in BSD -lutil but N/A on Solaris or GNU systems
AC_SEARCH_LIBS([setusercontext], [util])

# Define HAVE_XXXXX macros for some system functions
AC_CHECK_FUNCS([setusercontext getgrouplist clearenv])

# The type used by getgrouplist() is the same type used by getgroups()
AC_TYPE_GETGROUPS

# Don't fail without working nasm if rfxcodec is not enabled
if test "x$enable_rfxcodec" != xyes; then
  with_simd=no
  export with_simd
fi

# Check if -ldl is needed to use dlopen()
DLOPEN_LIBS=
AC_CHECK_FUNC(dlopen, [],
              [AC_CHECK_LIB(dl, dlopen, [DLOPEN_LIBS=-ldl])])
AC_SUBST(DLOPEN_LIBS)

# checking for openssl
PKG_CHECK_MODULES([OPENSSL], [openssl >= 0.9.8], [],
  [AC_MSG_ERROR([please install libssl-dev or openssl-devel])])

# look for openssl binary
OPENSSL_BIN=`$PKG_CONFIG --variable=exec_prefix openssl`/bin
AC_PATH_PROGS([OPENSSL], [openssl], [:], [$OPENSSL_BIN:$PATH])

# checking for PAM variation
# Linux-PAM is used in Linux systems
# OpenPAM is used by FreeBSD, NetBSD, DragonFly BSD and OS X
# OpenBSD uses BSD Authentication rather than both PAMs
AC_CHECK_HEADER([security/_pam_types.h],
                [AC_DEFINE([HAVE__PAM_TYPES_H], 1, [Using Linux-PAM], [])])
AC_CHECK_HEADER([security/pam_constants.h],
                [AC_DEFINE([HAVE_PAM_CONSTANTS_H], 1, [Using OpenPAM], [])])

# Find imlib2
case "$with_imlib2" in
    '' | no) AC_MSG_NOTICE([imlib2 will not be supported])
        use_imlib2=no
        ;;
    yes)
        PKG_CHECK_MODULES([IMLIB2], [imlib2 >= 1.4.5],
            [use_imlib2=yes],
            [AC_MSG_ERROR([please install libimlib2-dev or imlib2-devel])])
        ;;
    /*) AC_MSG_CHECKING([for imlib2 in $with_imlib2])
        if test -d $with_imlib2/lib; then
            IMLIB2_LIBS="-L$with_imlib2/lib -lImlib2"
        elif test -d $with_imlib2/lib64; then
            IMLIB2_LIBS="-L$with_imlib2/lib64 -lImlib2"
        else
            AC_MSG_RESULT([no])
            AC_MSG_ERROR([Can't find libImlib2 in $with_imlib2])
        fi

        if test -f $with_imlib2/include/Imlib2.h; then
            IMLIB2_CFLAGS="-I $with_imlib2/include"
        else
            AC_MSG_RESULT([no])
            AC_MSG_ERROR([Can't find $with_imlib2/include/Imlib2.h])
        fi
        AC_MSG_RESULT([yes])
        AC_SUBST([IMLIB2_LIBS])
        AC_SUBST([IMLIB2_CFLAGS])
        use_imlib2=yes
        ;;
    *)  AC_MSG_ERROR([--with-imlib2 needs yes/no or absolute path])
esac

if test x$use_imlib2 = xyes; then
    AC_DEFINE([USE_IMLIB2],1, [Compile with imlib2 support])
fi

# Find freetype2
#
# The modversion used by pkgcheck does not correspond to the
# freetype2 release. See docs/VERSIONS.TXT in the freetype2
# source for a table of correspondences. If you change one
# of the below defines, change both.
m4_define([FT2_REQUIRED_VERSION], [2_8_0])
m4_define([FT2_REQUIRED_MODVERSION], [20.0.14])
case "$with_freetype2" in
    '' | no) AC_MSG_NOTICE([freetype2 will not be supported])
        use_freetype2=no
        ;;
    yes)
        PKG_CHECK_MODULES([FREETYPE2], [freetype2 >= FT2_REQUIRED_MODVERSION],
            [use_freetype2=yes],
            [AC_MSG_ERROR([please install version FT2_REQUIRED_VERSION or later of libfreetype6-dev or freetype-devel])])
        ;;
    /*) AC_MSG_CHECKING([for freetype2 in $with_freetype2])
        if test -d $with_freetype2/lib; then
            FREETYPE2_LIBS="-L$with_freetype2/lib -llibfreetype"
        elif test -d $with_freetype2/lib64; then
            FREETYPE2_LIBS="-L$with_freetype2/lib64 -llibfreetype"
        else
            AC_MSG_RESULT([no])
            AC_MSG_ERROR([Can't find libfreetype in $with_freetype2])
        fi

        if test -f $with_freetype2/include/freetype2/ft2build.h; then
            FREETYPE2_CFLAGS="-I $with_freetype2/include/freetype2"
        else
            AC_MSG_RESULT([no])
            AC_MSG_ERROR([Can't find $with_freetype2/include/freetype2/ft2build.h])
        fi
        AC_MSG_RESULT([yes])
        AC_SUBST([FREETYPE2_LIBS])
        AC_SUBST([FREETYPE2_CFLAGS])
        use_freetype2=yes
        ;;
    *)  AC_MSG_ERROR([--with-freetype2 needs yes/no or absolute path])
esac
AM_CONDITIONAL([USE_FREETYPE2], [test "x$use_freetype2" = xyes])

# Check only one auth mechanism is specified, and give it a name
auth_cnt=0
auth_mech="Builtin"
AUTHMOD_OBJ=verify_user.lo
AUTHMOD_LIB=-lcrypt
if test x$enable_pam = xyes
then
  auth_cnt=`expr $auth_cnt + 1`
  auth_mech="PAM"
  AUTHMOD_OBJ=verify_user_pam.lo
  AUTHMOD_LIB=-lpam
fi
if test x$bsd = xtrue
then
  auth_cnt=`expr $auth_cnt + 1`
  auth_mech="BSD"
  AUTHMOD_OBJ=verify_user_bsd.lo
  AUTHMOD_LIB=
fi
if test x$enable_kerberos = xyes
then
  auth_cnt=`expr $auth_cnt + 1`
  auth_mech="Kerberos"
  AUTHMOD_OBJ=verify_user_kerberos.lo
  AUTHMOD_LIB=-lkrb5
fi
if test x$enable_pamuserpass = xyes
then
  auth_cnt=`expr $auth_cnt + 1`
  auth_mech="PAM userpass"
  AUTHMOD_OBJ=verify_user_pam_userpass.lo
  AUTHMOD_LIB="-lpam -lpam_userpass"
fi

if test $auth_cnt -gt 1
then
  AC_MSG_ERROR([--enable-pam, --enable-bsd, --enable-pamuserpass and --enable-kerberos are mutually exclusive])
fi

AC_SUBST([AUTHMOD_OBJ])
AC_SUBST([AUTHMOD_LIB])

# checking if pam should be autodetected.
if test "x$enable_pam" = "xyes"
then
  if test -z "$enable_bsd"
  then
    AC_CHECK_HEADER([security/pam_appl.h], [],
      [AC_MSG_ERROR([please install libpam0g-dev or pam-devel])])
  fi
  if test "x$enable_pam_config" = "x"; then
    PAM_RULES="auto"
  else
    pam_config_file="$srcdir/instfiles/pam.d/xrdp-sesman.$enable_pam_config"
    if test -f "$pam_config_file"; then
      PAM_RULES="$enable_pam_config"
    else
      AC_MSG_ERROR([PAM file "$pam_config_file" is not available])
    fi
  fi
fi

AC_SUBST(PAM_RULES)

# Add define for development options to config_ac.h
AC_DEFINE([CONFIG_AC_H],1, [Allow sources to check config_ac.h is included])
if test x$devel_logging = xyes
then
  AC_DEFINE([USE_DEVEL_LOGGING],1,[Enable development logging])
fi

if test x$devel_streamcheck = xyes
then
  AC_DEFINE([USE_DEVEL_STREAMCHECK],1,[Enable development stream checking])
fi

if test "x$enable_vsock" = "xyes"
then
  enable_vsock=yes
  if test "x$freebsd" = "xyes"
  then
    # Determine if we have AF_HYPERV defined (FreeBSD 13+)
    AC_CHECK_DECL([AF_HYPERV], [AC_DEFINE([XRDP_ENABLE_VSOCK], 1, [Enable AF_HYPERV])], [], [#include <sys/socket.h>])
  else
    AC_CHECK_HEADERS([linux/socket.h linux/vm_sockets.h],
                    [AC_DEFINE([XRDP_ENABLE_VSOCK], 1, [Enable AF_VSOCK])],
                    [],
                    [#include <sys/socket.h>])
  fi
fi

if test "x$enable_ipv6only" = "xyes"
then
  enable_ipv6=yes
  AC_DEFINE([XRDP_ENABLE_IPV6ONLY],1,[Enable IPv6 only])
fi

if test "x$enable_ipv6" = "xyes"
then
  AC_DEFINE([XRDP_ENABLE_IPV6],1,[Enable IPv6])
fi

AS_IF( [test "x$enable_neutrinordp" = "xyes"] , [PKG_CHECK_MODULES(FREERDP, freerdp >= 1.0.0)] )

# checking for libjpeg
if test "x$enable_jpeg" = "xyes"
then
  AC_CHECK_HEADER([jpeglib.h], [],
    [AC_MSG_ERROR([please install libjpeg-dev or libjpeg-devel])])
fi

# checking for fuse
if test "x$enable_fuse" = "xyes"
then
  PKG_CHECK_MODULES([FUSE], [fuse >= 2.6], [],
    [AC_MSG_ERROR([please install libfuse-dev or fuse-devel])])
fi

# checking for fdk aac
if test "x$enable_fdkaac" = "xyes"
then
  PKG_CHECK_MODULES([FDKAAC], [fdk-aac >= 0.1.0], [],
    [AC_MSG_ERROR([please install libfdk-aac-dev or fdk-aac-devel])])
fi

# checking for opus
if test "x$enable_opus" = "xyes"
then
  AC_CHECK_HEADER([opus/opus.h], [],
    [AC_MSG_ERROR([please install libopus-dev or opus-devel])])
fi

# checking for lame mp3
if test "x$enable_mp3lame" = "xyes"
then
  AC_CHECK_HEADER([lame/lame.h], [],
    [AC_MSG_ERROR([please install libmp3lame-dev or lamemp3-devel])])
fi

AS_IF( [test "x$enable_pixman" = "xyes"] , [PKG_CHECK_MODULES(PIXMAN, pixman-1 >= 0.1.0)] )

# checking for TurboJPEG
if test "x$enable_tjpeg" = "xyes"
then
if test ! -z "$TURBOJPEG_PATH"
then
  # env var TURBOJPEG_PATH has been defined, use that
  AC_CHECK_HEADER([$TURBOJPEG_PATH/include/turbojpeg.h], [],
    [AC_MSG_ERROR([could not find TurboJPEG in dir specified by env variable TURBOJPEG_PATH ($TURBOJPEG_PATH)])])

   AC_SUBST(TurboJpegIncDir, ["-I$TURBOJPEG_PATH/include"])
   AC_SUBST(TurboJpegLibDir, ["-L$TURBOJPEG_PATH/lib -Wl,-rpath -Wl,$TURBOJPEG_PATH/lib"])
elif test -e /opt/libjpeg-turbo/lib64
then
  # TurboJPEG has been installed to /opt on a 64 bit m/c
  AC_SUBST(TurboJpegIncDir, ["-I/opt/libjpeg-turbo/include"])
  AC_SUBST(TurboJpegLibDir, ["-L/opt/libjpeg-turbo/lib64 -Wl,-rpath -Wl,/opt/libjpeg-turbo/lib64"])
elif test -e /opt/libjpeg-turbo/lib32
then
  # TurboJPEG has been installed to /opt on a 32 bit m/c
  AC_SUBST(TurboJpegIncDir, ["-I/opt/libjpeg-turbo/include"])
  AC_SUBST(TurboJpegLibDir, ["-L/opt/libjpeg-turbo/lib32 -Wl,-rpath -Wl,/opt/libjpeg-turbo/lib32"])
else
  # check in default location
  AC_CHECK_HEADER([/usr/include/turbojpeg.h], [],
    [AC_MSG_ERROR([please install TurboJPEG ])])
  AC_SUBST(TurboJpegIncDir, [""])
  AC_SUBST(TurboJpegLibDir, [""])
fi
fi

AC_PATH_XTRA
if test "x$no_x" == "xyes"; then
  AC_MSG_ERROR([please install libx11-dev or libX11-devel])
fi

save_CFLAGS="$CFLAGS"
CFLAGS="$CFLAGS $X_CFLAGS"

# checking for Xfixes
AC_CHECK_HEADER([X11/extensions/Xfixes.h], [],
  [AC_MSG_ERROR([please install libxfixes-dev or libXfixes-devel])],
  [#include <X11/Xlib.h>])

# checking for Xrandr
AC_CHECK_HEADER([X11/extensions/Xrandr.h], [],
  [AC_MSG_ERROR([please install libxrandr-dev or libXrandr-devel])],
  [#include <X11/Xlib.h>])

if test "x$enable_utmp" = "xyes"
then
    AC_CHECK_HEADERS(utmp.h utmpx.h)

    # Test for non-standard extensions in struct utmpx
    AXRDP_CHECK_UTMPX_MEMBER_EXISTS([ut_host], [HAVE_UTMPX_UT_HOST])
    AXRDP_CHECK_UTMPX_MEMBER_EXISTS([ut_exit], [HAVE_UTMPX_UT_EXIT])
fi

CFLAGS="$save_CFLAGS"

# perform unit tests if libcheck and libmocka found
perform_unit_tests=yes; # Assume packages will be found
if test "x$ensure_tests_deps" == "xyes"; then
    PKG_CHECK_MODULES([CHECK], [check >= 0.10.0],
        [],
        [AC_MSG_ERROR([please install check, the unit test framework])])
    # Check if cmocka is available - needed for unit testing
    PKG_CHECK_MODULES([CMOCKA], [cmocka],
        [],
        [AC_MSG_ERROR([please install cmocka, the mocking framework])])
else
    PKG_CHECK_MODULES([CHECK], [check >= 0.10.0],
        [],
        [perform_unit_tests=no])
    PKG_CHECK_MODULES([CMOCKA], [cmocka],
        [],
        [perform_unit_tests=no])
fi

if test "x$perform_unit_tests" == "xyes"; then
    AC_MSG_NOTICE([libcheck found, unit tests will be performed])
else
    AC_MSG_NOTICE([libcheck not found, unit tests will be skipped])
fi
# -- end perform unit tests

AC_SUBST([moduledir], '${libdir}/xrdp')

AC_ARG_ENABLE([strict-locations],
  [AS_HELP_STRING([--enable-strict-locations],
                  [Use standard Autoconf install directories unless overridden
                   (default: use /etc and /var)])],
                  [], [enable_strict_locations=no])

if test "x$enable_strict_locations" != "xyes"; then
  sysconfdir="/etc";
  localstatedir="/var";
fi

AC_ARG_WITH([pamconfdir],
  [AS_HELP_STRING([--with-pamconfdir=DIR],
                  [Use directory for pam.d config (default: /etc/pam.d)])],
                  [], [with_pamconfdir="$sysconfdir/pam.d"])
AC_SUBST([pamconfdir], [$with_pamconfdir])

PKG_INSTALLDIR

AC_CHECK_HEADERS([sys/prctl.h uchar.h])

AC_CONFIG_FILES([
  common/Makefile
  docs/Makefile
  docs/man/Makefile
  fontutils/Makefile
  genkeymap/Makefile
  instfiles/default/Makefile
  instfiles/init.d/Makefile
  instfiles/Makefile
  instfiles/pam.d/Makefile
  instfiles/pulse/Makefile
  instfiles/rc.d/Makefile
  keygen/Makefile
  waitforx/Makefile
  libipm/Makefile
  libxrdp/Makefile
  Makefile
  mc/Makefile
  neutrinordp/Makefile
  ulalaca/Makefile
  pkgconfig/Makefile
  pkgconfig/xrdp.pc
  pkgconfig/xrdp-uninstalled.pc
  sesman/libsesman/Makefile
  sesman/chansrv/Makefile
  sesman/Makefile
  sesman/sesexec/Makefile
  sesman/tools/Makefile
  tests/Makefile
  tests/common/Makefile
  tests/libipm/Makefile
  tests/libxrdp/Makefile
  tests/memtest/Makefile
  tests/xrdp/Makefile
  tools/Makefile
  tools/devel/Makefile
  tools/devel/tcp_proxy/Makefile
  vnc/Makefile
  xrdpapi/Makefile
  xrdp/Makefile
  xrdpvr/Makefile
  xup/Makefile
  third_party/Makefile
  third_party/tomlc99/Makefile
])

AC_REQUIRE_AUX_FILE([tap-driver.sh])
AC_OUTPUT

echo ""
echo "xrdp will be compiled with:"
echo ""
echo "  mp3lame                 $enable_mp3lame"
echo "  opus                    $enable_opus"
echo "  fdkaac                  $enable_fdkaac"
echo "  jpeg                    $enable_jpeg"
echo "  turbo jpeg              $enable_tjpeg"
echo "  rfxcodec                $enable_rfxcodec"
echo "  painter                 $enable_painter"
echo "  pixman                  $enable_pixman"
echo "  fuse                    $enable_fuse"
echo "  ipv6                    $enable_ipv6"
echo "  ipv6only                $enable_ipv6only"
echo "  vsock                   $enable_vsock"
echo "  auth mechanism          $auth_mech"
echo "  rdpsndaudin             $enable_rdpsndaudin"
echo "  utmp support            $enable_utmp"
if test x$enable_utmp = xyes; then
    echo "    utmpx.ut_host         $ac_cv_utmpx_has_ut_host"
    echo "    utmpx.ut_exit         $ac_cv_utmpx_has_ut_exit"
fi

echo
echo "  with imlib2             $use_imlib2"
echo "  with freetype2          $use_freetype2"

echo
echo "  development logging     $devel_logging"
echo "  development streamcheck $devel_streamcheck"
echo ""
echo "  strict_locations        $enable_strict_locations"
echo "  prefix                  $prefix"
echo "  exec_prefix             $exec_prefix"
echo "  libdir                  $libdir"
echo "  bindir                  $bindir"
echo "  sysconfdir              $sysconfdir"
echo "  pamconfdir              $pamconfdir"
echo "  localstatedir           $localstatedir"
echo "  runstatedir             $runstatedir"
echo "  socketdir               $socketdir"
echo ""
echo "  unit tests performable  $perform_unit_tests"
echo ""
echo "  CFLAGS = $CFLAGS"
echo "  LDFLAGS = $LDFLAGS"

# xrdp_configure_options.h will be written to the build directory, not the source directory
echo '#define XRDP_CONFIGURE_OPTIONS \' > ./xrdp_configure_options.h
./config.status --config | xargs -n 1 | sed -e 's/^/"      /' -e 's/$/\\n" \\/' >> ./xrdp_configure_options.h
echo '""' >> ./xrdp_configure_options.h
