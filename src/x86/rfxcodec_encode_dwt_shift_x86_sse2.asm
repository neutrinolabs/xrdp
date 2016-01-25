;
;Copyright 2016 Jay Sorg
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
;x86 asm dwt

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

%define LHI_ADD  [esp + 1 * 16 + 4]
%define LHI_SFT  [esp + 2 * 16 + 4]
%define LLO_ADD  [esp + 3 * 16 + 4]
%define LLO_SFT  [esp + 4 * 16 + 4]

;******************************************************************************
; source 16 bit signed, 16 pixel width
rfx_dwt_2d_encode_block_horiz_16_16:
    mov ecx, 8
loop1a:
    ; pre / post
    movdqa xmm1, [esi]                  ; src[2n]
    movdqa xmm2, [esi + 16]
    movdqa xmm6, xmm1
    movdqa xmm7, xmm2
    pand xmm1, [cdFFFF]
    pand xmm2, [cdFFFF]
    pslld xmm1, 16
    pslld xmm2, 16
    psrad xmm1, 16
    psrad xmm2, 16
    packssdw xmm1, xmm2
    movdqa xmm2, xmm6                   ; src[2n + 1]
    movdqa xmm3, xmm7
    psrldq xmm2, 2
    psrldq xmm3, 2
    pand xmm2, [cdFFFF]
    pand xmm3, [cdFFFF]
    pslld xmm2, 16
    pslld xmm3, 16
    psrad xmm2, 16
    psrad xmm3, 16
    packssdw xmm2, xmm3
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
    pand xmm3, [cdFFFF]
    pand xmm4, [cdFFFF]
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
    movdqa xmm6, xmm5                   ; out hi
    paddw xmm6, LHI_ADD
    psraw xmm6, LHI_SFT
    movdqa [edi], xmm6
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
    paddw xmm6, LLO_ADD
    psraw xmm6, LLO_SFT
    movdqa [edx], xmm6

    ; move right
    lea esi, [esi + 16 * 2]
    lea edi, [edi + 8 * 2]
    lea edx, [edx + 8 * 2]

    ; move left
    lea esi, [esi - 16 * 2]
    lea edi, [edi - 8 * 2]
    lea edx, [edx - 8 * 2]

    ; move down
    lea esi, [esi + 16 * 2]
    lea edi, [edi + 8 * 2]
    lea edx, [edx + 8 * 2]

    dec ecx
    jnz loop1a

    ret

;******************************************************************************
; source 16 bit signed, 16 pixel width
rfx_dwt_2d_encode_block_verti_16_16:
    mov ecx, 2
loop1b:
    ; pre
    movdqa xmm1, [esi]                  ; src[2n]
    movdqa xmm2, [esi + 16 * 2]         ; src[2n + 1]
    movdqa xmm3, [esi + 16 * 2 * 2]     ; src[2n + 2]
    movdqa xmm4, xmm1
    movdqa xmm5, xmm2
    movdqa xmm6, xmm3
    ; h[n] = (src[2n + 1] - ((src[2n] + src[2n + 2]) >> 1)) >> 1
    paddw xmm4, xmm6
    psraw xmm4, 1
    psubw xmm5, xmm4
    psraw xmm5, 1
    movdqa [edi], xmm5                  ; out hi
    movdqa xmm6, xmm5                   ; save hi
    ; l[n] = src[2n] + ((h[n - 1] + h[n]) >> 1)
    paddw xmm5, xmm1
    movdqa [edx], xmm5                  ; out lo
    movdqa xmm7, xmm6                   ; save hi
    ; move down
    lea esi, [esi + 16 * 2 * 2]         ; 2 rows
    lea edi, [edi + 16 * 2]             ; 1 row
    lea edx, [edx + 16 * 2]             ; 1 row

    ; loop
    shl ecx, 16
    mov cx, 6
loop2b:
    movdqa xmm1, xmm3                   ; src[2n]
    movdqa xmm2, [esi + 16 * 2]         ; src[2n + 1]
    movdqa xmm3, [esi + 16 * 2 * 2]     ; src[2n + 2]
    movdqa xmm4, xmm1
    movdqa xmm5, xmm2
    movdqa xmm6, xmm3
    ; h[n] = (src[2n + 1] - ((src[2n] + src[2n + 2]) >> 1)) >> 1
    paddw xmm4, xmm6
    psraw xmm4, 1
    psubw xmm5, xmm4
    psraw xmm5, 1
    movdqa [edi], xmm5                  ; out hi
    movdqa xmm6, xmm5                   ; save hi
    ; l[n] = src[2n] + ((h[n - 1] + h[n]) >> 1)
    paddw xmm5, xmm7
    psraw xmm5, 1
    paddw xmm5, xmm1
    movdqa [edx], xmm5                  ; out lo
    movdqa xmm7, xmm6                   ; save hi
    ; move down
    lea esi, [esi + 16 * 2 * 2]         ; 2 rows
    lea edi, [edi + 16 * 2]             ; 1 row
    lea edx, [edx + 16 * 2]             ; 1 row

    dec cx
    jnz loop2b
    shr ecx, 16

    ; post
    movdqa xmm1, xmm3                   ; src[2n]
    movdqa xmm2, [esi + 16 * 2]         ; src[2n + 1]
    movdqa xmm4, xmm1
    movdqa xmm5, xmm2
    movdqa xmm6, xmm3
    ; h[n] = (src[2n + 1] - ((src[2n] + src[2n + 2]) >> 1)) >> 1
    paddw xmm4, xmm6
    psraw xmm4, 1
    psubw xmm5, xmm4
    psraw xmm5, 1
    movdqa [edi], xmm5                  ; out hi
    ; l[n] = src[2n] + ((h[n - 1] + h[n]) >> 1)
    paddw xmm5, xmm7
    psraw xmm5, 1
    paddw xmm5, xmm1
    movdqa [edx], xmm5                  ; out lo
    ; move down
    lea esi, [esi + 16 * 2 * 2]         ; 2 row
    lea edi, [edi + 16 * 2]             ; 1 row
    lea edx, [edx + 16 * 2]             ; 1 row

    ; move up
    lea esi, [esi - 16 * 16 * 2]
    lea edi, [edi - 8 * 16 * 2]
    lea edx, [edx - 8 * 16 * 2]

    ; move right
    lea esi, [esi + 16]
    lea edi, [edi + 16]
    lea edx, [edx + 16]

    dec ecx
    jnz loop1b

    ret

