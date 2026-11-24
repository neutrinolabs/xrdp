AM_CPPFLAGS = \
  -I$(top_srcdir)/common

EXTRA_DIST = xrdp_accel_assist_shaders.c

XRDP_EXTRA_LIBS =
XRDP_EXTRA_SOURCES =

if XRDP_NVENC
AM_CPPFLAGS += -DXRDP_NVENC
AM_CPPFLAGS += $(XRDP_NVENC_CFLAGS)
XRDP_EXTRA_LIBS += $(XRDP_NVENC_LIBS)
XRDP_EXTRA_SOURCES += xrdp_accel_assist_nvenc.c xrdp_accel_assist_nvenc.h encoder_headers/nvEncodeAPI_11_1.h
endif

if XRDP_VAAPI
AM_CPPFLAGS += -DXRDP_VAAPI
AM_CPPFLAGS += $(XRDP_VAAPI_CFLAGS)
XRDP_EXTRA_LIBS += $(XRDP_VAAPI_LIBS)
XRDP_EXTRA_SOURCES += xrdp_accel_assist_vaapi.c xrdp_accel_assist_vaapi.h
endif

pkglibexec_PROGRAMS = \
  xrdp-accel-assist

xrdp_accel_assist_SOURCES = \
  xrdp_accel_assist.c \
  xrdp_accel_assist.h \
  xrdp_accel_assist_x11.c \
  xrdp_accel_assist_x11.h \
  xrdp_accel_assist_egl.c \
  xrdp_accel_assist_egl.h \
  xrdp_accel_assist_glx.c \
  xrdp_accel_assist_glx.h \
  $(XRDP_EXTRA_SOURCES)

xrdp_accel_assist_LDADD = \
  $(top_builddir)/common/libcommon.la \
  $(XRDP_EXTRA_LIBS) \
  -lX11 -lepoxy

