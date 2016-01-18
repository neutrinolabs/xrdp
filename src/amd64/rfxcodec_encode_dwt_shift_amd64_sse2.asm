
section .data
    align 16
    cw128 times 8 dw 128
    cdFFFF times 4 dd 65535

section .text

%macro PROC 1
    align 16
    global %1
    %1:
%endmacro

;******************************************************************************
; source 16 bit signed, 8 pixel width
rfx_dwt_2d_encode_block_horiz_16_8:
    sub rsp, 32                         ; local vars, 32 bytes
    mov rcx, 8
loop1k:
    ; pre / post
    movdqu xmm1, [rsi]                  ; src[2n]
    movdqu xmm2, [rsi + 16]
    pand xmm1, [rel cdFFFF]
    pand xmm2, [rel cdFFFF]
    pslld xmm1, 16
    pslld xmm2, 16
    psrad xmm1, 16
    psrad xmm2, 16
    packssdw xmm1, xmm2
    movdqu xmm2, [rsi + 2]              ; src[2n + 1]
    movdqu xmm3, [rsi + 18]
    pand xmm2, [rel cdFFFF]
    pand xmm3, [rel cdFFFF]
    pslld xmm2, 16
    pslld xmm3, 16
    psrad xmm2, 16
    psrad xmm3, 16
    packssdw xmm2, xmm3
    mov bx, [rsi + 4]                   ; src[2n + 2]
    mov [rsp + 0], bx
    mov bx, [rsi + 8]
    mov [rsp + 2], bx
    mov bx, [rsi + 12]
    mov [rsp + 4], bx
    mov bx, [rsi + 16]
    mov [rsp + 6], bx
    mov bx, [rsi + 20]
    mov [rsp + 8], bx
    mov bx, [rsi + 24]
    mov [rsp + 10], bx
    mov bx, [rsi + 28]
    mov [rsp + 12], bx
    mov [rsp + 14], bx
    movdqu xmm3, [rsp]
    movdqa xmm4, xmm1
    movdqa xmm5, xmm2
    movdqa xmm6, xmm3
    ; h[n] = (src[2n + 1] - ((src[2n] + src[2n + 2]) >> 1)) >> 1
    paddw xmm4, xmm6
    psraw xmm4, 1
    psubw xmm5, xmm4
    psraw xmm5, 1
    movdqu [rdi], xmm5                  ; out hi
    ; l[n] = src[2n] + ((h[n - 1] + h[n]) >> 1)
    movdqu [rsp + 2], xmm5
    mov bx, [rsp + 2]
    mov [rsp], bx
    movdqu xmm7, [rsp]
    paddw xmm5, xmm7
    psraw xmm5, 1
    paddw xmm5, xmm1
    movdqu [rdx], xmm5                  ; out lo
    ; move right
    lea rsi, [rsi + 16 * 2]
    lea rdi, [rdi + 8 * 2]
    lea rdx, [rdx + 8 * 2]

    ; move left
    lea rsi, [rsi - 16 * 2]
    lea rdi, [rdi - 8 * 2]
    lea rdx, [rdx - 8 * 2]

    ; move down
    lea rsi, [rsi + 16 * 2]
    lea rdi, [rdi + 8 * 2]
    lea rdx, [rdx + 8 * 2]

    dec rcx
    jnz loop1k

    add rsp, 32                         ; local vars, 32 bytes
    ret

;******************************************************************************
; source 16 bit signed, 8 pixel width
rfx_dwt_2d_encode_block_verti_16_8:
    mov rcx, 2
loop1j:
    ; pre
    movdqu xmm1, [rsi]                  ; src[2n]
    movdqu xmm2, [rsi + 16 * 2]         ; src[2n + 1]
    movdqu xmm3, [rsi + 16 * 2 * 2]     ; src[2n + 2]
    movdqa xmm4, xmm1
    movdqa xmm5, xmm2
    movdqa xmm6, xmm3
    ; h[n] = (src[2n + 1] - ((src[2n] + src[2n + 2]) >> 1)) >> 1
    paddw xmm4, xmm6
    psraw xmm4, 1
    psubw xmm5, xmm4
    psraw xmm5, 1
    movdqu [rdi], xmm5                  ; out hi
    movdqa xmm6, xmm5                   ; save hi
    ; l[n] = src[2n] + ((h[n - 1] + h[n]) >> 1)
    paddw xmm5, xmm1
    movdqu [rdx], xmm5                  ; out lo
    movdqa xmm7, xmm6                   ; save hi
    ; move down
    lea rsi, [rsi + 16 * 2 * 2]         ; 2 rows
    lea rdi, [rdi + 16 * 2]             ; 1 row
    lea rdx, [rdx + 16 * 2]             ; 1 row

    ; loop
    mov rax, 6
