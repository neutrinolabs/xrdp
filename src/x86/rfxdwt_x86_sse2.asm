
section .data
    const1 times 8 dw 1

%macro PROC 1
    align 16
    global %1
    %1:
%endmacro

;int
;dwt_shift_x86_sse2(const int *quantization_values, uint8 *data,
;                   sint16 *dwt_buffer1, sint16 *dwt_buffer);

PROC dwt_shift_x86_sse2
    push ebx
    push esi
    push edi

    mov eax, 0
    pop edi
    pop esi
    pop ebx
    ret
    align 16

