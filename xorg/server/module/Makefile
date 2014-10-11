
OBJS = rdpDraw.o rdpPri.o rdpGC.o rdpFillSpans.o rdpSetSpans.o rdpPutImage.o \
rdpCopyArea.o rdpCopyPlane.o rdpPolyPoint.o rdpPolylines.o rdpPolySegment.o \
rdpPolyRectangle.o rdpPolyArc.o rdpFillPolygon.o rdpPolyFillRect.o \
rdpPolyFillArc.o rdpPolyText8.o rdpPolyText16.o rdpImageText8.o \
rdpImageText16.o rdpImageGlyphBlt.o rdpPolyGlyphBlt.o rdpPushPixels.o \
rdpCursor.o rdpMain.o rdpRandR.o rdpMisc.o rdpReg.o \
rdpComposite.o rdpGlyphs.o rdpPixmap.o rdpInput.o rdpClientCon.o rdpCapture.o \
rdpTrapezoids.o rdpXv.o rdpSimd.o

;OBJS += cpuid_x86.o i420_to_rgb32_x86_sse2.o yv12_to_rgb32_x86_sse2.o yuy2_to_rgb32_x86_sse2.o uyvy_to_rgb32_x86_sse2.o
;OBJS += cpuid_amd64.o i420_to_rgb32_amd64_sse2.o yv12_to_rgb32_amd64_sse2.o yuy2_to_rgb32_amd64_sse2.o uyvy_to_rgb32_amd64_sse2.o

CFLAGS = -g -O2 -Wall -fPIC -I/usr/include/xorg -I/usr/include/pixman-1 \
-I../../../common

;CFLAGS += -DSIMD_USE_ACCEL=1

LDFLAGS =

LIBS =

all: libxorgxrdp.so

libxorgxrdp.so: $(OBJS) Makefile
	$(CC) -shared -o libxorgxrdp.so $(LDFLAGS) $(OBJS) $(LIBS)

clean:
	rm -f $(OBJS) libxorgxrdp.so

cpuid_x86.o: x86/cpuid_x86.asm
	yasm -f elf32 -g dwarf2 x86/cpuid_x86.asm

i420_to_rgb32_x86_sse2.o: x86/i420_to_rgb32_x86_sse2.asm
	yasm -f elf32 -g dwarf2 x86/i420_to_rgb32_x86_sse2.asm

yv12_to_rgb32_x86_sse2.o: x86/yv12_to_rgb32_x86_sse2.asm
	yasm -f elf32 -g dwarf2 x86/yv12_to_rgb32_x86_sse2.asm

yuy2_to_rgb32_x86_sse2.o: x86/yuy2_to_rgb32_x86_sse2.asm
	yasm -f elf32 -g dwarf2 x86/yuy2_to_rgb32_x86_sse2.asm

uyvy_to_rgb32_x86_sse2.o: x86/uyvy_to_rgb32_x86_sse2.asm
	yasm -f elf32 -g dwarf2 x86/uyvy_to_rgb32_x86_sse2.asm

cpuid_amd64.o: amd64/cpuid_amd64.asm
	yasm -f elf64 -g dwarf2 amd64/cpuid_amd64.asm

i420_to_rgb32_amd64_sse2.o: amd64/i420_to_rgb32_amd64_sse2.asm
	yasm -f elf64 -g dwarf2 amd64/i420_to_rgb32_amd64_sse2.asm

yv12_to_rgb32_amd64_sse2.o: amd64/yv12_to_rgb32_amd64_sse2.asm
	yasm -f elf64 -g dwarf2 amd64/yv12_to_rgb32_amd64_sse2.asm

yuy2_to_rgb32_amd64_sse2.o: amd64/yuy2_to_rgb32_amd64_sse2.asm
	yasm -f elf64 -g dwarf2 amd64/yuy2_to_rgb32_amd64_sse2.asm

uyvy_to_rgb32_amd64_sse2.o: amd64/uyvy_to_rgb32_amd64_sse2.asm
	yasm -f elf64 -g dwarf2 amd64/uyvy_to_rgb32_amd64_sse2.asm

