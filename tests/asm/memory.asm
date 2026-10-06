bits 64
    mov rax, [rdi]
    mov rbx, [rsi]
    mov [rdi], rax
    mov [rsi], rbx
    mov rcx, [rsp+8]
    mov [rsp+8], rcx
    mov rdx, [rbp]
    mov [rbp], rdx
    mov r8, [r9]
    mov [r10], r11
    mov rax, [rsp]
    mov [rsp], rax
    mov rax, [gs:0]
    mov rax, [gs:8]
    mov [gs:8], rax
    mov rax, [rdi+128]
    mov rax, [rdi-8]
