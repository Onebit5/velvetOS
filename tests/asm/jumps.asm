bits 64
start:
    xor rax, rax
    cmp rax, 0
    jz  forward
    mov rax, 1
forward:
    add rax, 2
    call helper
    jmp  done
helper:
    push rbp
    mov rbp, rsp
    pop rbp
    ret
done:
    jmp start
    jne start
    ja  forward
    jmp rax
    call rbx
