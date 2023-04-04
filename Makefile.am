ACLOCAL_AMFLAGS = -I m4
AM_DISTCHECK_CONFIGURE_FLAGS = \
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

if XRDP_ULALACA
ULALACADIR = ulalaca
else
ULALACADIR =
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
  keygen \
  waitforx \
  docs \
  instfiles \
  genkeymap \
  xrdpapi \
  pkgconfig \
  $(XRDPVRDIR) \
  $(ULALACADIR) \
  tests \
  tools

distclean-local:
	-rm -f xrdp_configure_options.h
