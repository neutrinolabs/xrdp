
section .data
    const1 times 8 dw 1

%macro PROC 1
    align 16
    global %1
    %1:
%endmacro

;int
;diff_rlgr1_x86(sint16 *co, int num_co, uint8 *dst, int dst_bytes);

PROC diff_rlgr1_x86
    push ebx
    push esi
    push edi

    mov eax, 0
    pop edi
    pop esi
    pop ebx
    ret
    align 16

