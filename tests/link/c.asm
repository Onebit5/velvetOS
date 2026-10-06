; the writable half. a linker gets .text and .rodata right long before it
; gets these, because these are the two that behave differently: .data
; has bytes and needs a segment that may be written to, and .bss has no
; bytes at all -- it is an address and a length and a promise that the
; memory will be zero. a linker that writes it out anyway turns the
; kernel's zeroed globals into megabytes of file.
bits 64

global counter
global buffer

section .data
counter:
    dq 7
    dd 0x11223344
    ; a relocation that is not in .text, which is the case a linker
    ; written around one loop over .rela.text quietly gets wrong
    dq buffer

section .bss
buffer:
    resb 64
