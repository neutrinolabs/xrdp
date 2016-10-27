%ifidn __OUTPUT_FORMAT__,elf64
section .note.GNU-stack noalloc noexec nowrite progbits
%endif

section .data
    const1 times 8 dw 1

section .text

%macro PROC 1
    align 16
    global %1
    %1:
%endmacro

;The first six integer or pointer arguments are passed in registers
;RDI, RSI, RDX, RCX, R8, and R9

;int
;rfxcodec_encode_diff_rlgr1_amd64_sse2(short *co,
;                                      void *dst, int dst_bytes);

%ifidn __OUTPUT_FORMAT__,elf64
PROC rfxcodec_encode_diff_rlgr1_amd64_sse2
%else
PROC _rfxcodec_encode_diff_rlgr1_amd64_sse2
%endif
    ; save registers
    push rbx

    mov rax, 0
    ; restore registers
    pop rbx
    ret
    align 16

