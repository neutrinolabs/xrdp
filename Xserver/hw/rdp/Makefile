
OBJS = rdpmain.o rdpdraw.o rdpinput.o rdpmisc.o rdpup.o miinitext.o

CFLAGS = -O2 -fno-strength-reduce -ansi -pedantic
CFLAGS += -I. -I../../../../exports/include/X11
CFLAGS += -I../../../../include/fonts
CFLAGS += -I../../../../exports/include/X11
CFLAGS += -I../../cfb -I../../mfb -I../../mi
CFLAGS += -I../../include -I../../os
CFLAGS += -I../../../../../include
CFLAGS += -I/usr/local/include
CFLAGS += -I../../../.././ -I../../../../exports/include
CFLAGS += -I../../../../include/extensions
CFLAGS += -I../../dbe
CFLAGS += -I../../record
CFLAGS += -I../../render
CFLAGS += -Dlinux -D__i386__
CFLAGS += -D_POSIX_C_SOURCE=199309L
CFLAGS += -D_POSIX_SOURCE -D_XOPEN_SOURCE=500L
CFLAGS += -D_BSD_SOURCE -D_SVID_SOURCE -D_REENTRANT

CFLAGS += -DSHAPE -DBIGREQS -DGLXEXT -DGLX_USE_MESA -DMITMISC -DXTEST
CFLAGS += -DPANORAMIX -DRANDR -DRENDER -DXCMISC -DXSYNC -DXRECORD

CFLAGS += -D_GNU_SOURCE -DLBX -DXAPPGROUP -DXCSECURITY -DTOGCUP
CFLAGS += -DXF86BIGFONT -DDPMSExtension -DSINGLEDEPTH -DXvExtension
CFLAGS += -DXFree86Server -DXvMCExtension -DSMART_SCHEDULE -DXResExtension
CFLAGS += -DX_BYTE_ORDER=X_LITTLE_ENDIAN -DXTRAP -DMITSHM -DDBE -DEVI
CFLAGS += -DSCREENSAVER -DXV -DXVMC -DFONTCACHE

#CFLAGS += -DXINPUT -DXKB

CFLAGS += -DGCCUSESGAS -DSTATIC_COLOR -DAVOID_GLYPHBLT -DPIXPRIV
CFLAGS += -DNDEBUG -DFUNCPROTO=15 -DNARROWPROTO -DDDXOSINIT
CFLAGS += -DSERVER_LOCK -DDDXOSFATALERROR -DPART_NET

CFLAGS += -DDDXTIME
CFLAGS += -D_HAVE_XALLOC_DECLS

#CFLAGS += -DXVNCRELEASE=\"tight1.2.9\"

all: rdp

rdp: $(OBJS)
	$(AR) rvu librdp.a $(OBJS)
	ranlib librdp.a

clean:
	rm -f $(OBJS) librdp.a


miinitext.o: ../../mi/miinitext.c
	$(CC) $(CFLAGS) -c ../../mi/miinitext.c
