
section .data
    const1 times 8 dw 1

%macro PROC 1
    align 16
    global %1
    %1:
%endmacro

;int
;dwt_shift_amd64_sse2(const int *quantization_values, uint8 *data,
;                     sint16 *dwt_buffer1, sint16 *dwt_buffer);

PROC dwt_shift_amd64_sse2
    ; save registers
    push rbx
    mov rax, 0
    pop rbx
    ret
    align 16