loop2j:
    movdqa xmm1, xmm3                   ; src[2n]
    movdqu xmm2, [rsi + 16 * 2]         ; src[2n + 1]
    movdqu xmm3, [rsi + 16 * 2 * 2]     ; src[2n + 2]
    movdqa xmm4, xmm1
    movdqa xmm5, xmm2
    movdqa xmm6, xmm3
    ; h[n] = (src[2n + 1] - ((src[2n] + src[2n + 2]) >> 1)) >> 1
    paddw xmm4, xmm6
    psraw xmm4, 1
    psubw xmm5, xmm4
    psraw xmm5, 1
    movdqu [rdi], xmm5                  ; out hi
    movdqa xmm6, xmm5                   ; save hi
    ; l[n] = src[2n] + ((h[n - 1] + h[n]) >> 1)
    paddw xmm5, xmm7
    psraw xmm5, 1
    paddw xmm5, xmm1
    movdqu [rdx], xmm5                  ; out lo
    movdqa xmm7, xmm6                   ; save hi
    ; move down
    lea rsi, [rsi + 16 * 2 * 2]         ; 2 rows
    lea rdi, [rdi + 16 * 2]             ; 1 row
    lea rdx, [rdx + 16 * 2]             ; 1 row

    dec rax
    jnz loop2j

    ; post
    movdqa xmm1, xmm3                   ; src[2n]
    movdqu xmm2, [rsi + 16 * 2]         ; src[2n + 1]
    movdqa xmm4, xmm1
    movdqa xmm5, xmm2
    movdqa xmm6, xmm3
    ; h[n] = (src[2n + 1] - ((src[2n] + src[2n + 2]) >> 1)) >> 1
    paddw xmm4, xmm6
    psraw xmm4, 1
    psubw xmm5, xmm4
    psraw xmm5, 1
    movdqu [rdi], xmm5                  ; out hi
    ; l[n] = src[2n] + ((h[n - 1] + h[n]) >> 1)
    paddw xmm5, xmm7
    psraw xmm5, 1
    paddw xmm5, xmm1
    movdqu [rdx], xmm5                  ; out lo
    ; move down
    lea rsi, [rsi + 16 * 2 * 2]         ; 2 row
    lea rdi, [rdi + 16 * 2]             ; 1 row
    lea rdx, [rdx + 16 * 2]             ; 1 row

    ; move up
    lea rsi, [rsi - 16 * 16 * 2]
    lea rdi, [rdi - 8 * 16 * 2]
    lea rdx, [rdx - 8 * 16 * 2]

    ; move right
    lea rsi, [rsi + 16]
    lea rdi, [rdi + 16]
    lea rdx, [rdx + 16]

    dec rcx
    jnz loop1j

    ret

;******************************************************************************
; source 16 bit signed, 16 pixel width
rfx_dwt_2d_encode_block_horiz_16_16:
    sub rsp, 32                         ; local vars, 32 bytes
    mov rcx, 16