;******************************************************************************
; source 16 bit signed, 32 pixel width
rfx_dwt_2d_encode_block_horiz_16_32:
    mov ecx, 16
loop1c:
    ; pre
    movdqa xmm1, [esi]                  ; src[2n]
    movdqa xmm2, [esi + 16]
    movdqa xmm6, xmm1
    movdqa xmm7, xmm2
    pand xmm1, [cdFFFF]
    pand xmm2, [cdFFFF]
    pslld xmm1, 16
    pslld xmm2, 16
    psrad xmm1, 16
    psrad xmm2, 16
    packssdw xmm1, xmm2
    movdqa xmm2, xmm6                   ; src[2n + 1]
    movdqa xmm3, xmm7
    psrldq xmm2, 2
    psrldq xmm3, 2
    pand xmm2, [cdFFFF]
    pand xmm3, [cdFFFF]
    pslld xmm2, 16
    pslld xmm3, 16
    psrad xmm2, 16
    psrad xmm3, 16
    packssdw xmm2, xmm3
    movdqa xmm3, xmm6                   ; src[2n + 2]
    movdqa xmm4, xmm7
    psrldq xmm3, 4
    psrldq xmm4, 4
    movd eax, xmm7
    movd xmm5, eax
    pslldq xmm5, 12
    por xmm3, xmm5
    mov eax, [esi + 32]
    movd xmm5, eax
    pslldq xmm5, 12
    por xmm4, xmm5
    pand xmm3, [cdFFFF]
    pand xmm4, [cdFFFF]
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

    movdqa xmm6, xmm5                   ; out hi
    paddw xmm6, LHI_ADD
    psraw xmm6, LHI_SFT
    movdqa [edi], xmm6
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
    paddw xmm6, LLO_ADD
    psraw xmm6, LLO_SFT
    movdqa [edx], xmm6

    ; move right
    lea esi, [esi + 16 * 2]
    lea edi, [edi + 8 * 2]
    lea edx, [edx + 8 * 2]

    ; post
    movdqa xmm1, [esi]                  ; src[2n]
    movdqa xmm2, [esi + 16]
    movdqa xmm6, xmm1
    movdqa xmm7, xmm2
    pand xmm1, [cdFFFF]
    pand xmm2, [cdFFFF]
    pslld xmm1, 16
    pslld xmm2, 16
    psrad xmm1, 16
    psrad xmm2, 16
    packssdw xmm1, xmm2
    movdqa xmm2, xmm6                   ; src[2n + 1]
    movdqa xmm3, xmm7
    psrldq xmm2, 2
    psrldq xmm3, 2
    pand xmm2, [cdFFFF]
    pand xmm3, [cdFFFF]
    pslld xmm2, 16
    pslld xmm3, 16
    psrad xmm2, 16
    psrad xmm3, 16
    packssdw xmm2, xmm3
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
    pand xmm3, [cdFFFF]
    pand xmm4, [cdFFFF]
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

    movdqa xmm6, xmm5                   ; out hi
    paddw xmm6, LHI_ADD
    psraw xmm6, LHI_SFT
    movdqa [edi], xmm6

    ; l[n] = src[2n] + ((h[n - 1] + h[n]) >> 1)
    movdqa xmm7, xmm5
    pslldq xmm7, 2
    movd xmm6, ebx
    por xmm7, xmm6
    paddw xmm5, xmm7
    psraw xmm5, 1
    paddw xmm5, xmm1

    movdqa xmm6, xmm5                   ; out lo
    paddw xmm6, LLO_ADD
    psraw xmm6, LLO_SFT
    movdqa [edx], xmm6

    ; move right
    lea esi, [esi + 16 * 2]
    lea edi, [edi + 8 * 2]
    lea edx, [edx + 8 * 2]

    ; move left
    lea esi, [esi - 32 * 2]
    lea edi, [edi - 16 * 2]
    lea edx, [edx - 16 * 2]

    ; move down
    lea esi, [esi + 32 * 2]
    lea edi, [edi + 16 * 2]
    lea edx, [edx + 16 * 2]

    dec ecx
    jnz loop1c

    ret

