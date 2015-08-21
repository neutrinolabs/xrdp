
OBJS = rfxencode.o rfxcompose.o rfxencode_tile.o rfxencode_dwt.o \
rfxencode_quantization.o rfxencode_differential.o \
rfxencode_rlgr1.o rfxencode_rlgr3.o rfxencode_alpha.o

#OBJS += cpuid_x86.o rfxrlgr1_x86.o rfxrlgr3_x86.o rfxdwt_x86_sse2.o
#OBJS += cpuid_amd64.o rfxrlgr1_amd64.o rfxrlgr3_amd64.o rfxdwt_amd64_sse2.o

CFLAGS = $(PROFIL) -g -O2 -Wall -fPIC -I../include
#-DRFX_USE_ACCEL_X86
#-DRFX_USE_ACCEL_AMD64

LDFLAGS =

LIBS =

all: librfxencode.so

librfxencode.so: $(OBJS) Makefile
	$(CC) -shared -o librfxencode.so $(LDFLAGS) $(OBJS) $(LIBS)
	$(AR) -rv librfxencode.a $(OBJS)

cpuid_x86.o: x86/cpuid_x86.asm
	yasm -f elf32 -g dwarf2 x86/cpuid_x86.asm

rfxrlgr1_x86.o: x86/rfxrlgr1_x86.asm
	yasm -f elf32 -g dwarf2 x86/rfxrlgr1_x86.asm

rfxrlgr3_x86.o: x86/rfxrlgr3_x86.asm
	yasm -f elf32 -g dwarf2 x86/rfxrlgr3_x86.asm

rfxdwt_x86_sse2.o: x86/rfxdwt_x86_sse2.asm
	yasm -f elf32 -g dwarf2 x86/rfxdwt_x86_sse2.asm

cpuid_amd64.o: amd64/cpuid_amd64.asm
	yasm -f elf64 -g dwarf2 amd64/cpuid_amd64.asm

rfxrlgr1_amd64.o: amd64/rfxrlgr1_amd64.asm
	yasm -f elf64 -g dwarf2 amd64/rfxrlgr1_amd64.asm

rfxrlgr3_amd64.o: amd64/rfxrlgr3_amd64.asm
	yasm -f elf64 -g dwarf2 amd64/rfxrlgr3_amd64.asm

rfxdwt_amd64_sse2.o: amd64/rfxdwt_amd64_sse2.asm
	yasm -f elf64 -g dwarf2 amd64/rfxdwt_amd64_sse2.asm

clean:
	rm -f $(OBJS) librfxencode.so librfxencode.a
