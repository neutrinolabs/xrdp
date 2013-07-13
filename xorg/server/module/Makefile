
OBJS = rdpDraw.o rdpPri.o rdpGC.o rdpFillSpans.o rdpSetSpans.o rdpPutImage.o \
rdpCopyArea.o rdpCopyPlane.o rdpPolyPoint.o rdpPolylines.o rdpPolySegment.o \
rdpPolyRectangle.o rdpPolyArc.o rdpFillPolygon.o rdpPolyFillRect.o \
rdpPolyFillArc.o rdpPolyText8.o rdpPolyText16.o rdpImageText8.o \
rdpImageText16.o rdpImageGlyphBlt.o rdpPolyGlyphBlt.o rdpPushPixels.o

CFLAGS = -O2 -Wall -I/usr/include/xorg -I/usr/include/pixman-1

LDFLAGS =

LIBS =

all: xrdpdev_drv.so

xrdpdev_drv.so: $(OBJS) Makefile
	$(CC) -shared -o libxorgxrdp.so $(LDFLAGS) $(OBJS) $(LIBS)

clean:
	rm -f $(OBJS) libxorgxrdp.so
