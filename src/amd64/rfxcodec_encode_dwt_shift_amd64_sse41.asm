
section .data
    align 16
    cw128    times 8 dw 128
    cdFFFF   times 4 dd 65535
    ; these are 1 << (factor - 1) 0 to 15 is factor
    cwa0     times 8 dw 0     ; 0
    cwa1     times 8 dw 1     ; 1
    cwa2     times 8 dw 2     ; 2
    cwa4     times 8 dw 4     ; 3
    cwa8     times 8 dw 8     ; 4
    cwa16    times 8 dw 16    ; 5
    cwa32    times 8 dw 32    ; 6
    cwa64    times 8 dw 64    ; 7
    cwa128   times 8 dw 128   ; 8
    cwa256   times 8 dw 256   ; 9
    cwa512   times 8 dw 512   ; 10
    cwa1024  times 8 dw 1024  ; 11
    cwa2048  times 8 dw 2048  ; 12
    cwa4096  times 8 dw 4096  ; 13
    cwa8192  times 8 dw 8192  ; 14
    cwa16384 times 8 dw 16384 ; 15

section .text

%macro PROC 1
    align 16
    global %1
    %1:
%endmacro

;******************************************************************************
; source 16 bit signed, 16 pixel width
rfx_dwt_2d_encode_block_horiz_16_16:
    mov ecx, 8
loop1a:
    ; pre / post
    movdqa xmm1, [rsi]                  ; src[2n]
    movdqa xmm2, [rsi + 16]
    movdqa xmm6, xmm1
    movdqa xmm7, xmm2
    pand xmm1, [rel cdFFFF]
    pand xmm2, [rel cdFFFF]
    packusdw xmm1, xmm2
    movdqa xmm2, xmm6                   ; src[2n + 1]
    movdqa xmm3, xmm7
    psrldq xmm2, 2
    psrldq xmm3, 2
    pand xmm2, [rel cdFFFF]
    pand xmm3, [rel cdFFFF]
    packusdw xmm2, xmm3
    movdqa xmm3, xmm6                   ; src[2n + 2]
    movdqa xmm4, xmm7
    psrldq xmm3, 4
    psrldq xmm4, 4
    movd eax, xmm7
    movd xmm5, eax
    pslldq xmm5, 12
    por xmm3, xmm5
    movdqa xmm5, xmm7
    psrldq xmm5, 12
    pslldq xmm5, 12
    por xmm4, xmm5
    pand xmm3, [rel cdFFFF]
    pand xmm4, [rel cdFFFF]
    packusdw xmm3, xmm4
    movdqa xmm4, xmm1
    movdqa xmm5, xmm2
    movdqa xmm6, xmm3
    ; h[n] = (src[2n + 1] - ((src[2n] + src[2n + 2]) >> 1)) >> 1
    paddw xmm4, xmm6
    psraw xmm4, 1
    psubw xmm5, xmm4
    psraw xmm5, 1
    movdqa xmm6, xmm5                   ; out hi
    paddw xmm6, xmm8
    psraw xmm6, xmm9
    movdqa [rdi], xmm6
    ; l[n] = src[2n] + ((h[n - 1] + h[n]) >> 1)
    movdqa xmm7, xmm5
    movd eax, xmm7
    pslldq xmm7, 2
    and eax, 0xFFFF
    movd xmm6, eax
    por xmm7, xmm6
    paddw xmm5, xmm7
    psraw xmm5, 1
    paddw xmm5, xmm1

    movdqa xmm6, xmm5                   ; out lo
    paddw xmm6, xmm10
    psraw xmm6, xmm11
    movdqa [rdx], xmm6

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

    dec ecx
    jnz loop1a

    ret

;******************************************************************************
; source 16 bit signed, 16 pixel width
rfx_dwt_2d_encode_block_verti_16_16:
    mov ecx, 2
loop1b:
    ; pre
    movdqa xmm1, [rsi]                  ; src[2n]
    movdqa xmm2, [rsi + 16 * 2]         ; src[2n + 1]
    movdqa xmm3, [rsi + 16 * 2 * 2]     ; src[2n + 2]
    movdqa xmm4, xmm1
    movdqa xmm5, xmm2
    movdqa xmm6, xmm3
    ; h[n] = (src[2n + 1] - ((src[2n] + src[2n + 2]) >> 1)) >> 1
    paddw xmm4, xmm6
    psraw xmm4, 1
    psubw xmm5, xmm4
    psraw xmm5, 1
    movdqa [rdi], xmm5                  ; out hi
    movdqa xmm6, xmm5                   ; save hi
    ; l[n] = src[2n] + ((h[n - 1] + h[n]) >> 1)
    paddw xmm5, xmm1
    movdqa [rdx], xmm5                  ; out lo
    movdqa xmm7, xmm6                   ; save hi
    ; move down
    lea rsi, [rsi + 16 * 2 * 2]         ; 2 rows
    lea rdi, [rdi + 16 * 2]             ; 1 row
    lea rdx, [rdx + 16 * 2]             ; 1 row

    ; loop
    shl ecx, 16
    mov cx, 6