;******************************************************************************
; source 16 bit signed, 32 pixel width
rfx_dwt_2d_encode_block_horiz_16_32_no_lo:
    mov ecx, 16
loop1c1:
    ; pre
    movdqa xmm1, [esi]                  ; src[2n]
    movdqa xmm2, [esi + 16]
    movdqa xmm6, xmm1
    movdqa xmm7, xmm2
    pand xmm1, [cdFFFF]
    pand xmm2, [cdFFFF]
    pslld xmm1, 16
    pslld xmm2, 16
    psrad xmm1, 16
    psrad xmm2, 16
    packssdw xmm1, xmm2
    movdqa xmm2, xmm6                   ; src[2n + 1]
    movdqa xmm3, xmm7
    psrldq xmm2, 2
    psrldq xmm3, 2
    pand xmm2, [cdFFFF]
    pand xmm3, [cdFFFF]
    pslld xmm2, 16
    pslld xmm3, 16
    psrad xmm2, 16
    psrad xmm3, 16
    packssdw xmm2, xmm3
    movdqa xmm3, xmm6                   ; src[2n + 2]
    movdqa xmm4, xmm7
    psrldq xmm3, 4
    psrldq xmm4, 4
    movd eax, xmm7
    movd xmm5, eax
    pslldq xmm5, 12
    por xmm3, xmm5
    mov eax, [esi + 32]
    movd xmm5, eax
    pslldq xmm5, 12
    por xmm4, xmm5
    pand xmm3, [cdFFFF]
    pand xmm4, [cdFFFF]
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

    movdqa xmm6, xmm5                   ; out hi
    paddw xmm6, LHI_ADD
    psraw xmm6, LHI_SFT
    movdqa [edi], xmm6
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

    movdqa [edx], xmm5                  ; out lo

    ; move right
    lea esi, [esi + 16 * 2]
    lea edi, [edi + 8 * 2]
    lea edx, [edx + 8 * 2]

    ; post
    movdqa xmm1, [esi]                  ; src[2n]
    movdqa xmm2, [esi + 16]
    movdqa xmm6, xmm1
    movdqa xmm7, xmm2
    pand xmm1, [cdFFFF]
    pand xmm2, [cdFFFF]
    pslld xmm1, 16
    pslld xmm2, 16
    psrad xmm1, 16
    psrad xmm2, 16
    packssdw xmm1, xmm2
    movdqa xmm2, xmm6                   ; src[2n + 1]
    movdqa xmm3, xmm7
    psrldq xmm2, 2
    psrldq xmm3, 2
    pand xmm2, [cdFFFF]
    pand xmm3, [cdFFFF]
    pslld xmm2, 16
    pslld xmm3, 16
    psrad xmm2, 16
    psrad xmm3, 16
    packssdw xmm2, xmm3
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
    pand xmm3, [cdFFFF]
    pand xmm4, [cdFFFF]
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

    movdqa xmm6, xmm5                   ; out hi
    paddw xmm6, LHI_ADD
    psraw xmm6, LHI_SFT
    movdqa [edi], xmm6

    ; l[n] = src[2n] + ((h[n - 1] + h[n]) >> 1)
    movdqa xmm7, xmm5
    pslldq xmm7, 2
    movd xmm6, ebx
    por xmm7, xmm6
    paddw xmm5, xmm7
    psraw xmm5, 1
    paddw xmm5, xmm1

    movdqa [edx], xmm5                  ; out lo

    ; move right
    lea esi, [esi + 16 * 2]
    lea edi, [edi + 8 * 2]
    lea edx, [edx + 8 * 2]

    ; move left
    lea esi, [esi - 32 * 2]
    lea edi, [edi - 16 * 2]
    lea edx, [edx - 16 * 2]

    ; move down
    lea esi, [esi + 32 * 2]
    lea edi, [edi + 16 * 2]
    lea edx, [edx + 16 * 2]

    dec ecx
    jnz loop1c1

    ret

;******************************************************************************
; source 16 bit signed, 32 pixel width
rfx_dwt_2d_encode_block_verti_16_32:
    mov ecx, 4
loop1d:
    ; pre
    movdqa xmm1, [esi]                  ; src[2n]
    movdqa xmm2, [esi + 32 * 2]         ; src[2n + 1]
    movdqa xmm3, [esi + 32 * 2 * 2]     ; src[2n + 2]
    movdqa xmm4, xmm1
    movdqa xmm5, xmm2
    movdqa xmm6, xmm3
    ; h[n] = (src[2n + 1] - ((src[2n] + src[2n + 2]) >> 1)) >> 1
    paddw xmm4, xmm6
    psraw xmm4, 1
    psubw xmm5, xmm4
    psraw xmm5, 1
    movdqa [edi], xmm5                  ; out hi
    movdqa xmm6, xmm5                   ; save hi
    ; l[n] = src[2n] + ((h[n - 1] + h[n]) >> 1)
    paddw xmm5, xmm1
    movdqa [edx], xmm5                  ; out lo
    movdqa xmm7, xmm6                   ; save hi
    ; move down
    lea esi, [esi + 32 * 2 * 2]         ; 2 rows
    lea edi, [edi + 32 * 2]             ; 1 row
    lea edx, [edx + 32 * 2]             ; 1 row

    ; loop
    shl ecx, 16
    mov cx, 14