loop1e:
    ; pre
    movdqu xmm1, [rsi]                  ; src[2n]
    movdqu xmm2, [rsi + 16]
    pand xmm1, [rel cdFFFF]
    pand xmm2, [rel cdFFFF]
    pslld xmm1, 16
    pslld xmm2, 16
    psrad xmm1, 16
    psrad xmm2, 16
    packssdw xmm1, xmm2
    movdqu xmm2, [rsi + 2]              ; src[2n + 1]
    movdqu xmm3, [rsi + 18]
    pand xmm2, [rel cdFFFF]
    pand xmm3, [rel cdFFFF]
    pslld xmm2, 16
    pslld xmm3, 16
    psrad xmm2, 16
    psrad xmm3, 16
    packssdw xmm2, xmm3
    movdqu xmm3, [rsi + 4]              ; src[2n + 2]
    movdqu xmm4, [rsi + 20]
    pand xmm3, [rel cdFFFF]
    pand xmm4, [rel cdFFFF]
    pslld xmm3, 16
    pslld xmm4, 16
    psrad xmm3, 16
    psrad xmm4, 16
    packssdw xmm3, xmm4
    movdqa xmm4, xmm1
    movdqa xmm5, xmm2
    movdqa xmm6, xmm3
    ; h[n] = (src[2n + 1] - ((src[2n] + src[2n + 2]) >> 1)) >> 1
    paddw xmm4, xmm6
    psraw xmm4, 1
    psubw xmm5, xmm4
    psraw xmm5, 1
    movdqu [rdi], xmm5                  ; out hi
    ; l[n] = src[2n] + ((h[n - 1] + h[n]) >> 1)
    movdqu [rsp + 2], xmm5
    mov bx, [rsp + 2]
    mov [rsp], bx
    movdqu xmm7, [rsp]
    paddw xmm5, xmm7
    psraw xmm5, 1
    paddw xmm5, xmm1
    movdqu [rdx], xmm5                  ; out lo
    ; move right
    lea rsi, [rsi + 16 * 2]
    lea rdi, [rdi + 8 * 2]
    lea rdx, [rdx + 8 * 2]

    ; post
    movdqu xmm1, [rsi]                  ; src[2n]
    movdqu xmm2, [rsi + 16]
    pand xmm1, [rel cdFFFF]
    pand xmm2, [rel cdFFFF]
    pslld xmm1, 16
    pslld xmm2, 16
    psrad xmm1, 16
    psrad xmm2, 16
    packssdw xmm1, xmm2
    movdqu xmm2, [rsi + 2]              ; src[2n + 1]
    movdqu xmm3, [rsi + 18]
    pand xmm2, [rel cdFFFF]
    pand xmm3, [rel cdFFFF]
    pslld xmm2, 16
    pslld xmm3, 16
    psrad xmm2, 16
    psrad xmm3, 16
    packssdw xmm2, xmm3
    mov bx, [rsi + 4]                   ; src[2n + 2]
    mov [rsp + 0], bx
    mov bx, [rsi + 8]
    mov [rsp + 2], bx
    mov bx, [rsi + 12]
    mov [rsp + 4], bx
    mov bx, [rsi + 16]
    mov [rsp + 6], bx
    mov bx, [rsi + 20]
    mov [rsp + 8], bx
    mov bx, [rsi + 24]
    mov [rsp + 10], bx
    mov bx, [rsi + 28]
    mov [rsp + 12], bx
    mov [rsp + 14], bx
    movdqu xmm3, [rsp]
    movdqa xmm4, xmm1
    movdqa xmm5, xmm2
    movdqa xmm6, xmm3
    ; h[n] = (src[2n + 1] - ((src[2n] + src[2n + 2]) >> 1)) >> 1
    paddw xmm4, xmm6
    psraw xmm4, 1
    psubw xmm5, xmm4
    psraw xmm5, 1
    movdqu [rdi], xmm5                  ; out hi
    ; l[n] = src[2n] + ((h[n - 1] + h[n]) >> 1)
    movdqu xmm7, [rdi - 2]
    paddw xmm5, xmm7
    psraw xmm5, 1
    paddw xmm5, xmm1
    movdqu [rdx], xmm5                  ; out lo
    ; move right
    lea rsi, [rsi + 16 * 2]
    lea rdi, [rdi + 8 * 2]
    lea rdx, [rdx + 8 * 2]

    ; move left
    lea rsi, [rsi - 32 * 2]
    lea rdi, [rdi - 16 * 2]
    lea rdx, [rdx - 16 * 2]

    ; move down
    lea rsi, [rsi + 32 * 2]
    lea rdi, [rdi + 16 * 2]
    lea rdx, [rdx + 16 * 2]

    dec rcx
    jnz loop1e

    add rsp, 32                         ; local vars, 32 bytes
    ret

;******************************************************************************
; source 16 bit signed, 16 pixel width
rfx_dwt_2d_encode_block_verti_16_16:
    mov rcx, 4
