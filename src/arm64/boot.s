/* ARM64 Boot Code */
.section .text.boot
.global _start
.extern kmain

_start:
    mrs x0, mpidr_el1
    and x0, x0, #0xFF
    cbz x0, primary_cpu
    
secondary_cpu:
    wfi
    b secondary_cpu

primary_cpu:
    ldr x0, =stack_top
    mov sp, x0
    
    bl setup_exceptions
    bl clear_bss
    
    mov x0, xzr
    b kmain

.section .text
setup_exceptions:
    ldr x0, =exception_vectors
    msr vbar_el1, x0
    isb
    ret

clear_bss:
    ldr x0, =bss_start
    ldr x1, =bss_end
    mov x2, xzr

clear_loop:
    cmp x0, x1
    b.ge clear_done
    str x2, [x0]
    add x0, x0, #8
    b clear_loop

clear_done:
    ret

.section .text
.align 11
exception_vectors:
    /* EL1 Synchronous - Current SP_EL0 */
    b exception_handler
    .align 7

    /* EL1 IRQ - Current SP_EL0 */
    b exception_handler
    .align 7

    /* EL1 FIQ - Current SP_EL0 */
    b exception_handler
    .align 7

    /* EL1 SError - Current SP_EL0 */
    b exception_handler
    .align 7

    /* EL1 Synchronous - Current SP_ELx */
    b exception_handler
    .align 7

    /* EL1 IRQ - Current SP_ELx */
    b exception_handler
    .align 7

    /* EL1 FIQ - Current SP_ELx */
    b exception_handler
    .align 7

    /* EL1 SError - Current SP_ELx */
    b exception_handler

.section .text
exception_handler:
    hlt
    b exception_handler

.section .bss
.align 12
stack_bottom:
    .skip 0x4000
stack_top:

.section .bss
bss_start = .

.section .bss
bss_end = .
