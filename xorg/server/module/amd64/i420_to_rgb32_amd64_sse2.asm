;
;Copyright 2014 Jay Sorg
;
;Permission to use, copy, modify, distribute, and sell this software and its
;documentation for any purpose is hereby granted without fee, provided that
;the above copyright notice appear in all copies and that both that
;copyright notice and this permission notice appear in supporting
;documentation.
;
;The above copyright notice and this permission notice shall be included in
;all copies or substantial portions of the Software.
;
;THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
;IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
;FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
;OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
;AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
;CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
;
;I420 to RGB32
;amd64 SSE2 32 bit
;
; RGB to YUV
;   0.299    0.587    0.114
;  -0.14713 -0.28886  0.436
;   0.615   -0.51499 -0.10001
; YUV to RGB
;   1        0        1.13983
;   1       -0.39465 -0.58060
;   1        2.03211  0
; shift left 12
;   4096     0        4669
;   4096    -1616    -2378
;   4096     9324     0

SECTION .data
align 16
c128 times 8 dw 128
c4669 times 8 dw 4669
c1616 times 8 dw 1616
c2378 times 8 dw 2378
c9324 times 8 dw 9324

SECTION .text

%macro PROC 1
    align 16
    global %1
    %1:
%endmacro

do8_uv:

    ; v
    movd xmm1, [rbx]     ; 4 at a time
    lea rbx, [rbx + 4]
    punpcklbw xmm1, xmm1
    pxor xmm6, xmm6
    punpcklbw xmm1, xmm6
    movdqa xmm7, [rel c128]
    psubw xmm1, xmm7
    psllw xmm1, 4

    ; v
    movd xmm2, [rdx]     ; 4 at a time
    lea rdx, [rdx + 4]
    punpcklbw xmm2, xmm2
    punpcklbw xmm2, xmm6
    psubw xmm2, xmm7
    psllw xmm2, 4

do8:

    ; y
    movq xmm0, [rsi]     ; 8 at a time
    lea rsi, [rsi + 8]
    pxor xmm6, xmm6
    punpcklbw xmm0, xmm6

    ; r = y + hiword(4669 * (v << 4))
    movdqa xmm4, [rel c4669]
    pmulhw xmm4, xmm1
    movdqa xmm3, xmm0
    paddw xmm3, xmm4

    ; g = y - hiword(1616 * (u << 4)) - hiword(2378 * (v << 4))
    movdqa xmm5, [rel c1616]
    pmulhw xmm5, xmm2
    movdqa xmm6, [rel c2378]
    pmulhw xmm6, xmm1
    movdqa xmm4, xmm0
    psubw xmm4, xmm5
    psubw xmm4, xmm6

    ; b = y + hiword(9324 * (u << 4))
    movdqa xmm6, [rel c9324]
    pmulhw xmm6, xmm2
    movdqa xmm5, xmm0
    paddw xmm5, xmm6

    packuswb xmm3, xmm3  ; b
    packuswb xmm4, xmm4  ; g
    punpcklbw xmm3, xmm4 ; gb

    pxor xmm4, xmm4      ; a
    packuswb xmm5, xmm5  ; r
    punpcklbw xmm5, xmm4 ; ar

    movdqa xmm4, xmm3
    punpcklwd xmm3, xmm5 ; argb
    movdqa [rdi], xmm3
    lea rdi, [rdi + 16]
    punpckhwd xmm4, xmm5 ; argb
    movdqa [rdi], xmm4
    lea rdi, [rdi + 16]

    ret;

;The first six integer or pointer arguments are passed in registers
; RDI, RSI, RDX, RCX, R8, and R9

;int
;i420_to_rgb32_amd64_sse2(unsigned char *yuvs, int width, int height, int *rgbs)

PROC i420_to_rgb32_amd64_sse2
    push rbx
    push rsi
    push rdi
    push rbp

    push rdi
    push rdx
    mov rdi, rcx        ; rgbs

    mov rcx, rsi        ; width
    mov rdx, rcx
    pop rbp             ; height
    mov rax, rbp
    shr rbp, 1
    imul rax, rcx       ; rax = width * height

    pop rsi             ; y

    mov rbx, rsi        ; u = y + width * height
    add rbx, rax

    ; local vars
    ; char* yptr1
    ; char* yptr2
    ; char* uptr
    ; char* vptr
    ; int* rgbs1
    ; int* rgbs2
    ; int width
    sub rsp, 56         ; local vars, 56 bytes
    mov [rsp + 0], rsi  ; save y1
    add rsi, rdx
    mov [rsp + 8], rsi  ; save y2
    mov [rsp + 16], rbx ; save u
    shr rax, 2
    add rbx, rax        ; v = u + (width * height / 4)
    mov [rsp + 24], rbx ; save v

    mov [rsp + 32], rdi ; save rgbs1
    mov rax, rdx
    shl rax, 2
    add rdi, rax
    mov [rsp + 40], rdi ; save rgbs2

loop_y:

    mov rcx, rdx        ; width
    shr rcx, 3

    ; save rdx
    mov [rsp + 48], rdx

    ;prefetchnta 4096[rsp + 0]  ; y
    ;prefetchnta 1024[rsp + 16] ; u
    ;prefetchnta 1024[rsp + 24] ; v

loop_x:

    mov rsi, [rsp + 0]  ; y1
    mov rbx, [rsp + 16] ; u
    mov rdx, [rsp + 24] ; v
    mov rdi, [rsp + 32] ; rgbs1

    ; y1
    call do8_uv

    mov [rsp + 0], rsi  ; y1
    mov [rsp + 32], rdi ; rgbs1

    mov rsi, [rsp + 8]  ; y2
    mov rdi, [rsp + 40] ; rgbs2

    ; y2
    call do8

    mov [rsp + 8], rsi  ; y2
    mov [rsp + 16], rbx ; u
    mov [rsp + 24], rdx ; v
    mov [rsp + 40], rdi ; rgbs2

    dec rcx             ; width
    jnz loop_x

    ; restore rdx
    mov rdx, [rsp + 48]

    ; update y1 and 2
    mov rax, [rsp + 0]
    mov rbx, rdx
    add rax, rbx
    mov [rsp + 0], rax

    mov rax, [rsp + 8]
    add rax, rbx
    mov [rsp + 8], rax

    ; update rgb1 and 2
    mov rax, [rsp + 32]
    mov rbx, rdx
    shl rbx, 2
    add rax, rbx
    mov [rsp + 32], rax

    mov rax, [rsp + 40]
    add rax, rbx
    mov [rsp + 40], rax

    mov rcx, rbp
    dec rcx             ; height
    mov rbp, rcx
    jnz loop_y

    add rsp, 56

    mov rax, 0
    pop rbp
    pop rdi
    pop rsi
    pop rbx
    ret
    align 16


