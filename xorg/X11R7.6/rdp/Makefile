
#X11RDPBASE is an environment variable that needs to be set

INCBASE = $(X11RDPBASE)/include
LIBBASE = $(X11RDPBASE)/lib

OBJS = rdpmain.o rdpdraw.o rdpinput.o rdpmisc.o rdpup.o miinitext.o fbcmap.o

LIBS = ../../dbe/.libs/libdbe.a \
	../../dix/.libs/libdix.a \
	../../fb/.libs/libfb.a \
	../../mfb/.libs/libmfb.a \
	../../mi/.libs/libmi.a \
	../../os/.libs/libos.a \
	../../randr/.libs/librandr.a \
	../../record/.libs/librecord.a \
	../../render/.libs/librender.a \
	../../xkb/.libs/libxkb.a \
	../../XTrap/.libs/libxtrap.a \
	../../Xext/.libs/libXext.a \
	../../Xi/.libs/libXi.a \
	../../GL/glx/.libs/libglx.a \
	../../GL/mesa/.libs/libGLcore.a \
	../../xfixes/.libs/libxfixes.a \
	librdp.a \
	-lfreetype -lz -lm -lXfont -lXau -lXdmcp

CFLAGS = -O2 -fno-strength-reduce \
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

miinitext.o: ../../mi/miinitext.c
	$(CC) $(CFLAGS) -c ../../mi/miinitext.c

fbcmap.o: ../../fb/fbcmap.c
	$(CC) $(CFLAGS) -c ../../fb/fbcmap.c
