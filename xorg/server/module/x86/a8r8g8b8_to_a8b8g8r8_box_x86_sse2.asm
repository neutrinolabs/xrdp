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
;ARGB to ABGR
;x86 SSE2 32 bit
;

SECTION .data
align 16
c1 times 4 dd 0xFF00FF00
c2 times 4 dd 0x00FF0000
c3 times 4 dd 0x000000FF

SECTION .text

%macro PROC 1
    align 16
    global %1
    %1:
%endmacro

;int
;a8r8g8b8_to_a8b8g8r8_box_x86_sse2(char *s8, int src_stride,
;                                  char *d8, int dst_stride,
;                                  int width, int height);
PROC a8r8g8b8_to_a8b8g8r8_box_x86_sse2
    push ebx
    push esi
    push edi
    push ebp

    movdqa xmm4, [c1]
    movdqa xmm5, [c2]
    movdqa xmm6, [c3]

    mov esi, [esp + 20]  ; src
    mov edi, [esp + 28]  ; dst

loop_y:
    mov ecx, [esp + 36]  ; width

loop_xpre:
    mov eax, esi         ; look for aligned
    and eax, 0x0F        ; we can jump to next
    mov ebx, eax
    mov eax, edi
    and eax, 0x0F
    or eax, ebx
    cmp eax, 0
    je done_loop_xpre
    cmp ecx, 1
    jl done_loop_x       ; all done with this row
    mov eax, [esi]
    lea esi, [esi + 4]
    mov edx, eax         ; a and g
    and edx, 0xFF00FF00
    mov ebx, eax         ; r
    and ebx, 0x00FF0000
    shr ebx, 16
    or edx, ebx
    mov ebx, eax         ; b
    and ebx, 0x000000FF
    shl ebx, 16
    or edx, ebx
    mov [edi], edx
    lea edi, [edi + 4]
    dec ecx
    jmp loop_xpre;
done_loop_xpre:

    prefetchnta [esi]

; A R G B A R G B A R G B A R G B to
; A B G R A B G R A B G R A B G R

loop_x8:
    cmp ecx, 8
    jl done_loop_x8

    prefetchnta [esi + 32]

    movdqa xmm0, [esi]
    lea esi, [esi + 16]
    movdqa xmm3, xmm0    ; a and g
    pand xmm3, xmm4
    movdqa xmm1, xmm0    ; r
    pand xmm1, xmm5
    psrld xmm1, 16
    por xmm3, xmm1
    movdqa xmm1, xmm0    ; b
    pand xmm1, xmm6
    pslld xmm1, 16
    por xmm3, xmm1
    movdqa [edi], xmm3
    lea edi, [edi + 16]
    sub ecx, 4

    movdqa xmm0, [esi]
    lea esi, [esi + 16]
    movdqa xmm3, xmm0    ; a and g
    pand xmm3, xmm4
    movdqa xmm1, xmm0    ; r
    pand xmm1, xmm5
    psrld xmm1, 16
    por xmm3, xmm1
    movdqa xmm1, xmm0    ; b
    pand xmm1, xmm6
    pslld xmm1, 16
    por xmm3, xmm1
    movdqa [edi], xmm3
    lea edi, [edi + 16]
    sub ecx, 4

    jmp loop_x8;
done_loop_x8:

loop_x:
    cmp ecx, 1
    jl done_loop_x
    mov eax, [esi]
    lea esi, [esi + 4]
    mov edx, eax         ; a and g
    and edx, 0xFF00FF00
    mov ebx, eax         ; r
    and ebx, 0x00FF0000
    shr ebx, 16
    or edx, ebx
    mov ebx, eax         ; b
    and ebx, 0x000000FF
    shl ebx, 16
    or edx, ebx
    mov [edi], edx
    lea edi, [edi + 4]
    dec ecx
    jmp loop_x;
done_loop_x:

    mov esi, [esp + 20]
    add esi, [esp + 24]
    mov [esp + 20], esi

    mov edi, [esp + 28]
    add edi, [esp + 32]
    mov [esp + 28], edi

    mov ecx, [esp + 40] ; height
    dec ecx
    mov [esp + 40], ecx
    jnz loop_y

    mov eax, 0          ; return value
    pop ebp
    pop edi
    pop esi
    pop ebx
    ret
    align 16

