AM_CPPFLAGS = \
  -DXRDP_CFG_PATH=\"${sysconfdir}/${sysconfsubdir}\" \
  -DXRDP_SBIN_PATH=\"${sbindir}\" \
  -DXRDP_SHARE_PATH=\"${datadir}/xrdp\" \
  -DXRDP_PID_PATH=\"${localstatedir}/run\" \
  -I$(top_srcdir)/common \
  -I$(builddir)

AM_CFLAGS = $(WAYLAND_CLIENT_CFLAGS) $(XKBCOMMON_CFLAGS)

if XRDP_WLUP_MUTTER
AM_CPPFLAGS += -DXRDP_WLUP_MUTTER
AM_CFLAGS += $(WLUP_MUTTER_CFLAGS)
MUTTER_SOURCES = wlup_mutter.c wlup_mutter.h
MUTTER_LIBS = $(WLUP_MUTTER_LIBS)
else
MUTTER_SOURCES =
MUTTER_LIBS =
endif

# ext-foreign-toplevel-list-v1 is only needed because
# ext-image-capture-source-v1 refers to one of its interfaces
PROTOCOLS = \
  ext-foreign-toplevel-list-v1 \
  ext-image-capture-source-v1 \
  ext-image-copy-capture-v1 \
  virtual-keyboard-unstable-v1 \
  wlr-output-management-unstable-v1 \
  wlr-virtual-pointer-unstable-v1

PROTOCOL_H_FILES = $(PROTOCOLS:=-client-protocol.h)
PROTOCOL_C_FILES = $(PROTOCOLS:=-protocol.c)

BUILT_SOURCES = $(PROTOCOL_H_FILES) $(PROTOCOL_C_FILES)
CLEANFILES = $(BUILT_SOURCES)

EXTRA_DIST = $(PROTOCOLS:%=protocols/%.xml)

%-client-protocol.h: $(srcdir)/protocols/%.xml
	$(WAYLAND_SCANNER) client-header $< $@

%-protocol.c: $(srcdir)/protocols/%.xml
	$(WAYLAND_SCANNER) private-code $< $@

module_LTLIBRARIES = \
  libwlup.la

libwlup_la_SOURCES = \
  wlup.c \
  wlup.h \
  $(MUTTER_SOURCES)

EXTRA_libwlup_la_SOURCES = wlup_mutter.c wlup_mutter.h

nodist_libwlup_la_SOURCES = $(PROTOCOL_C_FILES) $(PROTOCOL_H_FILES)

libwlup_la_LIBADD = \
  $(top_builddir)/common/libcommon.la \
  $(WAYLAND_CLIENT_LIBS) \
  $(XKBCOMMON_LIBS) \
  $(MUTTER_LIBS)

libwlup_la_LDFLAGS = -avoid-version -module
