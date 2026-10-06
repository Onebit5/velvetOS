bits 64
    db 1
    db 0x41
    dw 0x1234
    dd 0xdeadbeef
    dq 0x1122334455667788
table:
    dq table
    dq entry
entry:
    ret
