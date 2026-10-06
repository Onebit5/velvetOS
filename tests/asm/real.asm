; sixteen bits, which is a different encoding rather than a smaller one.
;
; real mode has its own ModRM table -- four registers, in pairs, and no
; SIB byte anywhere -- and it flips the meaning of the operand-size
; prefix: `ax` is free here and `eax` costs a 0x66, which is exactly the
; other way round from every other file in this directory. everything a
; woken core does before it reaches long mode is written in this, so
; nothing here is hypothetical.

bits 16
org 0x8000

WHERE   equ 0x8f00

start:
    cli
    cld
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7c00
    mov es, ax
    mov ax, es

    ; the accumulator's own form: no ModRM byte, and only for a bare
    ; address. one byte shorter, and nasm always takes it
    mov al, [WHERE]
    mov ax, [WHERE]
    mov eax, [WHERE]
    mov [WHERE], al
    mov [WHERE], ax
    mov [WHERE], eax

    ; and the ordinary form, for every other register
    mov bx, [WHERE]
    mov ebx, [WHERE]
    mov [WHERE], cx
    mov word [WHERE], 5
    mov byte [WHERE], 5
    cmp byte [WHERE], 0

    ; the whole of the sixteen-bit table, in the order it is numbered
    mov ax, [bx+si]
    mov ax, [bx+di]
    mov ax, [bp+si]
    mov ax, [bp+di]
    mov ax, [si]
    mov ax, [di]
    mov ax, [bp]            ; rm 110 with mod 00 is an address, not bp
    mov ax, [bx]
    mov ax, [bx+si+4]
    mov ax, [bp+di+0x1234]
    mov ax, [bx+0x40]
    mov [bx+si], ax
    mov cx, [es:bx]

    ; immediates, and the prefix that says which width is meant
    mov cx, 0x1234
    mov ecx, 0xc0000080
    mov dx, 0x3f8
    add ax, 0x1234
    or  eax, 0x80000001
    or  al, 0x40
    and ax, 0xf
    xor bx, 0x7fff
    cmp cx, 3
    test al, 0x20
    test ax, 0x1234

    ; the control registers, which are neither 16- nor 32-bit but
    ; whatever the mode says they are
    mov eax, cr0
    or eax, 1
    mov cr0, eax
    mov eax, cr4
    mov eax, [WHERE]
    mov cr3, eax

    o32 lgdt [table]
    lgdt [table]
    lidt [table]

    in al, dx
    out dx, al
    in ax, dx
    out dx, ax

    ; a near branch is two bytes of displacement here rather than four
    call helper
    jmp  onward
helper:
    push ax
    pop ax
    ret
onward:
    jz  start
    jne onward
    jmp start
    jmp 0x0000:start
    jmp dword 0x08:wide

align 8
table:
    dw 23
    dd 0x80f8

bits 64
wide:
    mov ax, 0x10
    mov ds, ax
    mov rsp, [0x8f08]
    xor rbp, rbp
    mov rdi, [0x8f18]
    jmp rax
