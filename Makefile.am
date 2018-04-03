ACLOCAL_AMFLAGS = -I m4
AM_DISTCHECK_CONFIGURE_FLAGS = \
  --without-systemdsystemunitdir \
  --enable-strict-locations

# There're mixed contents automake and non-automake files/directories
# under tests/ directory. List only non-automakes EXTRA_DIST.
EXTRA_DIST = \
  COPYING \
  README.md \
  NEWS.md \
  astyle_config.as \
  bootstrap \
  coding_style.md \
  description-pak \
  design.txt \
  faq-compile.txt \
  faq-general.txt \
  file-loc.txt \
  fontdump \
  install.txt \
  m4 \
  postinstall-pak \
  tcutils \
  tests \
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

SUBDIRS = \
  common \
  vnc \
  xup \
  mc \
  $(NEUTRINORDPDIR) \
  libxrdp \
  $(PAINTERDIR) \
  $(RFXCODECDIR) \
  xrdp \
  sesman \
  keygen \
  docs \
  instfiles \
  genkeymap \
  xrdpapi \
  pkgconfig \
  $(XRDPVRDIR) \
  tests

distclean-local:
	-rm -f xrdp_configure_options.h