loop2d:
    movdqa xmm1, xmm3                   ; src[2n]
    movdqa xmm2, [esi + 32 * 2]         ; src[2n + 1]
    movdqa xmm3, [esi + 32 * 2 * 2]     ; src[2n + 2]
    movdqa xmm4, xmm1
    movdqa xmm5, xmm2
    movdqa xmm6, xmm3
    ; h[n] = (src[2n + 1] - ((src[2n] + src[2n + 2]) >> 1)) >> 1
    paddw xmm4, xmm6
    psraw xmm4, 1
    psubw xmm5, xmm4
    psraw xmm5, 1
    movdqa [edi], xmm5                  ; out hi
    movdqa xmm6, xmm5                   ; save hi
    ; l[n] = src[2n] + ((h[n - 1] + h[n]) >> 1)
    paddw xmm5, xmm7
    psraw xmm5, 1
    paddw xmm5, xmm1
    movdqa [edx], xmm5                  ; out lo
    movdqa xmm7, xmm6                   ; save hi
    ; move down
    lea esi, [esi + 32 * 2 * 2]         ; 2 rows
    lea edi, [edi + 32 * 2]             ; 1 row
    lea edx, [edx + 32 * 2]             ; 1 row

    dec cx
    jnz loop2d
    shr ecx, 16

    ; post
    movdqa xmm1, xmm3                   ; src[2n]
    movdqa xmm2, [esi + 32 * 2]         ; src[2n + 1]
    movdqa xmm4, xmm1
    movdqa xmm5, xmm2
    movdqa xmm6, xmm3
    ; h[n] = (src[2n + 1] - ((src[2n] + src[2n + 2]) >> 1)) >> 1
    paddw xmm4, xmm6
    psraw xmm4, 1
    psubw xmm5, xmm4
    psraw xmm5, 1
    movdqa [edi], xmm5                  ; out hi
    ; l[n] = src[2n] + ((h[n - 1] + h[n]) >> 1)
    paddw xmm5, xmm7
    psraw xmm5, 1
    paddw xmm5, xmm1
    movdqa [edx], xmm5                  ; out lo
    ; move down
    lea esi, [esi + 32 * 2 * 2]         ; 2 row
    lea edi, [edi + 32 * 2]             ; 1 row
    lea edx, [edx + 32 * 2]             ; 1 row

    ; move up
    lea esi, [esi - 32 * 32 * 2]
    lea edi, [edi - 16 * 32 * 2]
    lea edx, [edx - 16 * 32 * 2]

    ; move right
    lea esi, [esi + 16]
    lea edi, [edi + 16]
    lea edx, [edx + 16]

    dec ecx
    jnz loop1d

    ret

;******************************************************************************
; source 16 bit signed, 64 pixel width
rfx_dwt_2d_encode_block_horiz_16_64:
    mov ecx, 32
loop1e:
    ; pre
    movdqa xmm1, [esi]                  ; src[2n]
    movdqa xmm2, [esi + 16]
    movdqa xmm6, xmm1
    movdqa xmm7, xmm2
    pand xmm1, [cdFFFF]
    pand xmm2, [cdFFFF]
    pslld xmm1, 16
    pslld xmm2, 16
    psrad xmm1, 16
    psrad xmm2, 16
    packssdw xmm1, xmm2
    movdqa xmm2, xmm6                   ; src[2n + 1]
    movdqa xmm3, xmm7
    psrldq xmm2, 2
    psrldq xmm3, 2
    pand xmm2, [cdFFFF]
    pand xmm3, [cdFFFF]
    pslld xmm2, 16
    pslld xmm3, 16
    psrad xmm2, 16
    psrad xmm3, 16
    packssdw xmm2, xmm3
    movdqa xmm3, xmm6                   ; src[2n + 2]
    movdqa xmm4, xmm7
    psrldq xmm3, 4
    psrldq xmm4, 4
    movd eax, xmm7
    movd xmm5, eax
    pslldq xmm5, 12
    por xmm3, xmm5
    mov eax, [esi + 32]
    movd xmm5, eax
    pslldq xmm5, 12
    por xmm4, xmm5
    pand xmm3, [cdFFFF]
    pand xmm4, [cdFFFF]
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

    movdqa xmm6, xmm5                   ; out hi
    paddw xmm6, LHI_ADD
    psraw xmm6, LHI_SFT
    movdqa [edi], xmm6
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
    paddw xmm6, LLO_ADD
    psraw xmm6, LLO_SFT
    movdqa [edx], xmm6

    ; move right
    lea esi, [esi + 16 * 2]
    lea edi, [edi + 8 * 2]
    lea edx, [edx + 8 * 2]

    ; loop
    shl ecx, 16
    mov cx, 2
