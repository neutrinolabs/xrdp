EXTRA_DIST = bootstrap COPYING design.txt faq-compile.txt faq-general.txt file-loc.txt install.txt prog_std.txt readme.txt


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
  $(XRDPVRDIR)
