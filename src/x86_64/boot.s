.set ALIGN,    1<<0
.set MEMINFO,  1<<1
.set FLAGS,    ALIGN | MEMINFO
.set MAGIC,    0x1BADB002
.set CHECKSUM, -(MAGIC + FLAGS)

.section .multiboot
.align 4
.long MAGIC
.long FLAGS
.long CHECKSUM

.section .text
.global _start
.extern kmain

_start:
    cli
    mov $stack_top, %esp
    
    push %ebx
    push %eax
    
    call setup_paging
    
    pop %eax
    pop %ebx
    
    push %ebx
    call kmain
    hlt

.section .bss
.align 0x1000
boot_page_table_l4:
    .skip 0x1000

boot_page_table_l3:
    .skip 0x1000

boot_page_table_l2:
    .skip 0x1000

stack_bottom:
    .skip 0x4000
stack_top:

.section .text
setup_paging:
    pusha
    
    mov $boot_page_table_l4, %ecx
    mov $0x00, %eax
    mov $0x1000, %edx
clear_tables:
    mov $0, (%eax)
    add $0x4, %eax
    cmp %edx, %eax
    jl clear_tables
    
    mov $boot_page_table_l4, %eax
    mov $(boot_page_table_l3 + 0x3), %ecx
    mov %ecx, (%eax)
    
    mov $boot_page_table_l3, %eax
    mov $(boot_page_table_l2 + 0x3), %ecx
    mov %ecx, (%eax)
    
    mov $boot_page_table_l2, %eax
    mov $0x83, %ecx
    mov $0, %edx
    
fill_l2_table:
    mov %ecx, (%eax)
    add $0x4, %eax
    add $0x200000, %ecx
    add $1, %edx
    cmp $512, %edx
    jl fill_l2_table
    
    mov $boot_page_table_l4, %eax
    mov %eax, %cr3
    
    mov %cr4, %eax
    or $0x20, %eax
    mov %eax, %cr4
    
    mov $0xC0000080, %ecx
    rdmsr
    or $0x100, %eax
    wrmsr
    
    mov %cr0, %eax
    or $0x80000001, %eax
    mov %eax, %cr0
    
    popa
    ret