loop2b:
    movdqa xmm1, xmm3                   ; src[2n]
    movdqa xmm2, [rsi + 16 * 2]         ; src[2n + 1]
    movdqa xmm3, [rsi + 16 * 2 * 2]     ; src[2n + 2]
    movdqa xmm4, xmm1
    movdqa xmm5, xmm2
    movdqa xmm6, xmm3
    ; h[n] = (src[2n + 1] - ((src[2n] + src[2n + 2]) >> 1)) >> 1
    paddw xmm4, xmm6
    psraw xmm4, 1
    psubw xmm5, xmm4
    psraw xmm5, 1
    movdqa [rdi], xmm5                  ; out hi
    movdqa xmm6, xmm5                   ; save hi
    ; l[n] = src[2n] + ((h[n - 1] + h[n]) >> 1)
    paddw xmm5, xmm7
    psraw xmm5, 1
    paddw xmm5, xmm1
    movdqa [rdx], xmm5                  ; out lo
    movdqa xmm7, xmm6                   ; save hi
    ; move down
    lea rsi, [rsi + 16 * 2 * 2]         ; 2 rows
    lea rdi, [rdi + 16 * 2]             ; 1 row
    lea rdx, [rdx + 16 * 2]             ; 1 row

    dec cx
    jnz loop2b
    shr ecx, 16

    ; post
    movdqa xmm1, xmm3                   ; src[2n]
    movdqa xmm2, [rsi + 16 * 2]         ; src[2n + 1]
    movdqa xmm4, xmm1
    movdqa xmm5, xmm2
    movdqa xmm6, xmm3
    ; h[n] = (src[2n + 1] - ((src[2n] + src[2n + 2]) >> 1)) >> 1
    paddw xmm4, xmm6
    psraw xmm4, 1
    psubw xmm5, xmm4
    psraw xmm5, 1
    movdqa [rdi], xmm5                  ; out hi
    ; l[n] = src[2n] + ((h[n - 1] + h[n]) >> 1)
    paddw xmm5, xmm7
    psraw xmm5, 1
    paddw xmm5, xmm1
    movdqa [rdx], xmm5                  ; out lo
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

    dec ecx
    jnz loop1b

    ret

;******************************************************************************
; source 16 bit signed, 32 pixel width
rfx_dwt_2d_encode_block_horiz_16_32:
    mov ecx, 16
loop1c:
    ; pre
    movdqa xmm1, [rsi]                  ; src[2n]
    movdqa xmm2, [rsi + 16]
    movdqa xmm6, xmm1
    movdqa xmm7, xmm2
    pand xmm1, [rel cdFFFF]
    pand xmm2, [rel cdFFFF]
    packusdw xmm1, xmm2
    movdqa xmm2, xmm6                   ; src[2n + 1]
    movdqa xmm3, xmm7
    psrldq xmm2, 2
    psrldq xmm3, 2
    pand xmm2, [rel cdFFFF]
    pand xmm3, [rel cdFFFF]
    packusdw xmm2, xmm3
    movdqa xmm3, xmm6                   ; src[2n + 2]
    movdqa xmm4, xmm7
    psrldq xmm3, 4
    psrldq xmm4, 4
    movd eax, xmm7
    movd xmm5, eax
    pslldq xmm5, 12
    por xmm3, xmm5
    mov eax, [rsi + 32]
    movd xmm5, eax
    pslldq xmm5, 12
    por xmm4, xmm5
    pand xmm3, [rel cdFFFF]
    pand xmm4, [rel cdFFFF]
    packusdw xmm3, xmm4
    movdqa xmm4, xmm1
    movdqa xmm5, xmm2
    movdqa xmm6, xmm3
    ; h[n] = (src[2n + 1] - ((src[2n] + src[2n + 2]) >> 1)) >> 1
    paddw xmm4, xmm6
    psraw xmm4, 1
    psubw xmm5, xmm4
    psraw xmm5, 1

    movdqa xmm6, xmm5                   ; out hi
    paddw xmm6, xmm8
    psraw xmm6, xmm9
    movdqa [rdi], xmm6
    movdqa xmm2, xmm5                   ; save hi

    ; l[n] = src[2n] + ((h[n - 1] + h[n]) >> 1)
    movdqa xmm7, xmm5
    movd eax, xmm7
    pslldq xmm7, 2
    and eax, 0xFFFF
    movd xmm6, eax
    por xmm7, xmm6
    paddw xmm5, xmm7
    psraw xmm5, 1
    paddw xmm5, xmm1

    psrldq xmm2, 14
    movd ebx, xmm2                      ; save hi

    movdqa xmm6, xmm5                   ; out lo
    paddw xmm6, xmm10
    psraw xmm6, xmm11
    movdqa [rdx], xmm6

    ; move right
    lea rsi, [rsi + 16 * 2]
    lea rdi, [rdi + 8 * 2]
    lea rdx, [rdx + 8 * 2]

    ; post
    movdqa xmm1, [rsi]                  ; src[2n]
    movdqa xmm2, [rsi + 16]
    movdqa xmm6, xmm1
    movdqa xmm7, xmm2
    pand xmm1, [rel cdFFFF]
    pand xmm2, [rel cdFFFF]
    packusdw xmm1, xmm2
    movdqa xmm2, xmm6                   ; src[2n + 1]
    movdqa xmm3, xmm7
    psrldq xmm2, 2
    psrldq xmm3, 2
    pand xmm2, [rel cdFFFF]
    pand xmm3, [rel cdFFFF]
    packusdw xmm2, xmm3
    movdqa xmm3, xmm6                   ; src[2n + 2]
    movdqa xmm4, xmm7
    psrldq xmm3, 4
    psrldq xmm4, 4
    movd eax, xmm7
    movd xmm5, eax
    pslldq xmm5, 12
    por xmm3, xmm5
    movdqa xmm5, xmm7
    psrldq xmm5, 12
    pslldq xmm5, 12
    por xmm4, xmm5
    pand xmm3, [rel cdFFFF]
    pand xmm4, [rel cdFFFF]
    packusdw xmm3, xmm4
    movdqa xmm4, xmm1
    movdqa xmm5, xmm2
    movdqa xmm6, xmm3
    ; h[n] = (src[2n + 1] - ((src[2n] + src[2n + 2]) >> 1)) >> 1
    paddw xmm4, xmm6
    psraw xmm4, 1
    psubw xmm5, xmm4
    psraw xmm5, 1

    movdqa xmm6, xmm5                   ; out hi
    paddw xmm6, xmm8
    psraw xmm6, xmm9
    movdqa [rdi], xmm6

    ; l[n] = src[2n] + ((h[n - 1] + h[n]) >> 1)
    movdqa xmm7, xmm5
    pslldq xmm7, 2
    movd xmm6, ebx
    por xmm7, xmm6
    paddw xmm5, xmm7
    psraw xmm5, 1
    paddw xmm5, xmm1

    movdqa xmm6, xmm5                   ; out lo
    paddw xmm6, xmm10
    psraw xmm6, xmm11
    movdqa [rdx], xmm6

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

    dec ecx
    jnz loop1c

    ret