loop1b:
    ; pre
    movdqu xmm1, [rsi]                  ; src[2n]
    movdqu xmm2, [rsi + 32 * 2]         ; src[2n + 1]
    movdqu xmm3, [rsi + 32 * 2 * 2]     ; src[2n + 2]
    movdqa xmm4, xmm1
    movdqa xmm5, xmm2
    movdqa xmm6, xmm3
    ; h[n] = (src[2n + 1] - ((src[2n] + src[2n + 2]) >> 1)) >> 1
    paddw xmm4, xmm6
    psraw xmm4, 1
    psubw xmm5, xmm4
    psraw xmm5, 1
    movdqu [rdi], xmm5                  ; out hi
    movdqa xmm6, xmm5                   ; save hi
    ; l[n] = src[2n] + ((h[n - 1] + h[n]) >> 1)
    paddw xmm5, xmm1
    movdqu [rdx], xmm5                  ; out lo
    movdqa xmm7, xmm6                   ; save hi
    ; move down
    lea rsi, [rsi + 32 * 2 * 2]         ; 2 rows
    lea rdi, [rdi + 32 * 2]             ; 1 row
    lea rdx, [rdx + 32 * 2]             ; 1 row

    ; loop
    mov rax, 14
loop2b:
    movdqa xmm1, xmm3                   ; src[2n]
    movdqu xmm2, [rsi + 32 * 2]         ; src[2n + 1]
    movdqu xmm3, [rsi + 32 * 2 * 2]     ; src[2n + 2]
    movdqa xmm4, xmm1
    movdqa xmm5, xmm2
    movdqa xmm6, xmm3
    ; h[n] = (src[2n + 1] - ((src[2n] + src[2n + 2]) >> 1)) >> 1
    paddw xmm4, xmm6
    psraw xmm4, 1
    psubw xmm5, xmm4
    psraw xmm5, 1
    movdqu [rdi], xmm5                  ; out hi
    movdqa xmm6, xmm5                   ; save hi
    ; l[n] = src[2n] + ((h[n - 1] + h[n]) >> 1)
    paddw xmm5, xmm7
    psraw xmm5, 1
    paddw xmm5, xmm1
    movdqu [rdx], xmm5                  ; out lo
    movdqa xmm7, xmm6                   ; save hi
    ; move down
    lea rsi, [rsi + 32 * 2 * 2]         ; 2 rows
    lea rdi, [rdi + 32 * 2]             ; 1 row
    lea rdx, [rdx + 32 * 2]             ; 1 row

    dec rax
    jnz loop2b

    ; post
    movdqa xmm1, xmm3                   ; src[2n]
    movdqu xmm2, [rsi + 32 * 2]         ; src[2n + 1]
    movdqa xmm4, xmm1
    movdqa xmm5, xmm2
    movdqa xmm6, xmm3
    ; h[n] = (src[2n + 1] - ((src[2n] + src[2n + 2]) >> 1)) >> 1
    paddw xmm4, xmm6
    psraw xmm4, 1
    psubw xmm5, xmm4
    psraw xmm5, 1
    movdqu [rdi], xmm5                  ; out hi
    ; l[n] = src[2n] + ((h[n - 1] + h[n]) >> 1)
    paddw xmm5, xmm7
    psraw xmm5, 1
    paddw xmm5, xmm1
    movdqu [rdx], xmm5                  ; out lo
    ; move down
    lea rsi, [rsi + 32 * 2 * 2]         ; 2 row
    lea rdi, [rdi + 32 * 2]             ; 1 row
    lea rdx, [rdx + 32 * 2]             ; 1 row

    ; move up
    lea rsi, [rsi - 32 * 32 * 2]
    lea rdi, [rdi - 16 * 32 * 2]
    lea rdx, [rdx - 16 * 32 * 2]

    ; move right
    lea rsi, [rsi + 16]
    lea rdi, [rdi + 16]
    lea rdx, [rdx + 16]

    dec rcx
    jnz loop1b

    ret

;******************************************************************************
rfx_dwt_2d_encode_block_horiz_16_32:
    sub rsp, 32                         ; local vars, 32 bytes
    mov rcx, 32
