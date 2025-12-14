# AP (Application Processor) Startup Code for x86_64
# This code is executed by secondary processors to bring them online

.code16
.section .text.ap_startup
.global ap_startup_begin
.global ap_startup_end

# AP Startup Entry Point (Real Mode)
# This code is typically loaded at 0x8000 or similar low address
ap_startup_begin:
    cli
    xor %ax, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %ss
    
    # Enable protected mode
    lgdtl gdtr
    mov %cr0, %eax
    or $0x1, %eax
    mov %eax, %cr0
    
    # Long jump to protected mode code
    ljmpl $0x8, $ap_startup_pm

# Protected Mode Entry
.code32
ap_startup_pm:
    # Setup segment registers
    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %ss
    
    # Setup stack
    mov $ap_startup_stack_end, %esp
    
    # Enable paging (assuming 4-level page tables are ready)
    mov $0x80000000, %eax  # Assume page table base is set
    mov %eax, %cr3
    
    # Enable PAE, PSE, and PGE
    mov %cr4, %eax
    or $0xB0, %eax  # PAE | PSE | PGE
    mov %eax, %cr4
    
    # Enable long mode
    mov $0xC0000080, %ecx  # IA32_EFER
    rdmsr
    or $0x100, %eax        # LME bit
    wrmsr
    
    # Enable paging and protection
    mov %cr0, %eax
    or $0x80000001, %eax
    mov %eax, %cr0
    
    # Long jump to 64-bit mode
    ljmpl $0x8, $ap_startup_lm

# Long Mode Entry
.code64
ap_startup_lm:
    # Setup segment registers for 64-bit
    xor %ax, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %ss
    
    # Get CPU ID from APIC
    xor %eax, %eax
    mov $0x1b, %ecx
    rdmsr
    shr $24, %eax
    and $0xFF, %eax
    
    # Call the C function to continue boot
    # ap_startup_main(cpu_id)
    call ap_startup_main
    
    # Should not return
    hlt
    jmp .

# Global Descriptor Table for AP startup
.align 8
gdtr:
    .word 0x17       # GDT size - 1
    .long gdt        # GDT base address

.align 8
gdt:
    # Null descriptor
    .quad 0x0000000000000000
    # Code descriptor (index 1)
    .quad 0x00af9a000000ffff
    # Data descriptor (index 2)
    .quad 0x00af92000000ffff

# AP Startup Stack
.align 16
ap_startup_stack:
    .skip 0x1000
ap_startup_stack_end:

ap_startup_end:
