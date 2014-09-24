
%macro PROC 1
    align 16
    global %1
    %1:
%endmacro

;int
;yuy2_to_rgb32_x86_sse2(unsigned char *yuvs, int width, int height, int *rgbs)

PROC yuy2_to_rgb32_x86_sse2
    push ebx
    push esi
    push edi

    mov eax, 0
    pop edi
    pop esi
    pop ebx
    ret
    align 16