;******************************************************************************
; source 16 bit signed, 32 pixel width
rfx_dwt_2d_encode_block_horiz_16_32_no_lo:
    mov ecx, 16
loop1c1:
    ; pre
    movdqa xmm1, [rsi]                  ; src[2n]
    movdqa xmm2, [rsi + 16]
    movdqa xmm6, xmm1
    movdqa xmm7, xmm2
    pand xmm1, [rel cdFFFF]
    pand xmm2, [rel cdFFFF]
    packusdw xmm1, xmm2
    movdqa xmm2, xmm6                   ; src[2n + 1]
    movdqa xmm3, xmm7
    psrldq xmm2, 2
    psrldq xmm3, 2
    pand xmm2, [rel cdFFFF]
    pand xmm3, [rel cdFFFF]
    packusdw xmm2, xmm3
    movdqa xmm3, xmm6                   ; src[2n + 2]
    movdqa xmm4, xmm7
    psrldq xmm3, 4
    psrldq xmm4, 4
    movd eax, xmm7
    movd xmm5, eax
    pslldq xmm5, 12
    por xmm3, xmm5
    mov eax, [rsi + 32]
    movd xmm5, eax
    pslldq xmm5, 12
    por xmm4, xmm5
    pand xmm3, [rel cdFFFF]
    pand xmm4, [rel cdFFFF]
    packusdw xmm3, xmm4
    movdqa xmm4, xmm1
    movdqa xmm5, xmm2
    movdqa xmm6, xmm3
    ; h[n] = (src[2n + 1] - ((src[2n] + src[2n + 2]) >> 1)) >> 1
    paddw xmm4, xmm6
    psraw xmm4, 1
    psubw xmm5, xmm4
    psraw xmm5, 1

    movdqa xmm6, xmm5                   ; out hi
    paddw xmm6, xmm8
    psraw xmm6, xmm9
    movdqa [rdi], xmm6
    movdqa xmm2, xmm5                   ; save hi

    ; l[n] = src[2n] + ((h[n - 1] + h[n]) >> 1)
    movdqa xmm7, xmm5
    movd eax, xmm7
    pslldq xmm7, 2
    and eax, 0xFFFF
    movd xmm6, eax
    por xmm7, xmm6
    paddw xmm5, xmm7
    psraw xmm5, 1
    paddw xmm5, xmm1

    psrldq xmm2, 14
    movd ebx, xmm2                      ; save hi

    movdqa [rdx], xmm5                  ; out lo

    ; move right
    lea rsi, [rsi + 16 * 2]
    lea rdi, [rdi + 8 * 2]
    lea rdx, [rdx + 8 * 2]

    ; post
    movdqa xmm1, [rsi]                  ; src[2n]
    movdqa xmm2, [rsi + 16]
    movdqa xmm6, xmm1
    movdqa xmm7, xmm2
    pand xmm1, [rel cdFFFF]
    pand xmm2, [rel cdFFFF]
    packusdw xmm1, xmm2
    movdqa xmm2, xmm6                   ; src[2n + 1]
    movdqa xmm3, xmm7
    psrldq xmm2, 2
    psrldq xmm3, 2
    pand xmm2, [rel cdFFFF]
    pand xmm3, [rel cdFFFF]
    packusdw xmm2, xmm3
    movdqa xmm3, xmm6                   ; src[2n + 2]
    movdqa xmm4, xmm7
    psrldq xmm3, 4
    psrldq xmm4, 4
    movd eax, xmm7
    movd xmm5, eax
    pslldq xmm5, 12
    por xmm3, xmm5
    movdqa xmm5, xmm7
    psrldq xmm5, 12
    pslldq xmm5, 12
    por xmm4, xmm5
    pand xmm3, [rel cdFFFF]
    pand xmm4, [rel cdFFFF]
    packusdw xmm3, xmm4
    movdqa xmm4, xmm1
    movdqa xmm5, xmm2
    movdqa xmm6, xmm3
    ; h[n] = (src[2n + 1] - ((src[2n] + src[2n + 2]) >> 1)) >> 1
    paddw xmm4, xmm6
    psraw xmm4, 1
    psubw xmm5, xmm4
    psraw xmm5, 1

    movdqa xmm6, xmm5                   ; out hi
    paddw xmm6, xmm8
    psraw xmm6, xmm9
    movdqa [rdi], xmm6

    ; l[n] = src[2n] + ((h[n - 1] + h[n]) >> 1)
    movdqa xmm7, xmm5
    pslldq xmm7, 2
    movd xmm6, ebx
    por xmm7, xmm6
    paddw xmm5, xmm7
    psraw xmm5, 1
    paddw xmm5, xmm1

    movdqa [rdx], xmm5                  ; out lo

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

    dec ecx
    jnz loop1c1

    ret

