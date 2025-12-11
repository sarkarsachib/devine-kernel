/* x86_64 Interrupt Service Routines (ISRs) */

#include "../../include/types.h"

// Save CPU context
.macro SAVE_CONTEXT
    // Push general purpose registers
    push %rdi
    push %rsi
    push %rdx
    push %rcx
    push %rax
    
    // Push segment registers
    push %ds
    push %es
    push %fs
    push %gs
    
    // Push interrupt number
    push %rbp
    mov $0, %rbp
    push 8(%rbp)  // original rbp
    
    // Set up kernel data segment
    mov $0x10, %ax  // kernel data segment
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
.endm

// Restore CPU context
.macro RESTORE_CONTEXT
    pop %rbp       // restore original rbp
    pop %rsi       // restore interrupt number
    add $8, %rsp   // skip saved rbp
    
    pop %gs
    pop %fs
    pop %es
    pop %ds
    
    pop %rax
    pop %rcx
    pop %rdx
    pop %rsi
    pop %rdi
.endm

// Exception handlers (0-31)
.type isr0, @function
isr0:
    SAVE_CONTEXT
    mov $0, %rdi
    call isr_handler
    RESTORE_CONTEXT
    iretq

.type isr1, @function
isr1:
    SAVE_CONTEXT
    mov $1, %rdi
    call isr_handler
    RESTORE_CONTEXT
    iretq

.type isr2, @function
isr2:
    SAVE_CONTEXT
    mov $2, %rdi
    call isr_handler
    RESTORE_CONTEXT
    iretq

.type isr3, @function
isr3:
    SAVE_CONTEXT
    mov $3, %rdi
    call isr_handler
    RESTORE_CONTEXT
    iretq

.type isr4, @function
isr4:
    SAVE_CONTEXT
    mov $4, %rdi
    call isr_handler
    RESTORE_CONTEXT
    iretq

.type isr5, @function
isr5:
    SAVE_CONTEXT
    mov $5, %rdi
    call isr_handler
    RESTORE_CONTEXT
    iretq

.type isr6, @function
isr6:
    SAVE_CONTEXT
    mov $6, %rdi
    call isr_handler
    RESTORE_CONTEXT
    iretq

.type isr7, @function
isr7:
    SAVE_CONTEXT
    mov $7, %rdi
    call isr_handler
    RESTORE_CONTEXT
    iretq

.type isr8, @function
isr8:
    SAVE_CONTEXT
    mov $8, %rdi
    call isr_handler
    RESTORE_CONTEXT
    iretq

.type isr9, @function
isr9:
    SAVE_CONTEXT
    mov $9, %rdi
    call isr_handler
    RESTORE_CONTEXT
    iretq

.type isr10, @function
isr10:
    SAVE_CONTEXT
    mov $10, %rdi
    call isr_handler
    RESTORE_CONTEXT
    iretq

.type isr11, @function
isr11:
    SAVE_CONTEXT
    mov $11, %rdi
    call isr_handler
    RESTORE_CONTEXT
    iretq

.type isr12, @function
isr12:
    SAVE_CONTEXT
    mov $12, %rdi
    call isr_handler
    RESTORE_CONTEXT
    iretq

.type isr13, @function
isr13:
    SAVE_CONTEXT
    mov $13, %rdi
    call isr_handler
    RESTORE_CONTEXT
    iretq

.type isr14, @function
isr14:
    SAVE_CONTEXT
    mov $14, %rdi
    call isr_handler
    RESTORE_CONTEXT
    iretq

.type isr15, @function
isr15:
    SAVE_CONTEXT
    mov $15, %rdi
    call isr_handler
    RESTORE_CONTEXT
    iretq

.type isr16, @function
isr16:
    SAVE_CONTEXT
    mov $16, %rdi
    call isr_handler
    RESTORE_CONTEXT
    iretq

.type isr17, @function
isr17:
    SAVE_CONTEXT
    mov $17, %rdi
    call isr_handler
    RESTORE_CONTEXT
    iretq

.type isr18, @function
isr18:
    SAVE_CONTEXT
    mov $18, %rdi
    call isr_handler
    RESTORE_CONTEXT
    iretq

.type isr19, @function
isr19:
    SAVE_CONTEXT
    mov $19, %rdi
    call isr_handler
    RESTORE_CONTEXT
    iretq

.type isr20, @function
isr20:
    SAVE_CONTEXT
    mov $20, %rdi
    call isr_handler
    RESTORE_CONTEXT
    iretq

