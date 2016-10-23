EXTRA_DIST = $(AMD64_ASM) $(X86_ASM) nasm_lt.sh

AMD64_ASM = \
  amd64/cpuid_amd64.asm \
  amd64/rfxcodec_encode_diff_rlgr1_amd64_sse2.asm \
  amd64/rfxcodec_encode_diff_rlgr3_amd64_sse2.asm \
  amd64/rfxcodec_encode_dwt_shift_amd64_sse2.asm \
  amd64/rfxcodec_encode_dwt_shift_amd64_sse41.asm

X86_ASM = \
  x86/cpuid_x86.asm \
  x86/rfxcodec_encode_diff_rlgr1_x86_sse2.asm \
  x86/rfxcodec_encode_diff_rlgr3_x86_sse2.asm \
  x86/rfxcodec_encode_dwt_shift_x86_sse2.asm \
  x86/rfxcodec_encode_dwt_shift_x86_sse41.asm

ASM_SOURCES =

AM_CPPFLAGS = \
  -I$(top_srcdir)/include \
  -I../include

if WITH_SIMD_AMD64
ASM_SOURCES += $(AMD64_ASM)
AM_CPPFLAGS += -DSIMD_USE_ACCEL=1 -DRFX_USE_ACCEL_AMD64=1
endif

if WITH_SIMD_X86
ASM_SOURCES += $(X86_ASM)
AM_CPPFLAGS += -DSIMD_USE_ACCEL=1 -DRFX_USE_ACCEL_X86=1
endif

noinst_HEADERS = \
  rfx_bitstream.h \
  rfxcommon.h \
  rfxcompose.h \
  rfxconstants.h \
  rfxencode_alpha.h \
  rfxencode_differential.h \
  rfxencode_dwt.h \
  rfxencode.h \
  rfxencode_quantization.h \
  rfxencode_rlgr1.h \
  rfxencode_rlgr3.h \
  rfxencode_tile.h \
  amd64/funcs_amd64.h \
  x86/funcs_x86.h

lib_LTLIBRARIES = librfxencode.la

librfxencode_la_SOURCES = $(noinst_HEADERS) rfxencode.c \
  rfxcompose.c rfxencode_tile.c rfxencode_dwt.c \
  rfxencode_quantization.c rfxencode_differential.c \
  rfxencode_rlgr1.c rfxencode_rlgr3.c rfxencode_alpha.c $(ASM_SOURCES)

.asm.lo:
	$(LIBTOOL) --mode=compile --tag NASM $(srcdir)/nasm_lt.sh $(NASM) $(NAFLAGS) -I$(srcdir) -I. $< -o $@
