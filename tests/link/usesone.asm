; the program: it wants `answer` and nothing else
bits 64
global _start
extern answer
section .text
_start:
    call answer
    ret