loop2e:
    movdqa xmm1, [esi]                  ; src[2n]
    movdqa xmm2, [esi + 16]
    movdqa xmm6, xmm1
    movdqa xmm7, xmm2
    pand xmm1, [cdFFFF]
    pand xmm2, [cdFFFF]
    pslld xmm1, 16
    pslld xmm2, 16
    psrad xmm1, 16
    psrad xmm2, 16
    packssdw xmm1, xmm2
    movdqa xmm2, xmm6                   ; src[2n + 1]
    movdqa xmm3, xmm7
    psrldq xmm2, 2
    psrldq xmm3, 2
    pand xmm2, [cdFFFF]
    pand xmm3, [cdFFFF]
    pslld xmm2, 16
    pslld xmm3, 16
    psrad xmm2, 16
    psrad xmm3, 16
    packssdw xmm2, xmm3
    movdqa xmm3, xmm6                   ; src[2n + 2]
    movdqa xmm4, xmm7
    psrldq xmm3, 4
    psrldq xmm4, 4
    movd eax, xmm7
    movd xmm5, eax
    pslldq xmm5, 12
    por xmm3, xmm5
    mov eax, [esi + 32]
    movd xmm5, eax
    pslldq xmm5, 12
    por xmm4, xmm5
    pand xmm3, [cdFFFF]
    pand xmm4, [cdFFFF]
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

    movdqa xmm6, xmm5                   ; out hi
    paddw xmm6, LHI_ADD
    psraw xmm6, LHI_SFT
    movdqa [edi], xmm6
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
    paddw xmm6, LLO_ADD
    psraw xmm6, LLO_SFT
    movdqa [edx], xmm6

    ; move right
    lea esi, [esi + 16 * 2]
    lea edi, [edi + 8 * 2]
    lea edx, [edx + 8 * 2]

    dec cx
    jnz loop2e
    shr ecx, 16

    ; post
    movdqa xmm1, [esi]                  ; src[2n]
    movdqa xmm2, [esi + 16]
    movdqa xmm6, xmm1
    movdqa xmm7, xmm2
    pand xmm1, [cdFFFF]
    pand xmm2, [cdFFFF]
    pslld xmm1, 16
    pslld xmm2, 16
    psrad xmm1, 16
    psrad xmm2, 16
    packssdw xmm1, xmm2
    movdqa xmm2, xmm6                   ; src[2n + 1]
    movdqa xmm3, xmm7
    psrldq xmm2, 2
    psrldq xmm3, 2
    pand xmm2, [cdFFFF]
    pand xmm3, [cdFFFF]
    pslld xmm2, 16
    pslld xmm3, 16
    psrad xmm2, 16
    psrad xmm3, 16
    packssdw xmm2, xmm3
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
    pand xmm3, [cdFFFF]
    pand xmm4, [cdFFFF]
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

    movdqa xmm6, xmm5                   ; out hi
    paddw xmm6, LHI_ADD
    psraw xmm6, LHI_SFT
    movdqa [edi], xmm6

    ; l[n] = src[2n] + ((h[n - 1] + h[n]) >> 1)
    movdqa xmm7, xmm5
    pslldq xmm7, 2
    movd xmm6, ebx
    por xmm7, xmm6
    paddw xmm5, xmm7
    psraw xmm5, 1
    paddw xmm5, xmm1

    movdqa xmm6, xmm5                   ; out lo
    paddw xmm6, LLO_ADD
    psraw xmm6, LLO_SFT
    movdqa [edx], xmm6

    ; move right
    lea esi, [esi + 16 * 2]
    lea edi, [edi + 8 * 2]
    lea edx, [edx + 8 * 2]

    ; move left
    lea esi, [esi - 64 * 2]
    lea edi, [edi - 32 * 2]
    lea edx, [edx - 32 * 2]

    ; move down
    lea esi, [esi + 64 * 2]
    lea edi, [edi + 32 * 2]
    lea edx, [edx + 32 * 2]

    dec ecx
    jnz loop1e

    ret

;******************************************************************************
; source 16 bit signed, 64 pixel width
rfx_dwt_2d_encode_block_horiz_16_64_no_lo:
    mov ecx, 32
loop1e1:
    ; pre
    movdqa xmm1, [esi]                  ; src[2n]
    movdqa xmm2, [esi + 16]
    movdqa xmm6, xmm1
    movdqa xmm7, xmm2
    pand xmm1, [cdFFFF]
    pand xmm2, [cdFFFF]
    pslld xmm1, 16
    pslld xmm2, 16
    psrad xmm1, 16
    psrad xmm2, 16
    packssdw xmm1, xmm2
    movdqa xmm2, xmm6                   ; src[2n + 1]
    movdqa xmm3, xmm7
    psrldq xmm2, 2
    psrldq xmm3, 2
    pand xmm2, [cdFFFF]
    pand xmm3, [cdFFFF]
    pslld xmm2, 16
    pslld xmm3, 16
    psrad xmm2, 16
    psrad xmm3, 16
    packssdw xmm2, xmm3
    movdqa xmm3, xmm6                   ; src[2n + 2]
    movdqa xmm4, xmm7
    psrldq xmm3, 4
    psrldq xmm4, 4
    movd eax, xmm7
    movd xmm5, eax
    pslldq xmm5, 12
    por xmm3, xmm5
    mov eax, [esi + 32]
    movd xmm5, eax
    pslldq xmm5, 12
    por xmm4, xmm5
    pand xmm3, [cdFFFF]
    pand xmm4, [cdFFFF]
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

    movdqa xmm6, xmm5                   ; out hi
    paddw xmm6, LHI_ADD
    psraw xmm6, LHI_SFT
    movdqa [edi], xmm6
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

    movdqa [edx], xmm5                  ; out lo

    ; move right
    lea esi, [esi + 16 * 2]
    lea edi, [edi + 8 * 2]
    lea edx, [edx + 8 * 2]

    ; loop
    shl ecx, 16
    mov cx, 2
