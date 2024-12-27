#EXTRA_DIST = \
  #xrdp_surface.c

AM_CPPFLAGS = \
  -DXRDP_PAMCONF_PATH=\"${pamconfdir}\" \
  -DXRDP_CFG_PATH=\"${sysconfdir}/${sysconfsubdir}\" \
  -DXRDP_SOCKET_ROOT_PATH=\"${socketdir}\" \
  -I$(top_srcdir)/libipm \
  -I$(top_srcdir)/common
#  -DXRDP_SBIN_PATH=\"${sbindir}\" \
#  -DXRDP_SHARE_PATH=\"${datadir}/xrdp\" \
#  -DXRDP_PID_PATH=\"${localstatedir}/run\"

#AM_CFLAGS = $(OPENSSL_CFLAGS)

#AM_LDFLAGS =

#LIBXRDP_EXTRA_LIBS =

#if XRDP_NEUTRINORDP
#AM_CPPFLAGS += -DXRDP_NEUTRINORDP
#LIBXRDP_EXTRA_LIBS += $(FREERDP_LIBS)
#endif

#if XRDP_RFXCODEC
#AM_CPPFLAGS += -DXRDP_RFXCODEC
#endif

#if XRDP_TJPEG
#AM_CPPFLAGS += -DXRDP_JPEG -DXRDP_TJPEG @TurboJpegIncDir@
#AM_LDFLAGS += @TurboJpegLibDir@
#LIBXRDP_EXTRA_LIBS += -lturbojpeg
#else
#if XRDP_JPEG
#AM_CPPFLAGS += -DXRDP_JPEG
#LIBXRDP_EXTRA_LIBS += -ljpeg
#endif
#endif

module_LTLIBRARIES = \
  libsesman.la

# Possible authentication modules
# See https://www.gnu.org/software/automake/manual/html_node/Conditional-Sources.html
EXTRA_libsesman_la_SOURCES = \
 verify_user.c \
 verify_user_bsd.c \
 verify_user_kerberos.c \
 verify_user_pam.c \
 verify_user_pam_userpass.c

# Make sure the right authentication module is pulled in
libsesman_la_DEPENDENCIES = $(AUTHMOD_OBJ)

libsesman_la_SOURCES = \
  sesman_access.h \
  sesman_access.c \
  sesman_auth.h \
  sesman_config.h \
  sesman_config.c \
  sesman_clip_restrict.h \
  sesman_clip_restrict.c

libsesman_la_LIBADD = \
  $(AUTHMOD_OBJ) \
  $(top_builddir)/libipm/libipm.la \
  $(top_builddir)/common/libcommon.la \
  $(AUTHMOD_LIB)
