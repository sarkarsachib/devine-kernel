/* x86_64 Boot Code and Kernel Entry Point */

#include "../include/types.h"

// External functions
extern void kmain(void);
extern void idt_init(void);
extern void apic_init(void);
extern void gdt_init(void);

// Boot information structure
typedef struct {
    u64 magic;
    u64 flags;
    u64 mem_lower;
    u64 mem_upper;
    u64 boot_device;
    u64 cmdline;
    u64 mods_count;
    u64 mods_addr;
    u64 syms[4];
    u64 mmap_length;
    u64 mmap_addr;
    u64 drives_length;
    u64 drives_addr;
    u64 config_table;
    u64 boot_loader_name;
    u64 apm_table;
    u64 vbe_control_info;
    u64 vbe_mode_info;
    u16 vbe_mode;
    u16 vbe_interface_seg;
    u16 vbe_interface_off;
    u16 vbe_interface_len;
} __attribute__((packed)) boot_info_t;

// Multiboot header for bootloader
.section .multiboot
.align 8
multiboot_header:
    .long 0x1BADB002           // Magic number
    .long 0x00000003           // Flags
    .long -(0x1BADB002 + 0x00000003) // Checksum

// Stack for kernel
.section .bss
.align 16
stack_bottom:
    .skip 16384                // 16KB stack
stack_top:

// Kernel entry point
.section .text
.global _start
.type _start, @function

_start:
    // Set up stack
    mov $stack_top, %rsp
    
    // Initialize CPU state
    call gdt_init
    call idt_init
    call apic_init
    
    // Jump to kernel main
    call kmain
    
    // Halt CPU if kmain returns
1:  hlt
    jmp 1b

.size _start, . - _start

// Basic memory operations
.global memset
.global memcpy
.global memmove
.global strlen

memset:
    mov %rdi, %rax
    mov %rdx, %rcx
    rep stosb
    ret

memcpy:
    mov %rdi, %rax
    mov %rsi, %rdx
    mov %rcx, %r8
    rep movsb
    ret

memmove:
    // Handle overlapping regions
    cmp %rdi, %rsi
    jl .memmove_forward
    add %rdx, %rsi
    add %rdx, %rdi
    std
    rep movsb
    cld
    ret
.memmove_forward:
    rep movsb
    ret

strlen:
    xor %rax, %rax
1:  cmpb $0, (%rdi, %rax, 1)
    je 2f
    inc %rax
    jmp 1b
2:  ret