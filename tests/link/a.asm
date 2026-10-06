bits 64
global _start
extern helper
_start:
    xor rax, rax
    call helper
    mov rdi, table
    ret
section .rodata
table:
    dq _start
    dq helper