loop1d:
    ; pre
    movdqu xmm1, [rsi]                  ; src[2n]
    movdqu xmm2, [rsi + 16]
    pand xmm1, [rel cdFFFF]
    pand xmm2, [rel cdFFFF]
    pslld xmm1, 16
    pslld xmm2, 16
    psrad xmm1, 16
    psrad xmm2, 16
    packssdw xmm1, xmm2
    movdqu xmm2, [rsi + 2]              ; src[2n + 1]
    movdqu xmm3, [rsi + 18]
    pand xmm2, [rel cdFFFF]
    pand xmm3, [rel cdFFFF]
    pslld xmm2, 16
    pslld xmm3, 16
    psrad xmm2, 16
    psrad xmm3, 16
    packssdw xmm2, xmm3
    movdqu xmm3, [rsi + 4]              ; src[2n + 2]
    movdqu xmm4, [rsi + 20]
    pand xmm3, [rel cdFFFF]
    pand xmm4, [rel cdFFFF]
    pslld xmm3, 16
    pslld xmm4, 16
    psrad xmm3, 16
    psrad xmm4, 16
    packssdw xmm3, xmm4
    movdqa xmm4, xmm1
    movdqa xmm5, xmm2
    movdqa xmm6, xmm3
    ; h[n] = (src[2n + 1] - ((src[2n] + src[2n + 2]) >> 1)) >> 1
    paddw xmm4, xmm6
    psraw xmm4, 1
    psubw xmm5, xmm4
    psraw xmm5, 1
    movdqu [rdi], xmm5                  ; out hi
    ; l[n] = src[2n] + ((h[n - 1] + h[n]) >> 1)
    movdqu [rsp + 2], xmm5
    mov bx, [rsp + 2]
    mov [rsp], bx
    movdqu xmm7, [rsp]
    paddw xmm5, xmm7
    psraw xmm5, 1
    paddw xmm5, xmm1
    movdqu [rdx], xmm5                  ; out lo
    ; move right
    lea rsi, [rsi + 16 * 2]
    lea rdi, [rdi + 8 * 2]
    lea rdx, [rdx + 8 * 2]

    ; loop
    mov rax, 2