loop2e1:
    movdqa xmm1, [esi]                  ; src[2n]
    movdqa xmm2, [esi + 16]
    movdqa xmm6, xmm1
    movdqa xmm7, xmm2
    pand xmm1, [cdFFFF]
    pand xmm2, [cdFFFF]
    pslld xmm1, 16
    pslld xmm2, 16
    psrad xmm1, 16
    psrad xmm2, 16
    packssdw xmm1, xmm2
    movdqa xmm2, xmm6                   ; src[2n + 1]
    movdqa xmm3, xmm7
    psrldq xmm2, 2
    psrldq xmm3, 2
    pand xmm2, [cdFFFF]
    pand xmm3, [cdFFFF]
    pslld xmm2, 16
    pslld xmm3, 16
    psrad xmm2, 16
    psrad xmm3, 16
    packssdw xmm2, xmm3
    movdqa xmm3, xmm6                   ; src[2n + 2]
    movdqa xmm4, xmm7
    psrldq xmm3, 4
    psrldq xmm4, 4
    movd eax, xmm7
    movd xmm5, eax
    pslldq xmm5, 12
    por xmm3, xmm5
    mov eax, [esi + 32]
    movd xmm5, eax
    pslldq xmm5, 12
    por xmm4, xmm5
    pand xmm3, [cdFFFF]
    pand xmm4, [cdFFFF]
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

    movdqa xmm6, xmm5                   ; out hi
    paddw xmm6, LHI_ADD
    psraw xmm6, LHI_SFT
    movdqa [edi], xmm6
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

    movdqa [edx], xmm5                  ; out lo

    ; move right
    lea esi, [esi + 16 * 2]
    lea edi, [edi + 8 * 2]
    lea edx, [edx + 8 * 2]

    dec cx
    jnz loop2e1
    shr ecx, 16

    ; post
    movdqa xmm1, [esi]                  ; src[2n]
    movdqa xmm2, [esi + 16]
    movdqa xmm6, xmm1
    movdqa xmm7, xmm2
    pand xmm1, [cdFFFF]
    pand xmm2, [cdFFFF]
    pslld xmm1, 16
    pslld xmm2, 16
    psrad xmm1, 16
    psrad xmm2, 16
    packssdw xmm1, xmm2
    movdqa xmm2, xmm6                   ; src[2n + 1]
    movdqa xmm3, xmm7
    psrldq xmm2, 2
    psrldq xmm3, 2
    pand xmm2, [cdFFFF]
    pand xmm3, [cdFFFF]
    pslld xmm2, 16
    pslld xmm3, 16
    psrad xmm2, 16
    psrad xmm3, 16
    packssdw xmm2, xmm3
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
    pand xmm3, [cdFFFF]
    pand xmm4, [cdFFFF]
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

    movdqa xmm6, xmm5                   ; out hi
    paddw xmm6, LHI_ADD
    psraw xmm6, LHI_SFT
    movdqa [edi], xmm6

    ; l[n] = src[2n] + ((h[n - 1] + h[n]) >> 1)
    movdqa xmm7, xmm5
    pslldq xmm7, 2
    movd xmm6, ebx
    por xmm7, xmm6
    paddw xmm5, xmm7
    psraw xmm5, 1
    paddw xmm5, xmm1

    movdqa [edx], xmm5                  ; out lo

    ; move right
    lea esi, [esi + 16 * 2]
    lea edi, [edi + 8 * 2]
    lea edx, [edx + 8 * 2]

    ; move left
    lea esi, [esi - 64 * 2]
    lea edi, [edi - 32 * 2]
    lea edx, [edx - 32 * 2]

    ; move down
    lea esi, [esi + 64 * 2]
    lea edi, [edi + 32 * 2]
    lea edx, [edx + 32 * 2]

    dec ecx
    jnz loop1e1

    ret

;******************************************************************************
; source 8 bit unsigned, 64 pixel width
rfx_dwt_2d_encode_block_verti_8_64:
    mov ecx, 8
loop1f:
    ; pre
    movq xmm1, [esi]                    ; src[2n]
    movq xmm2, [esi + 64 * 1]           ; src[2n + 1]
    movq xmm3, [esi + 64 * 1 * 2]       ; src[2n + 2]
    punpcklbw xmm1, xmm0
    punpcklbw xmm2, xmm0
    punpcklbw xmm3, xmm0
    psubw xmm1, [cw128]
    psubw xmm2, [cw128]
    psubw xmm3, [cw128]
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
    movdqa [edi], xmm5                  ; out hi
    movdqa xmm6, xmm5                   ; save hi
    ; l[n] = src[2n] + ((h[n - 1] + h[n]) >> 1)
    paddw xmm5, xmm1
    movdqa [edx], xmm5                  ; out lo
    movdqa xmm7, xmm6                   ; save hi
    ; move down
    lea esi, [esi + 64 * 1 * 2]         ; 2 rows
    lea edi, [edi + 64 * 2]             ; 1 row
    lea edx, [edx + 64 * 2]             ; 1 row

    ; loop
    shl ecx, 16
    mov cx, 30
