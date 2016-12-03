ACLOCAL_AMFLAGS = -I m4
AM_DISTCHECK_CONFIGURE_FLAGS = --without-systemdsystemunitdir

EXTRA_DIST = bootstrap COPYING coding_style.md design.txt faq-compile.txt \
  faq-general.txt file-loc.txt install.txt m4 readme.txt

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

SUBDIRS = \
  libpainter \
  common \
  vnc \
  rdp \
  xup \
  mc \
  $(NEUTRINORDPDIR) \
  libxrdp \
  xrdp \
  sesman \
  keygen \
  docs \
  instfiles \
  genkeymap \
  xrdpapi \
  pkgconfig \
  $(XRDPVRDIR)
