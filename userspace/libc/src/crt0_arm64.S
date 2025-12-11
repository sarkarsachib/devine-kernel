    .global _start
    .extern kuser_entry

_start:
    mov x0, sp
    bl kuser_entry
1:
    wfi
    b 1b