.type isr21, @function
isr21:
    SAVE_CONTEXT
    mov $21, %rdi
    call isr_handler
    RESTORE_CONTEXT
    iretq

.type isr22, @function
isr22:
    SAVE_CONTEXT
    mov $22, %rdi
    call isr_handler
    RESTORE_CONTEXT
    iretq

.type isr23, @function
isr23:
    SAVE_CONTEXT
    mov $23, %rdi
    call isr_handler
    RESTORE_CONTEXT
    iretq

.type isr24, @function
isr24:
    SAVE_CONTEXT
    mov $24, %rdi
    call isr_handler
    RESTORE_CONTEXT
    iretq

.type isr25, @function
isr25:
    SAVE_CONTEXT
    mov $25, %rdi
    call isr_handler
    RESTORE_CONTEXT
    iretq

.type isr26, @function
isr26:
    SAVE_CONTEXT
    mov $26, %rdi
    call isr_handler
    RESTORE_CONTEXT
    iretq

.type isr27, @function
isr27:
    SAVE_CONTEXT
    mov $27, %rdi
    call isr_handler
    RESTORE_CONTEXT
    iretq

.type isr28, @function
isr28:
    SAVE_CONTEXT
    mov $28, %rdi
    call isr_handler
    RESTORE_CONTEXT
    iretq

.type isr29, @function
isr29:
    SAVE_CONTEXT
    mov $29, %rdi
    call isr_handler
    RESTORE_CONTEXT
    iretq

.type isr30, @function
isr30:
    SAVE_CONTEXT
    mov $30, %rdi
    call isr_handler
    RESTORE_CONTEXT
    iretq

.type isr31, @function
isr31:
    SAVE_CONTEXT
    mov $31, %rdi
    call isr_handler
    RESTORE_CONTEXT
    iretq

// IRQ handlers (32-47)
.type irq0, @function
irq0:
    SAVE_CONTEXT
    mov $32, %rdi
    call irq_handler
    RESTORE_CONTEXT
    iretq

.type irq1, @function
irq1:
    SAVE_CONTEXT
    mov $33, %rdi
    call irq_handler
    RESTORE_CONTEXT
    iretq

.type irq2, @function
irq2:
    SAVE_CONTEXT
    mov $34, %rdi
    call irq_handler
    RESTORE_CONTEXT
    iretq

.type irq3, @function
irq3:
    SAVE_CONTEXT
    mov $35, %rdi
    call irq_handler
    RESTORE_CONTEXT
    iretq

.type irq4, @function
irq4:
    SAVE_CONTEXT
    mov $36, %rdi
    call irq_handler
    RESTORE_CONTEXT
    iretq

.type irq5, @function
irq5:
    SAVE_CONTEXT
    mov $37, %rdi
    call irq_handler
    RESTORE_CONTEXT
    iretq

.type irq6, @function
irq6:
    SAVE_CONTEXT
    mov $38, %rdi
    call irq_handler
    RESTORE_CONTEXT
    iretq

.type irq7, @function
irq7:
    SAVE_CONTEXT
    mov $39, %rdi
    call irq_handler
    RESTORE_CONTEXT
    iretq

.type irq8, @function
irq8:
    SAVE_CONTEXT
    mov $40, %rdi
    call irq_handler
    RESTORE_CONTEXT
    iretq

.type irq9, @function
irq9:
    SAVE_CONTEXT
    mov $41, %rdi
    call irq_handler
    RESTORE_CONTEXT
    iretq

.type irq10, @function
irq10:
    SAVE_CONTEXT
    mov $42, %rdi
    call irq_handler
    RESTORE_CONTEXT
    iretq

.type irq11, @function
irq11:
    SAVE_CONTEXT
    mov $43, %rdi
    call irq_handler
    RESTORE_CONTEXT
    iretq

.type irq12, @function
irq12:
    SAVE_CONTEXT
    mov $44, %rdi
    call irq_handler
    RESTORE_CONTEXT
    iretq

.type irq13, @function
irq13:
    SAVE_CONTEXT
    mov $45, %rdi
    call irq_handler
    RESTORE_CONTEXT
    iretq

.type irq14, @function
irq14:
    SAVE_CONTEXT
    mov $46, %rdi
    call irq_handler
    RESTORE_CONTEXT
    iretq

.type irq15, @function
irq15:
    SAVE_CONTEXT
    mov $47, %rdi
    call irq_handler
    RESTORE_CONTEXT
    iretq