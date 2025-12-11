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
    
    // Enable long mode (LME) + syscall/sysret (SCE)
    mov $0xC0000080, %ecx        // IA32_EFER
    rdmsr
    or $0x101, %eax              // LME|SCE
    wrmsr

    // Configure syscall entry MSRs.
    // IA32_STAR: bits 63:48 = user CS, bits 47:32 = kernel CS.
    mov $0xC0000081, %ecx        // IA32_STAR
    mov $0x0, %eax
    mov $((0x1B << 16) | 0x08), %edx
    wrmsr

    // IA32_LSTAR: 64-bit RIP of syscall entry stub.
    mov $0xC0000082, %ecx        // IA32_LSTAR
    mov $syscall_entry, %eax
    mov $0x0, %edx
    wrmsr

    // IA32_FMASK: clear IF/DF on entry.
    mov $0xC0000084, %ecx        // IA32_FMASK
    mov $0x600, %eax
    mov $0x0, %edx
    wrmsr
    
    mov %cr0, %eax
    or $0x80000001, %eax
    mov %eax, %cr0
    
    popa
    ret

    // Placeholder syscall entry label for legacy 32-bit builds.
    // In the x86_64 kernel build, this symbol is provided by the Rust
    // syscall entry stub (`src/syscall/entry.rs`).
    .weak syscall_entry
syscall_entry:
    ret
