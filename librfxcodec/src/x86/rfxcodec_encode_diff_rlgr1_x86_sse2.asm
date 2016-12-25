%ifidn __OUTPUT_FORMAT__,elf
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

;int
;rfxcodec_encode_diff_rlgr1_x86_sse2(short *co,
;                                    void *dst, int dst_bytes);

%ifidn __OUTPUT_FORMAT__,elf
PROC rfxcodec_encode_diff_rlgr1_x86_sse2
%else
PROC _rfxcodec_encode_diff_rlgr1_x86_sse2
%endif
    push ebx
    push esi
    push edi

    mov eax, 0
    pop edi
    pop esi
    pop ebx
    ret
    align 16

