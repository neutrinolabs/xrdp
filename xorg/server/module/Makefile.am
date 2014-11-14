EXTRA_DIST =

AM_CFLAGS = \
  $(XORG_SERVER_CFLAGS) \
  -I../../../common

libxorgxrdp_la_LTLIBRARIES = libxorgxrdp.la

libxorgxrdp_la_LDFLAGS = -module -avoid-version

libxorgxrdp_ladir = $(moduledir)

libxorgxrdp_la_SOURCES = rdpDraw.c rdpPri.c rdpGC.c rdpFillSpans.c \
rdpSetSpans.c rdpPutImage.c rdpCopyArea.c rdpCopyPlane.c rdpPolyPoint.c \
rdpPolylines.c rdpPolySegment.c rdpPolyRectangle.c rdpPolyArc.c \
rdpFillPolygon.c rdpPolyFillRect.c rdpPolyFillArc.c rdpPolyText8.c \
rdpPolyText16.c rdpImageText8.c rdpImageText16.c rdpImageGlyphBlt.c \
rdpPolyGlyphBlt.c rdpPushPixels.c rdpCursor.c rdpMain.c rdpRandR.c \
rdpMisc.c rdpReg.c rdpComposite.c rdpGlyphs.c rdpPixmap.c rdpInput.c \
rdpClientCon.c rdpCapture.c rdpTrapezoids.c rdpXv.c rdpSimd.c

libxorgxrdp_la_LIBADD =