loop2f:
    movdqa xmm1, xmm3                   ; src[2n]
    movq xmm2, [esi + 64 * 1]           ; src[2n + 1]
    movq xmm3, [esi + 64 * 1 * 2]       ; src[2n + 2]
    punpcklbw xmm2, xmm0
    punpcklbw xmm3, xmm0
    psubw xmm2, [cw128]
    psubw xmm3, [cw128]
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
    movdqa [edi], xmm5                  ; out hi
    movdqa xmm6, xmm5                   ; save hi
    ; l[n] = src[2n] + ((h[n - 1] + h[n]) >> 1)
    paddw xmm5, xmm7
    psraw xmm5, 1
    paddw xmm5, xmm1
    movdqa [edx], xmm5                  ; out lo
    movdqa xmm7, xmm6                   ; save hi
    ; move down
    lea esi, [esi + 64 * 1 * 2]         ; 2 rows
    lea edi, [edi + 64 * 2]             ; 1 row
    lea edx, [edx + 64 * 2]             ; 1 row

    dec cx
    jnz loop2f
    shr ecx, 16

    ; post
    movdqa xmm1, xmm3                   ; src[2n]
    movq xmm2, [esi + 64 * 1]           ; src[2n + 1]
    punpcklbw xmm2, xmm0
    psubw xmm2, [cw128]
    psllw xmm2, 5
    movdqa xmm4, xmm1
    movdqa xmm5, xmm2
    movdqa xmm6, xmm3
    ; h[n] = (src[2n + 1] - ((src[2n] + src[2n + 2]) >> 1)) >> 1
    paddw xmm4, xmm6
    psraw xmm4, 1
    psubw xmm5, xmm4
    psraw xmm5, 1
    movdqa [edi], xmm5                  ; out hi
    ; l[n] = src[2n] + ((h[n - 1] + h[n]) >> 1)
    paddw xmm5, xmm7
    psraw xmm5, 1
    paddw xmm5, xmm1
    movdqa [edx], xmm5                  ; out lo
    ; move down
    lea esi, [esi + 64 * 1 * 2]         ; 2 rows
    lea edi, [edi + 64 * 2]             ; 1 row
    lea edx, [edx + 64 * 2]             ; 1 row

    ; move up
    lea esi, [esi - 64 * 1 * 64]
    lea edi, [edi - 32 * 64 * 2]
    lea edx, [edx - 32 * 64 * 2]

    ; move right
    lea esi, [esi + 8]
    lea edi, [edi + 16]
    lea edx, [edx + 16]

    dec ecx
    jnz loop1f

    ret

set_quants_hi:
    sub eax, 6 - 5
    movd xmm1, eax
    movdqa LHI_SFT, xmm1
    imul eax, 16
    lea edx, [cwa0]
    add edx, eax
    movdqa xmm1, [edx]
    movdqa LHI_ADD, xmm1
    ret

set_quants_lo:
    sub eax, 6 - 5
    movd xmm1, eax
    movdqa LLO_SFT, xmm1
    imul eax, 16
    lea edx, [cwa0]
    add edx, eax
    movdqa xmm1, [edx]
    movdqa LLO_ADD, xmm1
    ret

%define LQTABLE           [esp + 144] ; qtable
%define LIN_BUFFER        [esp + 148] ; in_buffer
%define LOUT_BUFFER       [esp + 152] ; out_buffer
%define LWORK_BUFFER      [esp + 156] ; work_buffer

;int
;rfxcodec_encode_dwt_shift_x86_sse2(const char *qtable,
;                                   unsigned char *in_buffer,
;                                   short *out_buffer,
;                                   short *work_buffer);

