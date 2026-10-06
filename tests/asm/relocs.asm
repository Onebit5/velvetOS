; the parts of an object a byte comparison cannot see.
;
; every line here assembles to something a .text diff agrees with
; whether or not the object is right: a name exported or kept to itself
; is one nibble in the symbol table, and a relocation filed under the
; wrong section still leaves .text identical. both were wrong for a
; version, and neither showed up until a linker read the result.
bits 64

global entry
global table
extern elsewhere

section .text
entry:
    call elsewhere          ; a name this file does not define
    mov rax, table          ; one it does, in the other section
    ret

local_only:
    ret

section .rodata
table:
    dq entry                ; a relocation in .rodata, not in .text
    dq local_only
    dq elsewhere
