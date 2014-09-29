
SECTION .text

%macro PROC 1
    align 16
    global %1
    %1:
%endmacro

;int
;cpuid_x86(int eax_in, int ecx_in, int *eax, int *ebx, int *ecx, int *edx)

PROC cpuid_x86
    ; save registers
    push ebx
    push ecx
    push edx
    ; cpuid
    mov eax, [esp + 16]
    mov ecx, [esp + 20]
    cpuid
    mov [esp + 24], eax
    mov [esp + 28], ebx
    mov [esp + 32], ecx
    mov [esp + 36], edx
    mov eax, 0
    ; restore registers
    pop edx
    pop ecx
    pop ebx
    ret;
    align 16