loop2d:
    movdqu xmm1, [rsi]                  ; src[2n]
    movdqu xmm2, [rsi + 16]
    pand xmm1, [rel cdFFFF]
    pand xmm2, [rel cdFFFF]
    pslld xmm1, 16
    pslld xmm2, 16
    psrad xmm1, 16
    psrad xmm2, 16
    packssdw xmm1, xmm2
    movdqu xmm2, [rsi + 2]              ; src[2n + 1]
    movdqu xmm3, [rsi + 18]
    pand xmm2, [rel cdFFFF]
    pand xmm3, [rel cdFFFF]
    pslld xmm2, 16
    pslld xmm3, 16
    psrad xmm2, 16
    psrad xmm3, 16
    packssdw xmm2, xmm3
    movdqu xmm3, [rsi + 4]              ; src[2n + 2]
    movdqu xmm4, [rsi + 20]
    pand xmm3, [rel cdFFFF]
    pand xmm4, [rel cdFFFF]
    pslld xmm3, 16
    pslld xmm4, 16
    psrad xmm3, 16
    psrad xmm4, 16
    packssdw xmm3, xmm4
    movdqa xmm4, xmm1
    movdqa xmm5, xmm2
    movdqa xmm6, xmm3
    ; h[n] = (src[2n + 1] - ((src[2n] + src[2n + 2]) >> 1)) >> 1
    paddw xmm4, xmm6
    psraw xmm4, 1
    psubw xmm5, xmm4
    psraw xmm5, 1
    movdqu [rdi], xmm5                  ; out hi
    ; l[n] = src[2n] + ((h[n - 1] + h[n]) >> 1)
    movdqu xmm7, [rdi - 2]
    paddw xmm5, xmm7
    psraw xmm5, 1
    paddw xmm5, xmm1
    movdqu [rdx], xmm5                  ; out lo
    ; move right
    lea rsi, [rsi + 16 * 2]
    lea rdi, [rdi + 8 * 2]
    lea rdx, [rdx + 8 * 2]

    dec rax
    jnz loop2d

    ; post
    movdqu xmm1, [rsi]                  ; src[2n]
    movdqu xmm2, [rsi + 16]
    pand xmm1, [rel cdFFFF]
    pand xmm2, [rel cdFFFF]
    pslld xmm1, 16
    pslld xmm2, 16
    psrad xmm1, 16
    psrad xmm2, 16
    packssdw xmm1, xmm2
    movdqu xmm2, [rsi + 2]              ; src[2n + 1]
    movdqu xmm3, [rsi + 18]
    pand xmm2, [rel cdFFFF]
    pand xmm3, [rel cdFFFF]
    pslld xmm2, 16
    pslld xmm3, 16
    psrad xmm2, 16
    psrad xmm3, 16
    packssdw xmm2, xmm3
    mov bx, [rsi + 4]                   ; src[2n + 2]
    mov [rsp + 0], bx
    mov bx, [rsi + 8]
    mov [rsp + 2], bx
    mov bx, [rsi + 12]
    mov [rsp + 4], bx
    mov bx, [rsi + 16]
    mov [rsp + 6], bx
    mov bx, [rsi + 20]
    mov [rsp + 8], bx
    mov bx, [rsi + 24]
    mov [rsp + 10], bx
    mov bx, [rsi + 28]
    mov [rsp + 12], bx
    mov [rsp + 14], bx
    movdqu xmm3, [rsp]
    movdqa xmm4, xmm1
    movdqa xmm5, xmm2
    movdqa xmm6, xmm3
    ; h[n] = (src[2n + 1] - ((src[2n] + src[2n + 2]) >> 1)) >> 1
    paddw xmm4, xmm6
    psraw xmm4, 1
    psubw xmm5, xmm4
    psraw xmm5, 1
    movdqu [rdi], xmm5                  ; out hi
    ; l[n] = src[2n] + ((h[n - 1] + h[n]) >> 1)
    movdqu xmm7, [rdi - 2]
    paddw xmm5, xmm7
    psraw xmm5, 1
    paddw xmm5, xmm1
    movdqu [rdx], xmm5                  ; out lo
    ; move right
    lea rsi, [rsi + 16 * 2]
    lea rdi, [rdi + 8 * 2]
    lea rdx, [rdx + 8 * 2]

    ; move left
    lea rsi, [rsi - 64 * 2]
    lea rdi, [rdi - 32 * 2]
    lea rdx, [rdx - 32 * 2]

    ; move down
    lea rsi, [rsi + 64 * 2]
    lea rdi, [rdi + 32 * 2]
    lea rdx, [rdx + 32 * 2]

    dec rcx
    jnz loop1d

    add rsp, 32                         ; local vars, 32 bytes
    ret

;******************************************************************************
; source 8 bit unsigned, 32 pixel width
rfx_dwt_2d_encode_block_verti_8_32:
    mov rcx, 8
loop1a:
    ; pre
    movq xmm1, [rsi]                    ; src[2n]
    movq xmm2, [rsi + 64 * 1]           ; src[2n + 1]
    movq xmm3, [rsi + 64 * 1 * 2]       ; src[2n + 2]
    punpcklbw xmm1, xmm0
    punpcklbw xmm2, xmm0
    punpcklbw xmm3, xmm0
    psubw xmm1, [rel cw128]
    psubw xmm2, [rel cw128]
    psubw xmm3, [rel cw128]
    psllw xmm1, 5
    psllw xmm2, 5
    psllw xmm3, 5
    movdqa xmm4, xmm1
    movdqa xmm5, xmm2
    movdqa xmm6, xmm3
    ; h[n] = (src[2n + 1] - ((src[2n] + src[2n + 2]) >> 1)) >> 1
    paddw xmm4, xmm6
    psraw xmm4, 1
    psubw xmm5, xmm4
    psraw xmm5, 1
    movdqu [rdi], xmm5                  ; out hi
    movdqa xmm6, xmm5                   ; save hi
    ; l[n] = src[2n] + ((h[n - 1] + h[n]) >> 1)
    paddw xmm5, xmm1
    movdqu [rdx], xmm5                  ; out lo
    movdqa xmm7, xmm6                   ; save hi
    ; move down
    lea rsi, [rsi + 64 * 1 * 2]         ; 2 rows
    lea rdi, [rdi + 64 * 2]             ; 1 row
    lea rdx, [rdx + 64 * 2]             ; 1 row

    ; loop
    mov rax, 30
