EXTRA_DIST =

EXTRA_DEFINES =
EXTRA_INCLUDES =
EXTRA_LIBS =
EXTRA_FLAGS =
EXTRA_SRC =
EXTRA_ENCODE_SRC =

if WITH_SIMD_AMD64
EXTRA_ENCODE_SRC += amd64/cpuid_amd64.asm \
  amd64/rfxcodec_encode_diff_rlgr1_amd64_sse2.asm \
  amd64/rfxcodec_encode_diff_rlgr3_amd64_sse2.asm \
  amd64/rfxcodec_encode_dwt_shift_amd64_sse2.asm
EXTRA_DEFINES += -DSIMD_USE_ACCEL=1 -DRFX_USE_ACCEL_AMD64=1
endif

if WITH_SIMD_X86
EXTRA_ENCODE_SRC += x86/cpuid_x86.asm \
  x86/rfxcodec_encode_diff_rlgr1_x86_sse2.asm \
  x86/rfxcodec_encode_diff_rlgr3_x86_sse2.asm \
  x86/rfxcodec_encode_dwt_shift_x86_sse2.asm
EXTRA_DEFINES += -DSIMD_USE_ACCEL=1 -DRFX_USE_ACCEL_X86=1
endif

if GOT_PREFIX
EXTRA_INCLUDES += -I$(prefix)/include
EXTRA_FLAGS += -L$(prefix)/lib -Wl,-rpath -Wl,$(prefix)/lib
endif

AM_CFLAGS = -I../include $(EXTRA_DEFINES)

INCLUDES = $(EXTRA_INCLUDES)

lib_LTLIBRARIES = librfxencode.la

librfxencode_la_LDFLAGS = $(EXTRA_FLAGS)


librfxencode_ladir = $(moduledir)

librfxencode_la_SOURCES = rfxencode.c \
  rfxcompose.c rfxencode_tile.c rfxencode_dwt.c \
  rfxencode_quantization.c rfxencode_differential.c \
  rfxencode_rlgr1.c rfxencode_rlgr3.c rfxencode_alpha.c $(EXTRA_ENCODE_SRC)

.asm.lo:
	$(LIBTOOL) --mode=compile --tag NASM $(srcdir)/nasm_lt.sh $(NASM) $(NAFLAGS) -I$(srcdir) -I. $< -o $@

librfxencode_la_LIBADD =

include_HEADERS = ../include/rfxcodec_encode.h

