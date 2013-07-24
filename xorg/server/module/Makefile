
OBJS = rdpDraw.o rdpPri.o rdpGC.o rdpFillSpans.o rdpSetSpans.o rdpPutImage.o \
rdpCopyArea.o rdpCopyPlane.o rdpPolyPoint.o rdpPolylines.o rdpPolySegment.o \
rdpPolyRectangle.o rdpPolyArc.o rdpFillPolygon.o rdpPolyFillRect.o \
rdpPolyFillArc.o rdpPolyText8.o rdpPolyText16.o rdpImageText8.o \
rdpImageText16.o rdpImageGlyphBlt.o rdpPolyGlyphBlt.o rdpPushPixels.o \
rdpCursor.o rdpMain.o rdpRandR.o rdpMisc.o rdpReg.o \
rdpComposite.o rdpGlyphs.o rdpPixmap.o rdpInput.o

CFLAGS = -g -O2 -Wall -fPIC -I/usr/include/xorg -I/usr/include/pixman-1

LDFLAGS =

LIBS =

all: libxorgxrdp.so

libxorgxrdp.so: $(OBJS) Makefile
	$(CC) -shared -o libxorgxrdp.so $(LDFLAGS) $(OBJS) $(LIBS)

clean:
	rm -f $(OBJS) libxorgxrdp.so
