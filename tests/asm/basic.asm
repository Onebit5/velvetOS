bits 64
    ret
    nop
    cli
    sti
    hlt
    cld
    syscall
    iretq
    rdmsr
    wrmsr
    swapgs
    push rax
    push rbp
    push r12
    push r15
    pop r15
    pop r12
    pop rbp
    pop rax
    mov rax, rbx
    mov rdi, rsi
    mov r8, r9
    mov rsp, rbp
    mov rax, 1
    mov rdi, 0
    mov r10, 255
    mov eax, 7
    xor rax, rax
    xor eax, eax
    xor r8, r8
    add rax, rbx
    add rsp, 8
    sub rsp, 16
    or rax, rcx
    and rdx, rbx
    cmp rax, rdx
    cmp rax, 0
    test rax, rax
    test r8, r8
    in al, dx
    out dx, al
    in eax, dx
    out dx, eax