;******************************************************************************
; source 16 bit signed, 32 pixel width
rfx_dwt_2d_encode_block_verti_16_32:
    mov ecx, 4
loop1d:
    ; pre
    movdqa xmm1, [rsi]                  ; src[2n]
    movdqa xmm2, [rsi + 32 * 2]         ; src[2n + 1]
    movdqa xmm3, [rsi + 32 * 2 * 2]     ; src[2n + 2]
    movdqa xmm4, xmm1
    movdqa xmm5, xmm2
    movdqa xmm6, xmm3
    ; h[n] = (src[2n + 1] - ((src[2n] + src[2n + 2]) >> 1)) >> 1
    paddw xmm4, xmm6
    psraw xmm4, 1
    psubw xmm5, xmm4
    psraw xmm5, 1
    movdqa [rdi], xmm5                  ; out hi
    movdqa xmm6, xmm5                   ; save hi
    ; l[n] = src[2n] + ((h[n - 1] + h[n]) >> 1)
    paddw xmm5, xmm1
    movdqa [rdx], xmm5                  ; out lo
    movdqa xmm7, xmm6                   ; save hi
    ; move down
    lea rsi, [rsi + 32 * 2 * 2]         ; 2 rows
    lea rdi, [rdi + 32 * 2]             ; 1 row
    lea rdx, [rdx + 32 * 2]             ; 1 row

    ; loop
    shl ecx, 16
    mov cx, 14
loop2d:
    movdqa xmm1, xmm3                   ; src[2n]
    movdqa xmm2, [rsi + 32 * 2]         ; src[2n + 1]
    movdqa xmm3, [rsi + 32 * 2 * 2]     ; src[2n + 2]
    movdqa xmm4, xmm1
    movdqa xmm5, xmm2
    movdqa xmm6, xmm3
    ; h[n] = (src[2n + 1] - ((src[2n] + src[2n + 2]) >> 1)) >> 1
    paddw xmm4, xmm6
    psraw xmm4, 1
    psubw xmm5, xmm4
    psraw xmm5, 1
    movdqa [rdi], xmm5                  ; out hi
    movdqa xmm6, xmm5                   ; save hi
    ; l[n] = src[2n] + ((h[n - 1] + h[n]) >> 1)
    paddw xmm5, xmm7
    psraw xmm5, 1
    paddw xmm5, xmm1
    movdqa [rdx], xmm5                  ; out lo
    movdqa xmm7, xmm6                   ; save hi
    ; move down
    lea rsi, [rsi + 32 * 2 * 2]         ; 2 rows
    lea rdi, [rdi + 32 * 2]             ; 1 row
    lea rdx, [rdx + 32 * 2]             ; 1 row

    dec cx
    jnz loop2d
    shr ecx, 16

    ; post
    movdqa xmm1, xmm3                   ; src[2n]
    movdqa xmm2, [rsi + 32 * 2]         ; src[2n + 1]
    movdqa xmm4, xmm1
    movdqa xmm5, xmm2
    movdqa xmm6, xmm3
    ; h[n] = (src[2n + 1] - ((src[2n] + src[2n + 2]) >> 1)) >> 1
    paddw xmm4, xmm6
    psraw xmm4, 1
    psubw xmm5, xmm4
    psraw xmm5, 1
    movdqa [rdi], xmm5                  ; out hi
    ; l[n] = src[2n] + ((h[n - 1] + h[n]) >> 1)
    paddw xmm5, xmm7
    psraw xmm5, 1
    paddw xmm5, xmm1
    movdqa [rdx], xmm5                  ; out lo
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

    dec ecx
    jnz loop1d

    ret

;******************************************************************************
; source 16 bit signed, 64 pixel width
rfx_dwt_2d_encode_block_horiz_16_64:
    mov ecx, 32
loop1e:
    ; pre
    movdqa xmm1, [rsi]                  ; src[2n]
    movdqa xmm2, [rsi + 16]
    movdqa xmm6, xmm1
    movdqa xmm7, xmm2
    pand xmm1, [rel cdFFFF]
    pand xmm2, [rel cdFFFF]
    packusdw xmm1, xmm2
    movdqa xmm2, xmm6                   ; src[2n + 1]
    movdqa xmm3, xmm7
    psrldq xmm2, 2
    psrldq xmm3, 2
    pand xmm2, [rel cdFFFF]
    pand xmm3, [rel cdFFFF]
    packusdw xmm2, xmm3
    movdqa xmm3, xmm6                   ; src[2n + 2]
    movdqa xmm4, xmm7
    psrldq xmm3, 4
    psrldq xmm4, 4
    movd eax, xmm7
    movd xmm5, eax
    pslldq xmm5, 12
    por xmm3, xmm5
    mov eax, [rsi + 32]
    movd xmm5, eax
    pslldq xmm5, 12
    por xmm4, xmm5
    pand xmm3, [rel cdFFFF]
    pand xmm4, [rel cdFFFF]
    packusdw xmm3, xmm4
    movdqa xmm4, xmm1
    movdqa xmm5, xmm2
    movdqa xmm6, xmm3
    ; h[n] = (src[2n + 1] - ((src[2n] + src[2n + 2]) >> 1)) >> 1
    paddw xmm4, xmm6
    psraw xmm4, 1
    psubw xmm5, xmm4
    psraw xmm5, 1

    movdqa xmm6, xmm5                   ; out hi
    paddw xmm6, xmm8
    psraw xmm6, xmm9
    movdqa [rdi], xmm6
    movdqa xmm2, xmm5                   ; save hi

    ; l[n] = src[2n] + ((h[n - 1] + h[n]) >> 1)
    movdqa xmm7, xmm5
    movd eax, xmm7
    pslldq xmm7, 2
    and eax, 0xFFFF
    movd xmm6, eax
    por xmm7, xmm6
    paddw xmm5, xmm7
    psraw xmm5, 1
    paddw xmm5, xmm1

    psrldq xmm2, 14
    movd ebx, xmm2                      ; save hi

    movdqa xmm6, xmm5                   ; out lo
    paddw xmm6, xmm10
    psraw xmm6, xmm11
    movdqa [rdx], xmm6

    ; move right
    lea rsi, [rsi + 16 * 2]
    lea rdi, [rdi + 8 * 2]
    lea rdx, [rdx + 8 * 2]

    ; loop
    shl ecx, 16
    mov cx, 2
