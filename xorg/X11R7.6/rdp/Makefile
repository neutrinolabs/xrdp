
#X11RDPBASE is an environment variable that needs to be set

INCBASE = $(X11RDPBASE)/include
LIBBASE = $(X11RDPBASE)/lib

XSRCBASE = ../build_dir/xorg-server-1.9.3

OBJS = rdpmain.o rdpdraw.o rdpinput.o rdpmisc.o rdpup.o miinitext.o

# fbcmap.o

LIBS = $(XSRCBASE)/dbe/.libs/libdbe.a \
	$(XSRCBASE)/dix/.libs/libdix.a \
	$(XSRCBASE)/dix/.libs/libmain.a \
	$(XSRCBASE)/fb/.libs/libfb.a \
	$(XSRCBASE)/mi/.libs/libmi.a \
	$(XSRCBASE)/os/.libs/libos.a \
	$(XSRCBASE)/randr/.libs/librandr.a \
	$(XSRCBASE)/record/.libs/librecord.a \
	$(XSRCBASE)/render/.libs/librender.a \
	$(XSRCBASE)/xkb/.libs/libxkb.a \
	$(XSRCBASE)/Xext/.libs/libXext.a \
	$(XSRCBASE)/Xi/.libs/libXi.a \
	$(XSRCBASE)/glx/.libs/libglx.a \
	$(XSRCBASE)/xfixes/.libs/libxfixes.a \
	librdp.a \
	-lfreetype -lz -lm -lXfont -lXau -lXdmcp -lpixman-1

CFLAGS = -O2 -Wall -fno-strength-reduce \
	-I../../include \
	-I../../cfb \
	-I../../mfb \
	-I../../mi \
	-I$(INCBASE) \
	-I$(INCBASE)/X11 \
	-I$(INCBASE)/X11/fonts \
	-I$(INCBASE)/X11/extensions \
	-I$(INCBASE)/xorg \
	-I$(INCBASE)/pixman-1 \
	-I../../os \
	-I../../render \
	-I../xfree86/common \
	-I../xfree86/os-support \
	-D_POSIX_C_SOURCE=199309L -D_SVID_SOURCE -D_REENTRANT \
	-DGLX_USE_MESA -DXRECORD -D_GNU_SOURCE -DXAPPGROUP \
	-DXCSECURITY -DTOGCUP -DSINGLEDEPTH -DXFree86Server \
	-DDBE -DEVI -DXVMC -DFONTCACHE -DGCCUSESGAS -DSTATIC_COLOR \
	-DAVOID_GLYPHBLT -DFUNCPROTO=15 -DNARROWPROTO -DDDXOSFATALERROR \
	-DPART_NET -DDDXTIME -D_HAVE_XALLOC_DECLS \
	\
	-DXFIXES \
	-DSMART_SCHEDULE -DSERVER_LOCK -DGLXEXT -DSHAPE -DRENDER -DRANDR \
	-DBIGREQS -D_POSIX_SOURCE -D_BSD_SOURCE -DSCREENSAVER -DXF86BIGFONT \
	-DXCMISC -DXTEST -DXTRAP -DXV -DXSYNC -DMITMISC -DPANORAMIX \
	-DDPMSExtension -DXvExtension -DXvMCExtension -DXResExtension \
	-DMITSHM -DPIXPRIV -DNDEBUG -DDDXOSINIT -DXKB -DXINPUT

# -pedantic

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
	$(CC) $(LDFLAGS) -o X11rdp $(LIBS) $(LIBS)

clean:
	rm -f $(OBJS) librdp.a

miinitext.o: ../build_dir/xorg-server-1.9.3/mi/miinitext.c
	$(CC) $(CFLAGS) -I../build_dir/xorg-server-1.9.3/Xext -c ../build_dir/xorg-server-1.9.3/mi/miinitext.c

#fbcmap.o: ../../fb/fbcmap.c
#	$(CC) $(CFLAGS) -c ../../fb/fbcmap.c
