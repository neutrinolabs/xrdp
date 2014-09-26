
SECTION .data
align 16
c16 times 4 dd 16
c100 times 4 dd 100
c128 times 4 dd 128
c208 times 4 dd 208
c255 times 4 dd 255
c298 times 4 dd 298
c409 times 4 dd 409
c516 times 4 dd 516

SECTION .text

%macro PROC 1
    align 16
    global %1
    %1:
%endmacro

y1_do4:
    ; y
    mov eax, 0
    mov al, [esi]
    add esi, 1
    pinsrd xmm0, eax, 0
    mov al, [esi]
    add esi, 1
    pinsrd xmm0, eax, 1
    mov al, [esi]
    add esi, 1
    pinsrd xmm0, eax, 2
    mov al, [esi]
    add esi, 1
    pinsrd xmm0, eax, 3
    movdqa xmm7, [c16]
    psubd xmm0, xmm7

    ; u
    mov eax, 0
    mov al, [ebx]
    add ebx, 1
    pinsrd xmm1, eax, 0
    pinsrd xmm1, eax, 1
    mov al, [ebx]
    add ebx, 1
    pinsrd xmm1, eax, 2
    pinsrd xmm1, eax, 3
    movdqa xmm7, [c128]
    psubd xmm1, xmm7

    ; v
    mov eax, 0
    mov al, [edx]
    add edx, 1
    pinsrd xmm2, eax, 0
    pinsrd xmm2, eax, 1
    mov al, [edx]
    add edx, 1
    pinsrd xmm2, eax, 2
    pinsrd xmm2, eax, 3
    psubd xmm2, xmm7

    ; t = (298 * c + 409 * e + 128) >> 8;
    movdqa xmm3, [c298]
    pmulld xmm3, xmm0
    movdqa xmm4, [c409]
    pmulld xmm4, xmm2
    paddd xmm3, xmm4
    paddd xmm3, xmm7
    psrad xmm3, 8
    ; b = RDPCLAMP(t, 0, 255);
    pxor xmm4, xmm4
    pmaxsd xmm3, xmm4
    movdqa xmm4, [c255]
    pminsd xmm3, xmm4

    ; t = (298 * c - 100 * d - 208 * e + 128) >> 8;
    movdqa xmm4, [c298]
    pmulld xmm4, xmm0
    movdqa xmm5, [c100]
    pmulld xmm5, xmm1
    movdqa xmm6, [c208]
    pmulld xmm6, xmm2
    psubd xmm4, xmm5
    psubd xmm4, xmm6
    paddd xmm4, xmm7
    psrad xmm4, 8
    ; g = RDPCLAMP(t, 0, 255);
    pxor xmm5, xmm5
    pmaxsd xmm4, xmm5
    movdqa xmm5, [c255]
    pminsd xmm4, xmm5

    ; t = (298 * c + 516 * d + 128) >> 8;
    movdqa xmm5, [c298]
    pmulld xmm5, xmm0
    movdqa xmm6, [c516]
    pmulld xmm6, xmm1
    paddd xmm5, xmm6
    paddd xmm5, xmm7
    psrad xmm5, 8
    ; r = RDPCLAMP(t, 0, 255);
    pxor xmm6, xmm6
    pmaxsd xmm5, xmm6
    movdqa xmm6, [c255]
    pminsd xmm5, xmm6

    pextrd eax, xmm3, 0
    mov [edi], al
    pextrd eax, xmm4, 0
    mov [edi + 1], al
    pextrd eax, xmm5, 0
    mov [edi + 2], al
    mov eax, 0
    mov [edi + 3], al
    add edi, 4

    pextrd eax, xmm3, 1
    mov [edi], al
    pextrd eax, xmm4, 1
    mov [edi + 1], al
    pextrd eax, xmm5, 1
    mov [edi + 2], al
    mov eax, 0
    mov [edi + 3], al
    add edi, 4

    pextrd eax, xmm3, 2
    mov [edi], al
    pextrd eax, xmm4, 2
    mov [edi + 1], al
    pextrd eax, xmm5, 2
    mov [edi + 2], al
    mov eax, 0
    mov [edi + 3], al
    add edi, 4

    pextrd eax, xmm3, 3
    mov [edi], al
    pextrd eax, xmm4, 3
    mov [edi + 1], al
    pextrd eax, xmm5, 3
    mov [edi + 2], al
    mov eax, 0
    mov [edi + 3], al
    add edi, 4

    ret;

