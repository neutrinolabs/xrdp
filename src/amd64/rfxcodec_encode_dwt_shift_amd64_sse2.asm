
section .data
    const1 times 8 dw 1

%macro PROC 1
    align 16
    global %1
    %1:
%endmacro

;int
;rfxcodec_encode_dwt_shift_amd64_sse2(const char *qtable,
;                                     unsigned char *data,
;                                     short *dwt_buffer1,
;                                     short *dwt_buffer);

%ifidn __OUTPUT_FORMAT__,elf64
PROC rfxcodec_encode_dwt_shift_amd64_sse2
%else
PROC _rfxcodec_encode_dwt_shift_amd64_sse2
%endif
    ; save registers
    push rbx
    mov rax, 0
    pop rbx
    ret
    align 16

