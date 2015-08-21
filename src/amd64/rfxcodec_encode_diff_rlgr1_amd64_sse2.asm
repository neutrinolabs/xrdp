
section .data
    const1 times 8 dw 1

%macro PROC 1
    align 16
    global %1
    %1:
%endmacro

;int
;rfxcodec_encode_diff_rlgr1_amd64_sse2(short *co,
;                                      void *dst, int dst_bytes);

PROC rfxcodec_encode_diff_rlgr1_amd64_sse2
    ; save registers
    push rbx
    mov rax, 0
    pop rbx
    ret
    align 16