;******************************************************************************
%ifidn __OUTPUT_FORMAT__,elf
PROC rfxcodec_encode_dwt_shift_x86_sse2
%else
PROC _rfxcodec_encode_dwt_shift_x86_sse2
%endif
    ; align stack
    mov eax, esp
    sub eax, 0x10
    and eax, 0x0F
    sub esp, eax
    push eax
    sub esp, 3 * 4
    sub esp, 4 * 4
    ; copy params to after align
    movdqu xmm0, [esp + eax + 4 * 4 + 3 * 4 + 4 + 4]
    movdqu [esp], xmm0
    ; save registers
    push ebx
    push esi
    push edi
    push ebp
    sub esp, 16 * 8
    pxor xmm0, xmm0

    ; verical DWT to work buffer, level 1
    mov esi, LIN_BUFFER                 ; src
    mov edi, LWORK_BUFFER               ; dst hi
    lea edi, [edi + 64 * 32 * 2]        ; dst hi
    mov edx, LWORK_BUFFER               ; dst lo
    call rfx_dwt_2d_encode_block_verti_8_64

    ; horizontal DWT to out buffer, level 1, part 1
    xor eax, eax
    mov edx, LQTABLE
    mov al, [edx + 4]
    and al, 0xF
    call set_quants_hi
    mov esi, LWORK_BUFFER               ; src
    mov edi, LOUT_BUFFER                ; dst hi - HL1
    mov edx, LOUT_BUFFER                ; dst lo - LL1
    lea edx, [edx + 32 * 32 * 6]        ; dst lo - LL1
    call rfx_dwt_2d_encode_block_horiz_16_64_no_lo

    ; horizontal DWT to out buffer, level 1, part 2
    xor eax, eax
    mov edx, LQTABLE
    mov al, [edx + 4]
    shr al, 4
    call set_quants_hi
    xor eax, eax
    mov edx, LQTABLE
    mov al, [edx + 3]
    shr al, 4
    call set_quants_lo
    mov esi, LWORK_BUFFER               ; src
    lea esi, [esi + 64 * 32 * 2]        ; src
    mov edi, LOUT_BUFFER                ; dst hi - HH1
    lea edi, [edi + 32 * 32 * 4]        ; dst hi - HH1
    mov edx, LOUT_BUFFER                ; dst lo - LH1
    lea edx, [edx + 32 * 32 * 2]        ; dst lo - LH1
    call rfx_dwt_2d_encode_block_horiz_16_64

    ; verical DWT to work buffer, level 2
    mov esi, LOUT_BUFFER                ; src
    lea esi, [esi + 32 * 32 * 6]        ; src
    mov edi, LWORK_BUFFER               ; dst hi
    lea edi, [edi + 32 * 16 * 2]        ; dst hi
    mov edx, LWORK_BUFFER               ; dst lo
    call rfx_dwt_2d_encode_block_verti_16_32

    ; horizontal DWT to out buffer, level 2, part 1
    xor eax, eax
    mov edx, LQTABLE
    mov al, [edx + 2]
    shr al, 4
    call set_quants_hi
    mov esi, LWORK_BUFFER               ; src
    ; 32 * 32 * 6 + 16 * 16 * 0 = 6144
    mov edi, LOUT_BUFFER                ; dst hi - HL2
    lea edi, [edi + 6144]               ; dst hi - HL2
    ; 32 * 32 * 6 + 16 * 16 * 6 = 7680
    mov edx, LOUT_BUFFER                ; dst lo - LL2
    lea edx, [edx + 7680]               ; dst lo - LL2
    call rfx_dwt_2d_encode_block_horiz_16_32_no_lo

    ; horizontal DWT to out buffer, level 2, part 2
    xor eax, eax
    mov edx, LQTABLE
    mov al, [edx + 3]
    and al, 0xF
    call set_quants_hi
    xor eax, eax
    mov edx, LQTABLE
    mov al, [edx + 2]
    and al, 0xF
    call set_quants_lo
    mov esi, LWORK_BUFFER               ; src
    lea esi, [esi + 32 * 16 * 2]        ; src
    ; 32 * 32 * 6 + 16 * 16 * 4 = 7168
    mov edi, LOUT_BUFFER                ; dst hi - HH2
    lea edi, [edi + 7168]               ; dst hi - HH2
    ; 32 * 32 * 6 + 16 * 16 * 2 = 6656
    mov edx, LOUT_BUFFER                ; dst lo - LH2
    lea edx, [edx + 6656]               ; dst lo - LH2
    call rfx_dwt_2d_encode_block_horiz_16_32

    ; verical DWT to work buffer, level 3
    ; 32 * 32 * 6 + 16 * 16 * 6 = 7680
    mov esi, LOUT_BUFFER                ; src
    lea esi, [esi + 7680]               ; src
    mov edi, LWORK_BUFFER               ; dst hi
    lea edi, [edi + 16 * 8 * 2]         ; dst hi
    mov edx, LWORK_BUFFER               ; dst lo
    call rfx_dwt_2d_encode_block_verti_16_16

    ; horizontal DWT to out buffer, level 3, part 1
    xor eax, eax
    mov edx, LQTABLE
    mov al, [edx + 1]
    and al, 0xF
    call set_quants_hi
    xor eax, eax
    mov edx, LQTABLE
    mov al, [edx + 0]
    and al, 0xF
    call set_quants_lo
    mov esi, LWORK_BUFFER               ; src
    ; 32 * 32 * 6 + 16 * 16 * 6 + 8 * 8 * 0 = 7680
    mov edi, LOUT_BUFFER                ; dst hi - HL3
    lea edi, [edi + 7680]               ; dst hi - HL3
    ; 32 * 32 * 6 + 16 * 16 * 6 + 8 * 8 * 6 = 8064
    mov edx, LOUT_BUFFER                ; dst lo - LL3
    lea edx, [edx + 8064]               ; dst lo - LL3
    call rfx_dwt_2d_encode_block_horiz_16_16

    ; horizontal DWT to out buffer, level 3, part 2
    xor eax, eax
    mov edx, LQTABLE
    mov al, [edx + 1]
    shr al, 4
    call set_quants_hi
    xor eax, eax
    mov edx, LQTABLE
    mov al, [edx + 0]
    shr al, 4
    call set_quants_lo
    mov esi, LWORK_BUFFER               ; src
    lea esi, [esi + 16 * 8 * 2]         ; src
    ; 32 * 32 * 6 + 16 * 16 * 6 + 8 * 8 * 4 = 7936
    mov edi, LOUT_BUFFER                ; dst hi - HH3
    lea edi, [edi + 7936]               ; dst hi - HH3
    ; 32 * 32 * 6 + 16 * 16 * 6 + 8 * 8 * 2 = 7808
    mov edx, LOUT_BUFFER                ; dst lo - LH3
    lea edx, [edx + 7808]               ; dst lo - LH3
    call rfx_dwt_2d_encode_block_horiz_16_16

    ; quants
    add esp, 16 * 8
    ; restore registers
    pop ebp
    pop edi
    pop esi
    pop ebx
    ; params
    add esp, 3 * 4
    add esp, 4 * 4
    ; align
    pop eax
    add esp, eax
    ; return value
    mov eax, 0
    ret
    align 16

