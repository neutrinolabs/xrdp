
section .data
    const1 times 8 dw 1

%macro PROC 1
    align 16
    global %1
    %1:
%endmacro

;int
;rfxcodec_encode_dwt_shift_x86_sse2(const char *qtable,
;                                   unsigned char *data,
;                                   short *dwt_buffer1,
;                                   short *dwt_buffer);

%ifidn __OUTPUT_FORMAT__,win
; Win32 - add _(underscore)
PROC rfxcodec_encode_dwt_shift_x86_sse2
%else
PROC rfxcodec_encode_dwt_shift_x86_sse2
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

