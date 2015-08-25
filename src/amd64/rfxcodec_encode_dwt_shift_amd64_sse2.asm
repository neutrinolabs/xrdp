
section .data
    align 16
    const128 times 8 dw 128

section .text

%macro PROC 1
    align 16
    global %1
    %1:
%endmacro

rfx_dwt_2d_encode_block_8_32:
    ; save registers
    push rsi
    push rcx

    pxor xmm0, xmm0
    mov rax, rcx
    mov rcx, 8
loop1:
    movq xmm1, [rsi]
    movq xmm2, [rsi + 64]
    movq xmm3, [rsi + 128]
    punpcklbw xmm1, xmm0
    punpcklbw xmm2, xmm0
    punpcklbw xmm3, xmm0
    psubw xmm1, [rel const128]
    psubw xmm2, [rel const128]
    psubw xmm3, [rel const128]
    psllw xmm1, 5
    psllw xmm2, 5
    psllw xmm3, 5
    movdqu xmm4, xmm1
    ; h[n] = (src[2n + 1] - ((src[2n] + src[2n + 2]) >> 1)) >> 1
    paddw xmm1, xmm3
    psraw xmm1, 1
    psubw xmm2, xmm1
    psraw xmm2, 1
    movdqu [rax + 64 * 32 * 2], xmm2
    ; l[n] = src[2n] + ((h[n - 1] + h[n]) >> 1)
    paddw xmm4, xmm2
    movdqu [rax], xmm4
    lea rsi, [rsi + 8]
    lea rax, [rax + 16]
    dec cl
    jnz loop1

    ; restore registers
    pop rcx
    pop rsi
    ret

rfx_dwt_2d_encode_block_16_16:
    ret

rfx_dwt_2d_encode_block_16_8:
    ret

;The first six integer or pointer arguments are passed in registers
;RDI, RSI, RDX, RCX, R8, and R9

;int
;rfxcodec_encode_dwt_shift_amd64_sse2(const char *qtable,
;                                     unsigned char *in_buffer,
;                                     short *out_buffer,
;                                     short *work_buffer);

%ifidn __OUTPUT_FORMAT__,elf64
PROC rfxcodec_encode_dwt_shift_amd64_sse2
%else
PROC _rfxcodec_encode_dwt_shift_amd64_sse2
%endif
    ; save registers
    push rbx
    ; source 8 bit unsigned, 32 pixel width
    call rfx_dwt_2d_encode_block_8_32
    ; source 16 bit signed, 16 pixel width
    call rfx_dwt_2d_encode_block_16_16
    ; source 16 bit signed, 8 pixel width
    call rfx_dwt_2d_encode_block_16_8
    mov rax, 0
    ; restore registers
    pop rbx
    ret
    align 16