loop2a:
    movdqa xmm1, xmm3                   ; src[2n]
    movq xmm2, [rsi + 64 * 1]           ; src[2n + 1]
    movq xmm3, [rsi + 64 * 1 * 2]       ; src[2n + 2]
    punpcklbw xmm2, xmm0
    punpcklbw xmm3, xmm0
    psubw xmm2, [rel cw128]
    psubw xmm3, [rel cw128]
    psllw xmm2, 5
    psllw xmm3, 5
    movdqa xmm4, xmm1
    movdqa xmm5, xmm2
    movdqa xmm6, xmm3
    ; h[n] = (src[2n + 1] - ((src[2n] + src[2n + 2]) >> 1)) >> 1
    paddw xmm4, xmm6
    psraw xmm4, 1
    psubw xmm5, xmm4
    psraw xmm5, 1
    movdqu [rdi], xmm5                  ; out hi
    movdqa xmm6, xmm5                   ; save hi
    ; l[n] = src[2n] + ((h[n - 1] + h[n]) >> 1)
    paddw xmm5, xmm7
    psraw xmm5, 1
    paddw xmm5, xmm1
    movdqu [rdx], xmm5                  ; out lo
    movdqa xmm7, xmm6                   ; save hi
    ; move down
    lea rsi, [rsi + 64 * 1 * 2]         ; 2 rows
    lea rdi, [rdi + 64 * 2]             ; 1 row
    lea rdx, [rdx + 64 * 2]             ; 1 row

    dec rax
    jnz loop2a

    ; post
    movdqa xmm1, xmm3                   ; src[2n]
    movq xmm2, [rsi + 64 * 1]           ; src[2n + 1]
    punpcklbw xmm2, xmm0
    psubw xmm2, [rel cw128]
    psllw xmm2, 5
    movdqa xmm4, xmm1
    movdqa xmm5, xmm2
    movdqa xmm6, xmm3
    ; h[n] = (src[2n + 1] - ((src[2n] + src[2n + 2]) >> 1)) >> 1
    paddw xmm4, xmm6
    psraw xmm4, 1
    psubw xmm5, xmm4
    psraw xmm5, 1
    movdqu [rdi], xmm5                  ; out hi
    ; l[n] = src[2n] + ((h[n - 1] + h[n]) >> 1)
    paddw xmm5, xmm7
    psraw xmm5, 1
    paddw xmm5, xmm1
    movdqu [rdx], xmm5                  ; out lo
    ; move down
    lea rsi, [rsi + 64 * 1 * 2]         ; 2 rows
    lea rdi, [rdi + 64 * 2]             ; 1 row
    lea rdx, [rdx + 64 * 2]             ; 1 row

    ; move up
    lea rsi, [rsi - 64 * 1 * 64]
    lea rdi, [rdi - 32 * 64 * 2]
    lea rdx, [rdx - 32 * 64 * 2]

    ; move right
    lea rsi, [rsi + 8]
    lea rdi, [rdi + 16]
    lea rdx, [rdx + 16]

    dec rcx
    jnz loop1a

    ret

;The first six integer or pointer arguments are passed in registers
;RDI, RSI, RDX, RCX, R8, and R9

;int
;rfxcodec_encode_dwt_shift_amd64_sse2(const char *qtable,
;                                     unsigned char *in_buffer,
;                                     short *out_buffer,
;                                     short *work_buffer);