loop2e:
    movdqa xmm1, [rsi]                  ; src[2n]
    movdqa xmm2, [rsi + 16]
    movdqa xmm6, xmm1
    movdqa xmm7, xmm2
    pand xmm1, [rel cdFFFF]
    pand xmm2, [rel cdFFFF]
    packusdw xmm1, xmm2
    movdqa xmm2, xmm6                   ; src[2n + 1]
    movdqa xmm3, xmm7
    psrldq xmm2, 2
    psrldq xmm3, 2
    pand xmm2, [rel cdFFFF]
    pand xmm3, [rel cdFFFF]
    packusdw xmm2, xmm3
    movdqa xmm3, xmm6                   ; src[2n + 2]
    movdqa xmm4, xmm7
    psrldq xmm3, 4
    psrldq xmm4, 4
    movd eax, xmm7
    movd xmm5, eax
    pslldq xmm5, 12
    por xmm3, xmm5
    mov eax, [rsi + 32]
    movd xmm5, eax
    pslldq xmm5, 12
    por xmm4, xmm5
    pand xmm3, [rel cdFFFF]
    pand xmm4, [rel cdFFFF]
    packusdw xmm3, xmm4
    movdqa xmm4, xmm1
    movdqa xmm5, xmm2
    movdqa xmm6, xmm3
    ; h[n] = (src[2n + 1] - ((src[2n] + src[2n + 2]) >> 1)) >> 1
    paddw xmm4, xmm6
    psraw xmm4, 1
    psubw xmm5, xmm4
    psraw xmm5, 1

    movdqa xmm6, xmm5                   ; out hi
    paddw xmm6, xmm8
    psraw xmm6, xmm9
    movdqa [rdi], xmm6
    movdqa xmm2, xmm5                   ; save hi

    ; l[n] = src[2n] + ((h[n - 1] + h[n]) >> 1)
    movdqa xmm7, xmm5
    pslldq xmm7, 2
    movd xmm6, ebx
    por xmm7, xmm6
    paddw xmm5, xmm7
    psraw xmm5, 1
    paddw xmm5, xmm1

    psrldq xmm2, 14
    movd ebx, xmm2                      ; save hi

    movdqa xmm6, xmm5                   ; out lo
    paddw xmm6, xmm10
    psraw xmm6, xmm11
    movdqa [rdx], xmm6

    ; move right
    lea rsi, [rsi + 16 * 2]
    lea rdi, [rdi + 8 * 2]
    lea rdx, [rdx + 8 * 2]

    dec cx
    jnz loop2e
    shr ecx, 16

    ; post
    movdqa xmm1, [rsi]                  ; src[2n]
    movdqa xmm2, [rsi + 16]
    movdqa xmm6, xmm1
    movdqa xmm7, xmm2
    pand xmm1, [rel cdFFFF]
    pand xmm2, [rel cdFFFF]
    packusdw xmm1, xmm2
    movdqa xmm2, xmm6                   ; src[2n + 1]
    movdqa xmm3, xmm7
    psrldq xmm2, 2
    psrldq xmm3, 2
    pand xmm2, [rel cdFFFF]
    pand xmm3, [rel cdFFFF]
    packusdw xmm2, xmm3
    movdqa xmm3, xmm6                   ; src[2n + 2]
    movdqa xmm4, xmm7
    psrldq xmm3, 4
    psrldq xmm4, 4
    movd eax, xmm7
    movd xmm5, eax
    pslldq xmm5, 12
    por xmm3, xmm5
    movdqa xmm5, xmm7
    psrldq xmm5, 12
    pslldq xmm5, 12
    por xmm4, xmm5
    pand xmm3, [rel cdFFFF]
    pand xmm4, [rel cdFFFF]
    packusdw xmm3, xmm4
    movdqa xmm4, xmm1
    movdqa xmm5, xmm2
    movdqa xmm6, xmm3
    ; h[n] = (src[2n + 1] - ((src[2n] + src[2n + 2]) >> 1)) >> 1
    paddw xmm4, xmm6
    psraw xmm4, 1
    psubw xmm5, xmm4
    psraw xmm5, 1

    movdqa xmm6, xmm5                   ; out hi
    paddw xmm6, xmm8
    psraw xmm6, xmm9
    movdqa [rdi], xmm6

    ; l[n] = src[2n] + ((h[n - 1] + h[n]) >> 1)
    movdqa xmm7, xmm5
    pslldq xmm7, 2
    movd xmm6, ebx
    por xmm7, xmm6
    paddw xmm5, xmm7
    psraw xmm5, 1
    paddw xmm5, xmm1

    movdqa xmm6, xmm5                   ; out lo
    paddw xmm6, xmm10
    psraw xmm6, xmm11
    movdqa [rdx], xmm6

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

    dec ecx
    jnz loop1e

    ret

