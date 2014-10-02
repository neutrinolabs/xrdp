
section .data
    const1 times 8 dw 1

%macro PROC 1
    align 16
    global %1
    %1:
%endmacro

;int
;dwt_shift_x86_sse4(const int* qtable, sint8* src, sint16* dst, sint16* temp)

PROC dwt_shift_x86_sse4
    push ebx
    push esi
    push edi

    mov eax, 0
    pop edi
    pop esi
    pop ebx
    ret
    align 16