;******************************************************************************
%ifidn __OUTPUT_FORMAT__,elf64
PROC rfxcodec_encode_dwt_shift_amd64_sse2
%else
PROC _rfxcodec_encode_dwt_shift_amd64_sse2
%endif
    ; save registers
    push rbx
    pxor xmm0, xmm0

    ; verical DWT to work buffer, level 1
    push rdx
    push rcx
    push rsi
    push rdi
    lea rdi, [rcx + 64 * 32 * 2]        ; dst hi
    lea rdx, [rcx]                      ; dst lo
    call rfx_dwt_2d_encode_block_verti_8_32
    pop rdi
    pop rsi
    pop rcx
    pop rdx

    ; horizontal DWT to out buffer, level 1, part 1
    push rdx
    push rcx
    push rsi
    push rdi
    lea rsi, [rcx]                      ; src
    lea rdi, [rdx]                      ; dst hi - HL1
    lea rdx, [rdx + 32 * 32 * 6]        ; dst lo - LL1
    call rfx_dwt_2d_encode_block_horiz_16_32
    pop rdi
    pop rsi
    pop rcx
    pop rdx

    ; horizontal DWT to out buffer, level 1, part 2
    push rdx
    push rcx
    push rsi
    push rdi
    lea rsi, [rcx + 64 * 32 * 2]        ; src
    lea rdi, [rdx + 32 * 32 * 4]        ; dst hi - HH1
    lea rdx, [rdx + 32 * 32 * 2]        ; dst lo - LH1
    call rfx_dwt_2d_encode_block_horiz_16_32
    pop rdi
    pop rsi
    pop rcx
    pop rdx

    ; verical DWT to work buffer, level 2
    push rdx
    push rcx
    push rsi
    push rdi
    lea rsi, [rdx + 32 * 32 * 6]        ; src
    lea rdi, [rcx + 32 * 16 * 2]        ; dst hi
    lea rdx, [rcx]                      ; dst lo
    call rfx_dwt_2d_encode_block_verti_16_16
    pop rdi
    pop rsi
    pop rcx
    pop rdx

    ; horizontal DWT to out buffer, level 2, part 1
    push rdx
    push rcx
    push rsi
    push rdi
    lea rsi, [rcx]                      ; src
    ; 32 * 32 * 6 + 16 * 16 * 0 = 6144
    lea rdi, [rdx + 6144]               ; dst hi - HL2
    ; 32 * 32 * 6 + 16 * 16 * 6 = 7680
    lea rdx, [rdx + 7680]               ; dst lo - LL2
    call rfx_dwt_2d_encode_block_horiz_16_16
    pop rdi
    pop rsi
    pop rcx
    pop rdx

    ; horizontal DWT to out buffer, level 2, part 2
    push rdx
    push rcx
    push rsi
    push rdi
    lea rsi, [rcx + 32 * 16 * 2]        ; src
    ; 32 * 32 * 6 + 16 * 16 * 4 = 7168
    lea rdi, [rdx + 7168]               ; dst hi - HH2
    ; 32 * 32 * 6 + 16 * 16 * 2 = 6656
    lea rdx, [rdx + 6656]               ; dst lo - LH2
    call rfx_dwt_2d_encode_block_horiz_16_16
    pop rdi
    pop rsi
    pop rcx
    pop rdx

    ; verical DWT to work buffer, level 3
    push rdx
    push rcx
    push rsi
    push rdi
    ; 32 * 32 * 6 + 16 * 16 * 6 = 7680
    lea rsi, [rdx + 7680]               ; src
    lea rdi, [rcx + 16 * 8 * 2]         ; dst hi
    lea rdx, [rcx]                      ; dst lo
    call rfx_dwt_2d_encode_block_verti_16_8
    pop rdi
    pop rsi
    pop rcx
    pop rdx

    ; horizontal DWT to out buffer, level 3, part 1
    push rdx
    push rcx
    push rsi
    push rdi
    lea rsi, [rcx]                      ; src
    ; 32 * 32 * 6 + 16 * 16 * 6 + 8 * 8 * 0 = 7680
    lea rdi, [rdx + 7680]               ; dst hi - HL3
    ; 32 * 32 * 6 + 16 * 16 * 6 + 8 * 8 * 6 = 8064
    lea rdx, [rdx + 8064]               ; dst lo - LL3
    call rfx_dwt_2d_encode_block_horiz_16_8
    pop rdi
    pop rsi
    pop rcx
    pop rdx

    ; horizontal DWT to out buffer, level 3, part 2
    push rdx
    push rcx
    push rsi
    push rdi
    lea rsi, [rcx + 16 * 8 * 2]         ; src
    ; 32 * 32 * 6 + 16 * 16 * 6 + 8 * 8 * 4 = 7936
    lea rdi, [rdx + 7936]               ; dst hi - HH3
    ; 32 * 32 * 6 + 16 * 16 * 6 + 8 * 8 * 2 = 7808
    lea rdx, [rdx + 7808]               ; dst lo - LH3
    call rfx_dwt_2d_encode_block_horiz_16_8
    pop rdi
    pop rsi
    pop rcx
    pop rdx

    mov rax, 0
    ; restore registers
    pop rbx
    ret
    align 16

