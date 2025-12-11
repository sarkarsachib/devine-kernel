    .global _start
    .extern kuser_entry

_start:
    xor %rbp, %rbp
    mov %rsp, %rdi
    call kuser_entry
1:
    hlt
    jmp 1b
