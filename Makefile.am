ACLOCAL_AMFLAGS = -I m4
# Redefine the localstatedir to stop github CI defaulting to
# something like /home/runner/work/xrdp/xrdp/xrdp-0.10.80/_inst/var
#
# A path this long (50 chars) can trigger some -Werror=format-truncation
# errors when using the XRDP_SOCKET_ROOT_PATH macro to create a Unix
# domain socket name, as these names have a quite short length (108
# characters on Linux).
AM_DISTCHECK_CONFIGURE_FLAGS = \
  --localstatedir=/distcheckdummy/var \
  --without-systemdsystemunitdir \
  --enable-strict-locations \
  --enable-tests

EXTRA_DIST = \
  COPYING \
  README.md \
  NEWS.md \
  astyle_config.as \
  bootstrap \
  coding_style.md \
  m4 \
  vrplayer

if XRDP_NEUTRINORDP
NEUTRINORDPDIR = neutrinordp
else
NEUTRINORDPDIR =
endif

if XRDP_XRDPVR
XRDPVRDIR = xrdpvr
else
XRDPVRDIR =
endif

if XRDP_PAINTER
PAINTERDIR = libpainter
else
PAINTERDIR =
endif

if XRDP_RFXCODEC
RFXCODECDIR = librfxcodec
else
RFXCODECDIR =
endif

if XRDP_ACCEL
ACCELDIR = xrdp_accel_assist
else
ACCELDIR =
endif

# This should not be dictionary order but build order
SUBDIRS = \
  third_party \
  third_party/tomlc99 \
  common \
  vnc \
  xup \
  mc \
  $(NEUTRINORDPDIR) \
  libipm \
  libxrdp \
  $(PAINTERDIR) \
  $(RFXCODECDIR) \
  sesman \
  xrdp \
  fontutils \
  fuzz \
  keygen \
  waitforx \
  docs \
  instfiles \
  genkeymap \
  xrdpapi \
  pkgconfig \
  $(XRDPVRDIR) \
  tests \
  tools \
  $(ACCELDIR)

distclean-local:
	-rm -f xrdp_configure_options.h
