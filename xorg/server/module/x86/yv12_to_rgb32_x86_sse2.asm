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
;YV12 to RGB32
;x86 SSE2 32 bit
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

    ; u
    movd xmm1, [ebx]     ; 4 at a time
    lea ebx, [ebx + 4]
    punpcklbw xmm1, xmm1
    pxor xmm6, xmm6
    punpcklbw xmm1, xmm6
    movdqa xmm7, [c128]
    psubw xmm1, xmm7
    psllw xmm1, 4

    ; v
    movd xmm2, [edx]     ; 4 at a time
    lea edx, [edx + 4]
    punpcklbw xmm2, xmm2
    punpcklbw xmm2, xmm6
    psubw xmm2, xmm7
    psllw xmm2, 4

do8:

    ; y
    movq xmm0, [esi]     ; 8 at a time
    lea esi, [esi + 8]
    pxor xmm6, xmm6
    punpcklbw xmm0, xmm6

    ; r = y + hiword(4669 * (v << 4))
    movdqa xmm4, [c4669]
    pmulhw xmm4, xmm2
    movdqa xmm3, xmm0
    paddw xmm3, xmm4

    ; g = y - hiword(1616 * (u << 4)) - hiword(2378 * (v << 4))
    movdqa xmm5, [c1616]
    pmulhw xmm5, xmm1
    movdqa xmm6, [c2378]
    pmulhw xmm6, xmm2
    movdqa xmm4, xmm0
    psubw xmm4, xmm5
    psubw xmm4, xmm6

    ; b = y + hiword(9324 * (u << 4))
    movdqa xmm6, [c9324]
    pmulhw xmm6, xmm1
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
    movdqa [edi], xmm3
    lea edi, [edi + 16]
    punpckhwd xmm4, xmm5 ; argb
    movdqa [edi], xmm4
    lea edi, [edi + 16]

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
    ; char* yptr1
    ; char* yptr2
    ; char* uptr
    ; char* vptr
    ; int* rgbs1
    ; int* rgbs2
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
    shr ecx, 3

    ; save edx
    mov [esp + 24], edx

    prefetchnta 4096[esp + 0]  ; y
    prefetchnta 1024[esp + 8]  ; u
    prefetchnta 1024[esp + 12] ; v

loop_x:

    mov esi, [esp + 0]  ; y1
    mov ebx, [esp + 8]  ; u
    mov edx, [esp + 12] ; v
    mov edi, [esp + 16] ; rgbs1

    ; y1
    call do8_uv

    mov [esp + 0], esi  ; y1
    mov [esp + 16], edi ; rgbs1

    mov esi, [esp + 4]  ; y2
    mov edi, [esp + 20] ; rgbs2

    ; y2
    call do8

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


