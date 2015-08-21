
SECTION .text

%macro PROC 1
    align 16
    global %1
    %1:
%endmacro

;int
;cpuid_x86(int eax_in, int ecx_in, int *eax, int *ebx, int *ecx, int *edx)

%ifidn __OUTPUT_FORMAT__,elf
PROC cpuid_x86
%else
PROC _cpuid_x86
%endif
    ; save registers
    push ebx
    push ecx
    push edx
    push edi
    ; cpuid
    mov eax, [esp + 20]
    mov ecx, [esp + 24]
    cpuid
    mov edi, [esp + 28]
    mov [edi], eax
    mov edi, [esp + 32]
    mov [edi], ebx
    mov edi, [esp + 36]
    mov [edi], ecx
    mov edi, [esp + 40]
    mov [edi], edx
    mov eax, 0
    ; restore registers
    pop edi
    pop edx
    pop ecx
    pop ebx
    ret;
    align 16