;******************************************************************************
; source 16 bit signed, 64 pixel width
rfx_dwt_2d_encode_block_horiz_16_64_no_lo:
    mov ecx, 32
loop1e1:
    ; pre
    movdqa xmm1, [rsi]                  ; src[2n]
    movdqa xmm2, [rsi + 16]
    movdqa xmm6, xmm1
    movdqa xmm7, xmm2
    pand xmm1, [rel cdFFFF]
    pand xmm2, [rel cdFFFF]
    packusdw xmm1, xmm2
    movdqa xmm2, xmm6                   ; src[2n + 1]
    movdqa xmm3, xmm7
    psrldq xmm2, 2
    psrldq xmm3, 2
    pand xmm2, [rel cdFFFF]
    pand xmm3, [rel cdFFFF]
    packusdw xmm2, xmm3
    movdqa xmm3, xmm6                   ; src[2n + 2]
    movdqa xmm4, xmm7
    psrldq xmm3, 4
    psrldq xmm4, 4
    movd eax, xmm7
    movd xmm5, eax
    pslldq xmm5, 12
    por xmm3, xmm5
    mov eax, [rsi + 32]
    movd xmm5, eax
    pslldq xmm5, 12
    por xmm4, xmm5
    pand xmm3, [rel cdFFFF]
    pand xmm4, [rel cdFFFF]
    packusdw xmm3, xmm4
    movdqa xmm4, xmm1
    movdqa xmm5, xmm2
    movdqa xmm6, xmm3
    ; h[n] = (src[2n + 1] - ((src[2n] + src[2n + 2]) >> 1)) >> 1
    paddw xmm4, xmm6
    psraw xmm4, 1
    psubw xmm5, xmm4
    psraw xmm5, 1

    movdqa xmm6, xmm5                   ; out hi
    paddw xmm6, xmm8
    psraw xmm6, xmm9
    movdqa [rdi], xmm6
    movdqa xmm2, xmm5                   ; save hi

    ; l[n] = src[2n] + ((h[n - 1] + h[n]) >> 1)
    movdqa xmm7, xmm5
    movd eax, xmm7
    pslldq xmm7, 2
    and eax, 0xFFFF
    movd xmm6, eax
    por xmm7, xmm6
    paddw xmm5, xmm7
    psraw xmm5, 1
    paddw xmm5, xmm1

    psrldq xmm2, 14
    movd ebx, xmm2                      ; save hi

    movdqa [rdx], xmm5                  ; out lo

    ; move right
    lea rsi, [rsi + 16 * 2]
    lea rdi, [rdi + 8 * 2]
    lea rdx, [rdx + 8 * 2]

    ; loop
    shl ecx, 16
    mov cx, 2
loop2e1:
    movdqa xmm1, [rsi]                  ; src[2n]
    movdqa xmm2, [rsi + 16]
    movdqa xmm6, xmm1
    movdqa xmm7, xmm2
    pand xmm1, [rel cdFFFF]
    pand xmm2, [rel cdFFFF]
    packusdw xmm1, xmm2
    movdqa xmm2, xmm6                   ; src[2n + 1]
    movdqa xmm3, xmm7
    psrldq xmm2, 2
    psrldq xmm3, 2
    pand xmm2, [rel cdFFFF]
    pand xmm3, [rel cdFFFF]
    packusdw xmm2, xmm3
    movdqa xmm3, xmm6                   ; src[2n + 2]
    movdqa xmm4, xmm7
    psrldq xmm3, 4
    psrldq xmm4, 4
    movd eax, xmm7
    movd xmm5, eax
    pslldq xmm5, 12
    por xmm3, xmm5
    mov eax, [rsi + 32]
    movd xmm5, eax
    pslldq xmm5, 12
    por xmm4, xmm5
    pand xmm3, [rel cdFFFF]
    pand xmm4, [rel cdFFFF]
    packusdw xmm3, xmm4
    movdqa xmm4, xmm1
    movdqa xmm5, xmm2
    movdqa xmm6, xmm3
    ; h[n] = (src[2n + 1] - ((src[2n] + src[2n + 2]) >> 1)) >> 1
    paddw xmm4, xmm6
    psraw xmm4, 1
    psubw xmm5, xmm4
    psraw xmm5, 1

    movdqa xmm6, xmm5                   ; out hi
    paddw xmm6, xmm8
    psraw xmm6, xmm9
    movdqa [rdi], xmm6
    movdqa xmm2, xmm5                   ; save hi

    ; l[n] = src[2n] + ((h[n - 1] + h[n]) >> 1)
    movdqa xmm7, xmm5
    pslldq xmm7, 2
    movd xmm6, ebx
    por xmm7, xmm6
    paddw xmm5, xmm7
    psraw xmm5, 1
    paddw xmm5, xmm1

    psrldq xmm2, 14
    movd ebx, xmm2                      ; save hi

    movdqa [rdx], xmm5                  ; out lo

    ; move right
    lea rsi, [rsi + 16 * 2]
    lea rdi, [rdi + 8 * 2]
    lea rdx, [rdx + 8 * 2]

    dec cx
    jnz loop2e1
    shr ecx, 16

    ; post
    movdqa xmm1, [rsi]                  ; src[2n]
    movdqa xmm2, [rsi + 16]
    movdqa xmm6, xmm1
    movdqa xmm7, xmm2
    pand xmm1, [rel cdFFFF]
    pand xmm2, [rel cdFFFF]
    packusdw xmm1, xmm2
    movdqa xmm2, xmm6                   ; src[2n + 1]
    movdqa xmm3, xmm7
    psrldq xmm2, 2
    psrldq xmm3, 2
    pand xmm2, [rel cdFFFF]
    pand xmm3, [rel cdFFFF]
    packusdw xmm2, xmm3
    movdqa xmm3, xmm6                   ; src[2n + 2]
    movdqa xmm4, xmm7
    psrldq xmm3, 4
    psrldq xmm4, 4
    movd eax, xmm7
    movd xmm5, eax
    pslldq xmm5, 12
    por xmm3, xmm5
    movdqa xmm5, xmm7
    psrldq xmm5, 12
    pslldq xmm5, 12
    por xmm4, xmm5
    pand xmm3, [rel cdFFFF]
    pand xmm4, [rel cdFFFF]
    packusdw xmm3, xmm4
    movdqa xmm4, xmm1
    movdqa xmm5, xmm2
    movdqa xmm6, xmm3
    ; h[n] = (src[2n + 1] - ((src[2n] + src[2n + 2]) >> 1)) >> 1
    paddw xmm4, xmm6
    psraw xmm4, 1
    psubw xmm5, xmm4
    psraw xmm5, 1

    movdqa xmm6, xmm5                   ; out hi
    paddw xmm6, xmm8
    psraw xmm6, xmm9
    movdqa [rdi], xmm6

    ; l[n] = src[2n] + ((h[n - 1] + h[n]) >> 1)
    movdqa xmm7, xmm5
    pslldq xmm7, 2
    movd xmm6, ebx
    por xmm7, xmm6
    paddw xmm5, xmm7
    psraw xmm5, 1
    paddw xmm5, xmm1

    movdqa [rdx], xmm5                  ; out lo

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

    dec ecx
    jnz loop1e1

    ret

