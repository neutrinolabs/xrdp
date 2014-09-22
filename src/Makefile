
OBJS = rfxencode.o rfxcompose.o rfxencode_tile.o rfxencode_dwt.o rfxencode_quantization.o \
rfxencode_differential.o rfxencode_rlgr1.o rfxencode_rlgr3.o \
rfxdwt_sse4.o rfxrlgr1.o rfxrlgr3.o

CFLAGS = $(PROFIL) -g -O2 -Wall -fPIC -I../include

LDFLAGS =

LIBS =

all: librfxencode.so

librfxencode.so: $(OBJS) Makefile
	$(CC) -shared -o librfxencode.so $(LDFLAGS) $(OBJS) $(LIBS)
	$(AR) -rv librfxencode.a $(OBJS)

rfxdwt_sse4.o: x86/rfxdwt_sse4.asm
	yasm -f elf32 -g dwarf2 x86/rfxdwt_sse4.asm

rfxrlgr1.o: x86/rfxrlgr1.asm
	yasm -f elf32 -g dwarf2 x86/rfxrlgr1.asm

rfxrlgr3.o: x86/rfxrlgr3.asm
	yasm -f elf32 -g dwarf2 x86/rfxrlgr3.asm

clean:
	rm -f $(OBJS) librfxencode.so librfxencode.a
