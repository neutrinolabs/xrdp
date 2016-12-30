# X11RDPBASE is the top-level X11rdp install directory
ifeq ($(X11RDPBASE),)
$(error X11RDPBASE needs to be set)
endif

BINBASE = $(X11RDPBASE)/bin
INCBASE = $(X11RDPBASE)/include
LIBBASE = $(X11RDPBASE)/lib

INSTALL = /usr/bin/install -c
XSRCBASE = ../build_dir/xorg-server-1.9.3

OBJS = rdpmain.o rdpdraw.o rdpinput.o rdpmisc.o rdpup.o rdprandr.o \
rdpCopyArea.o rdpPolyFillRect.o rdpPutImage.o rdpPolyRectangle.o \
rdpPolylines.o rdpPolySegment.o rdpFillSpans.o rdpSetSpans.o \
rdpCopyPlane.o rdpPolyPoint.o rdpPolyArc.o rdpFillPolygon.o \
rdpPolyFillArc.o rdpPolyText8.o rdpPolyText16.o \
rdpImageText8.o rdpImageText16.o rdpImageGlyphBlt.o rdpPolyGlyphBlt.o \
rdpPushPixels.o rdpxv.o rdpglyph.o rdpComposite.o \
rdpkeyboard.o rdpkeyboardevdev.o rdpkeyboardbase.o \
miinitext.o \
fbcmap_mi.o

# in Xorg 7.1, fbcmap.c was used but now it looks like fbcmap_mi.c should
# be used
#fbcmap_mi.o
#fbcmap.o

LIBS = \
	$(XSRCBASE)/dix/.libs/libmain.a \
	librdp.a \
	$(XSRCBASE)/dbe/.libs/libdbe.a \
	$(XSRCBASE)/dix/.libs/libdix.a \
	$(XSRCBASE)/fb/.libs/libfb.a \
	$(XSRCBASE)/mi/.libs/libmi.a \
	$(XSRCBASE)/randr/.libs/librandr.a \
	$(XSRCBASE)/render/.libs/librender.a \
	$(XSRCBASE)/os/.libs/libos.a \
	$(XSRCBASE)/record/.libs/librecord.a \
	$(XSRCBASE)/xkb/.libs/libxkb.a \
	$(XSRCBASE)/Xext/.libs/libXext.a \
	$(XSRCBASE)/Xi/.libs/libXi.a \
	$(XSRCBASE)/glx/.libs/libglx.a \
	$(XSRCBASE)/xfixes/.libs/libxfixes.a \
	$(XSRCBASE)/damageext/.libs/libdamageext.a \
	$(XSRCBASE)/miext/damage/.libs/libdamage.a

LLIBS = -Wl,-rpath=$(LIBBASE) -lfreetype -lz -lm -lXfont -lXau \
	-lXdmcp -lpixman-1 -lrt -ldl -lcrypto -lGL -lXdamage

CFLAGS = -g -O2 -Wall -fno-strength-reduce \
	-I$(INCBASE) \
	-I$(INCBASE)/X11 \
	-I$(INCBASE)/X11/fonts \
	-I$(INCBASE)/X11/extensions \
	-I$(INCBASE)/xorg \
	-I$(INCBASE)/pixman-1 \
	-I../../../common \
	-D_POSIX_C_SOURCE=199309L -D_SVID_SOURCE -D_REENTRANT \
	-DGLX_USE_MESA -DXRECORD -D_GNU_SOURCE -DXAPPGROUP \
	-DTOGCUP -DSINGLEDEPTH -DXFree86Server \
	-DDBE -DEVI -DXVMC -DFONTCACHE -DGCCUSESGAS -DSTATIC_COLOR \
	-DAVOID_GLYPHBLT -DFUNCPROTO=15 -DNARROWPROTO -DDDXOSFATALERROR \
	-DPART_NET -DDDXTIME -D_HAVE_XALLOC_DECLS \
	-DDAMAGE \
	-DXFIXES \
	-DSMART_SCHEDULE -DSERVER_LOCK -DGLXEXT -DSHAPE -DRENDER -DRANDR \
	-DBIGREQS -D_POSIX_SOURCE -D_BSD_SOURCE -DSCREENSAVER \
	-DXCMISC -DXTEST -DXTRAP -DXV -DXSYNC -DMITMISC -DPANORAMIX \
	-DDPMSExtension -DXvExtension -DXvMCExtension -DXResExtension \
	-DMITSHM -DPIXPRIV -DNDEBUG -DDDXOSINIT -DXKB -DXINPUT

#-DDAMAGE

# -pedantic
# -DXCSECURITY -DXF86BIGFONT

# these are defined in xorg-server.h
# -D_XOPEN_SOURCE=500L
# -DX_BYTE_ORDER=X_LITTLE_ENDIAN
# -DSMART_SCHEDULE -DSERVER_LOCK -DGLXEXT -DSHAPE -DRENDER -DRANDR
# -DBIGREQS -D_POSIX_SOURCE -D_BSD_SOURCE -DSCREENSAVER -DXF86BIGFONT
# -DXCMISC -DXTEST -DXTRAP -DXV -DXSYNC -DMITMISC -DPANORAMIX
# -DDPMSExtension -DXvExtension -DXvMCExtension -DXResExtension
# -DMITSHM -DPIXPRIV -DNDEBUG -DDDXOSINIT

# these are not needed I think
# -Dlinux -D__i386__

LDFLAGS = -L$(LIBBASE) -Wl,-rpath,$(LIBBASE)

all: X11rdp

X11rdp: $(OBJS)
	$(AR) rvu librdp.a $(OBJS)
	ranlib librdp.a
	$(CC) $(LDFLAGS) -o X11rdp $(LIBS) $(LLIBS)

clean:
	rm -f $(OBJS) librdp.a

miinitext.o: ../build_dir/xorg-server-1.9.3/mi/miinitext.c Makefile
	$(CC) $(CFLAGS) -I../build_dir/xorg-server-1.9.3/Xext -c ../build_dir/xorg-server-1.9.3/mi/miinitext.c

fbcmap.o: ../build_dir/xorg-server-1.9.3/fb/fbcmap.c
	$(CC) $(CFLAGS) -c ../build_dir/xorg-server-1.9.3/fb/fbcmap.c

fbcmap_mi.o: ../build_dir/xorg-server-1.9.3/fb/fbcmap_mi.c
	$(CC) $(CFLAGS) -c ../build_dir/xorg-server-1.9.3/fb/fbcmap_mi.c

install: all
	$(INSTALL) X11rdp $(BINBASE)/X11rdp