;******************************************************************************
; source 8 bit unsigned, 64 pixel width
rfx_dwt_2d_encode_block_verti_8_64:
    mov ecx, 8
loop1f:
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
    movdqa [rdi], xmm5                  ; out hi
    movdqa xmm6, xmm5                   ; save hi
    ; l[n] = src[2n] + ((h[n - 1] + h[n]) >> 1)
    paddw xmm5, xmm1
    movdqa [rdx], xmm5                  ; out lo
    movdqa xmm7, xmm6                   ; save hi
    ; move down
    lea rsi, [rsi + 64 * 1 * 2]         ; 2 rows
    lea rdi, [rdi + 64 * 2]             ; 1 row
    lea rdx, [rdx + 64 * 2]             ; 1 row

    ; loop
    shl ecx, 16
    mov cx, 30
loop2f:
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
    movdqa [rdi], xmm5                  ; out hi
    movdqa xmm6, xmm5                   ; save hi
    ; l[n] = src[2n] + ((h[n - 1] + h[n]) >> 1)
    paddw xmm5, xmm7
    psraw xmm5, 1
    paddw xmm5, xmm1
    movdqa [rdx], xmm5                  ; out lo
    movdqa xmm7, xmm6                   ; save hi
    ; move down
    lea rsi, [rsi + 64 * 1 * 2]         ; 2 rows
    lea rdi, [rdi + 64 * 2]             ; 1 row
    lea rdx, [rdx + 64 * 2]             ; 1 row

    dec cx
    jnz loop2f
    shr ecx, 16

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
    movdqa [rdi], xmm5                  ; out hi
    ; l[n] = src[2n] + ((h[n - 1] + h[n]) >> 1)
    paddw xmm5, xmm7
    psraw xmm5, 1
    paddw xmm5, xmm1
    movdqa [rdx], xmm5                  ; out lo
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

    dec ecx
    jnz loop1f

    ret

set_quants_hi:
    sub rax, 6 - 5
    movd xmm9, eax
    imul rax, 16
    lea rdx, [rel cwa0]
    add rdx, rax
    movdqa xmm8, [rdx]
    ret

set_quants_lo:
    sub rax, 6 - 5
    movd xmm11, eax
    imul rax, 16
    lea rdx, [rel cwa0]
    add rdx, rax
    movdqa xmm10, [rdx]
    ret

;The first six integer or pointer arguments are passed in registers
;RDI, RSI, RDX, RCX, R8, and R9

;int
;rfxcodec_encode_dwt_shift_amd64_sse41(const char *qtable,
;                                      unsigned char *in_buffer,
;                                      short *out_buffer,
;                                      short *work_buffer);

