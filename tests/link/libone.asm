; a member somebody wants: `usesone` calls it, so any linker worth the
; name pulls this in
bits 64
global answer
section .text
answer:
    mov rax, 42
    ret
