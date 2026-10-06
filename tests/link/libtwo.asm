; and one nobody wants. it defines a name nothing asks for -- and a
; second `answer`, so a linker that pulls members in whether or not they
; resolve anything does not merely make a bigger program, it fails.
;
; the string is there so the check can ask the plainest question there
; is about whether this member was linked: are its bytes in the file
bits 64
global unwanted_helper
global answer
section .text
unwanted_helper:
    mov rax, 7
    ret
answer:
    mov rax, 0
    ret
section .rodata
    db "unwanted", 0