;******************************************************************************
%ifidn __OUTPUT_FORMAT__,elf64
PROC rfxcodec_encode_dwt_shift_amd64_sse41
%else
PROC _rfxcodec_encode_dwt_shift_amd64_sse41
%endif
    ; save registers
    push rbx
    push rdx
    push rcx
    push rsi
    push rdi
    pxor xmm0, xmm0

    ; verical DWT to work buffer, level 1
    mov rsi, [rsp + 8]                  ; src
    mov rdi, [rsp + 16]                 ; dst hi
    lea rdi, [rdi + 64 * 32 * 2]        ; dst hi
    mov rdx, [rsp + 16]                 ; dst lo
    call rfx_dwt_2d_encode_block_verti_8_64

    ; horizontal DWT to out buffer, level 1, part 1
    xor rax, rax
    mov rdx, [rsp]
    mov al, [rdx + 4]
    and al, 0xF
    call set_quants_hi
    mov rsi, [rsp + 16]                 ; src
    mov rdi, [rsp + 24]                 ; dst hi - HL1
    mov rdx, [rsp + 24]                 ; dst lo - LL1
    lea rdx, [rdx + 32 * 32 * 6]        ; dst lo - LL1
    call rfx_dwt_2d_encode_block_horiz_16_64_no_lo

    ; horizontal DWT to out buffer, level 1, part 2
    xor rax, rax
    mov rdx, [rsp]
    mov al, [rdx + 4]
    shr al, 4
    call set_quants_hi
    xor rax, rax
    mov rdx, [rsp]
    mov al, [rdx + 3]
    shr al, 4
    call set_quants_lo
    mov rsi, [rsp + 16]                 ; src
    lea rsi, [rsi + 64 * 32 * 2]        ; src
    mov rdi, [rsp + 24]                 ; dst hi - HH1
    lea rdi, [rdi + 32 * 32 * 4]        ; dst hi - HH1
    mov rdx, [rsp + 24]                 ; dst lo - LH1
    lea rdx, [rdx + 32 * 32 * 2]        ; dst lo - LH1
    call rfx_dwt_2d_encode_block_horiz_16_64

    ; verical DWT to work buffer, level 2
    mov rsi, [rsp + 24]                 ; src
    lea rsi, [rsi + 32 * 32 * 6]        ; src
    mov rdi, [rsp + 16]                 ; dst hi
    lea rdi, [rdi + 32 * 16 * 2]        ; dst hi
    mov rdx, [rsp + 16]                 ; dst lo
    call rfx_dwt_2d_encode_block_verti_16_32

    ; horizontal DWT to out buffer, level 2, part 1
    xor rax, rax
    mov rdx, [rsp]
    mov al, [rdx + 2]
    shr al, 4
    call set_quants_hi
    mov rsi, [rsp + 16]                 ; src
    ; 32 * 32 * 6 + 16 * 16 * 0 = 6144
    mov rdi, [rsp + 24]                 ; dst hi - HL2
    lea rdi, [rdi + 6144]               ; dst hi - HL2
    ; 32 * 32 * 6 + 16 * 16 * 6 = 7680
    mov rdx, [rsp + 24]                 ; dst lo - LL2
    lea rdx, [rdx + 7680]               ; dst lo - LL2
    call rfx_dwt_2d_encode_block_horiz_16_32_no_lo

    ; horizontal DWT to out buffer, level 2, part 2
    xor rax, rax
    mov rdx, [rsp]
    mov al, [rdx + 3]
    and al, 0xF
    call set_quants_hi
    xor rax, rax
    mov rdx, [rsp]
    mov al, [rdx + 2]
    and al, 0xF
    call set_quants_lo
    mov rsi, [rsp + 16]                 ; src
    lea rsi, [rsi + 32 * 16 * 2]        ; src
    ; 32 * 32 * 6 + 16 * 16 * 4 = 7168
    mov rdi, [rsp + 24]                 ; dst hi - HH2
    lea rdi, [rdi + 7168]               ; dst hi - HH2
    ; 32 * 32 * 6 + 16 * 16 * 2 = 6656
    mov rdx, [rsp + 24]                 ; dst lo - LH2
    lea rdx, [rdx + 6656]               ; dst lo - LH2
    call rfx_dwt_2d_encode_block_horiz_16_32

    ; verical DWT to work buffer, level 3
    ; 32 * 32 * 6 + 16 * 16 * 6 = 7680
    mov rsi, [rsp + 24]                 ; src
    lea rsi, [rsi + 7680]               ; src
    mov rdi, [rsp + 16]                 ; dst hi
    lea rdi, [rdi + 16 * 8 * 2]         ; dst hi
    mov rdx, [rsp + 16]                 ; dst lo
    call rfx_dwt_2d_encode_block_verti_16_16

    ; horizontal DWT to out buffer, level 3, part 1
    xor rax, rax
    mov rdx, [rsp]
    mov al, [rdx + 1]
    and al, 0xF
    call set_quants_hi
    xor rax, rax
    mov rdx, [rsp]
    mov al, [rdx + 0]
    and al, 0xF
    call set_quants_lo
    mov rsi, [rsp + 16]                 ; src
    ; 32 * 32 * 6 + 16 * 16 * 6 + 8 * 8 * 0 = 7680
    mov rdi, [rsp + 24]                 ; dst hi - HL3
    lea rdi, [rdi + 7680]               ; dst hi - HL3
    ; 32 * 32 * 6 + 16 * 16 * 6 + 8 * 8 * 6 = 8064
    mov rdx, [rsp + 24]                 ; dst lo - LL3
    lea rdx, [rdx + 8064]               ; dst lo - LL3
    call rfx_dwt_2d_encode_block_horiz_16_16

    ; horizontal DWT to out buffer, level 3, part 2
    xor rax, rax
    mov rdx, [rsp]
    mov al, [rdx + 1]
    shr al, 4
    call set_quants_hi
    xor rax, rax
    mov rdx, [rsp]
    mov al, [rdx + 0]
    shr al, 4
    call set_quants_lo
    mov rsi, [rsp + 16]                 ; src
    lea rsi, [rsi + 16 * 8 * 2]         ; src
    ; 32 * 32 * 6 + 16 * 16 * 6 + 8 * 8 * 4 = 7936
    mov rdi, [rsp + 24]                 ; dst hi - HH3
    lea rdi, [rdi + 7936]               ; dst hi - HH3
    ; 32 * 32 * 6 + 16 * 16 * 6 + 8 * 8 * 2 = 7808
    mov rdx, [rsp + 24]                 ; dst lo - LH3
    lea rdx, [rdx + 7808]               ; dst lo - LH3
    call rfx_dwt_2d_encode_block_horiz_16_16

    mov rax, 0
    ; restore registers
    pop rdi
    pop rsi
    pop rcx
    pop rdx
    pop rbx
    ret
    align 16