y2_do4:
    ; y
    mov eax, 0
    mov al, [esi]
    add esi, 1
    pinsrd xmm0, eax, 0
    mov al, [esi]
    add esi, 1
    pinsrd xmm0, eax, 1
    mov al, [esi]
    add esi, 1
    pinsrd xmm0, eax, 2
    mov al, [esi]
    add esi, 1
    pinsrd xmm0, eax, 3
    movdqa xmm7, [c16]
    psubd xmm0, xmm7

    movdqa xmm7, [c128]

    ; t = (298 * c + 409 * e + 128) >> 8;
    movdqa xmm3, [c298]
    pmulld xmm3, xmm0
    movdqa xmm4, [c409]
    pmulld xmm4, xmm2
    paddd xmm3, xmm4
    paddd xmm3, xmm7
    psrad xmm3, 8
    ; b = RDPCLAMP(t, 0, 255);
    pxor xmm4, xmm4
    pmaxsd xmm3, xmm4
    movdqa xmm4, [c255]
    pminsd xmm3, xmm4

    ; t = (298 * c - 100 * d - 208 * e + 128) >> 8;
    movdqa xmm4, [c298]
    pmulld xmm4, xmm0
    movdqa xmm5, [c100]
    pmulld xmm5, xmm1
    movdqa xmm6, [c208]
    pmulld xmm6, xmm2
    psubd xmm4, xmm5
    psubd xmm4, xmm6
    paddd xmm4, xmm7
    psrad xmm4, 8
    ; g = RDPCLAMP(t, 0, 255);
    pxor xmm5, xmm5
    pmaxsd xmm4, xmm5
    movdqa xmm5, [c255]
    pminsd xmm4, xmm5

    ; t = (298 * c + 516 * d + 128) >> 8;
    movdqa xmm5, [c298]
    pmulld xmm5, xmm0
    movdqa xmm6, [c516]
    pmulld xmm6, xmm1
    paddd xmm5, xmm6
    paddd xmm5, xmm7
    psrad xmm5, 8
    ; r = RDPCLAMP(t, 0, 255);
    pxor xmm6, xmm6
    pmaxsd xmm5, xmm6
    movdqa xmm6, [c255]
    pminsd xmm5, xmm6

    pextrd eax, xmm3, 0
    mov [edi], al
    pextrd eax, xmm4, 0
    mov [edi + 1], al
    pextrd eax, xmm5, 0
    mov [edi + 2], al
    mov eax, 0
    mov [edi + 3], al
    add edi, 4

    pextrd eax, xmm3, 1
    mov [edi], al
    pextrd eax, xmm4, 1
    mov [edi + 1], al
    pextrd eax, xmm5, 1
    mov [edi + 2], al
    mov eax, 0
    mov [edi + 3], al
    add edi, 4

    pextrd eax, xmm3, 2
    mov [edi], al
    pextrd eax, xmm4, 2
    mov [edi + 1], al
    pextrd eax, xmm5, 2
    mov [edi + 2], al
    mov eax, 0
    mov [edi + 3], al
    add edi, 4

    pextrd eax, xmm3, 3
    mov [edi], al
    pextrd eax, xmm4, 3
    mov [edi + 1], al
    pextrd eax, xmm5, 3
    mov [edi + 2], al
    mov eax, 0
    mov [edi + 3], al
    add edi, 4

    ret;

;int
;yv12_to_rgb32_x86_sse2(unsigned char *yuvs, int width, int height, int *rgbs)

PROC yv12_to_rgb32_x86_sse2
    push ebx
    push esi
    push edi
    push ebp

    mov edi, [esp + 32] ; rgbs

    mov ecx, [esp + 24] ; width
    mov edx, ecx
    mov ebp, [esp + 28] ; height
    mov eax, ebp
    shr ebp, 1
    imul eax, ecx       ; eax = width * height

    mov esi, [esp + 20] ; y

    mov ebx, esi        ; u = y + width * height
    add ebx, eax

    ; local vars
    ; char* yptr1;
    ; char* yptr2;
    ; char* uptr;
    ; char* vptr;
    ; int* rgbs1;
    ; int* rgbs2;
    ; int width
    sub esp, 28         ; local vars, 28 bytes
    mov [esp + 0], esi  ; save y1
    add esi, edx
    mov [esp + 4], esi  ; save y2
    mov [esp + 8], ebx  ; save u
    shr eax, 2
    add ebx, eax        ; v = u + (width * height / 4)
    mov [esp + 12], ebx ; save v

    mov [esp + 16], edi ; save rgbs1
    mov eax, edx
    shl eax, 2
    add edi, eax
    mov [esp + 20], edi ; save rgbs2

loop_y:

    mov ecx, edx        ; width
    shr ecx, 2

    ; save edx
    mov [esp + 24], edx

loop_x:

    mov esi, [esp + 0]  ; y1
    mov ebx, [esp + 8]  ; u
    mov edx, [esp + 12] ; v
    mov edi, [esp + 16] ; rgbs1

    ; y1
    call y1_do4

    mov [esp + 0], esi  ; y1
    mov [esp + 16], edi ; rgbs1

    mov esi, [esp + 4]  ; y2
    mov edi, [esp + 20] ; rgbs2

    ; y2
    call y2_do4

    mov [esp + 4], esi  ; y2
    mov [esp + 8], ebx  ; u
    mov [esp + 12], edx ; v
    mov [esp + 20], edi ; rgbs2

    dec ecx             ; width
    jnz loop_x

    ; restore edx
    mov edx, [esp + 24]

    ; update y1 and 2
    mov eax, [esp + 0]
    mov ebx, edx
    add eax, ebx
    mov [esp + 0], eax

    mov eax, [esp + 4]
    add eax, ebx
    mov [esp + 4], eax

    ; update rgb1 and 2
    mov eax, [esp + 16]
    mov ebx, edx
    shl ebx, 2
    add eax, ebx
    mov [esp + 16], eax

    mov eax, [esp + 20]
    add eax, ebx
    mov [esp + 20], eax

    mov ecx, ebp
    dec ecx             ; height
    mov ebp, ecx
    jnz loop_y

    add esp, 28

    mov eax, 0
    pop ebp
    pop edi
    pop esi
    pop ebx
    ret
    align 16


