/* x86_64 Kernel Linker Script */

OUTPUT_FORMAT(elf64-x86-64)
OUTPUT_ARCH(i386:x86-64)
ENTRY(_start)

SECTIONS
{
    /* Load kernel at 1MB physical address */
    . = 1M;

    /* Multiboot header - must be first */
    .multiboot ALIGN(8) : {
        *(.multiboot)
    }

    /* Code section */
    .text ALIGN(4K) : {
        *(.text)
    }

    /* Read-only data */
    .rodata ALIGN(4K) : {
        *(.rodata)
    }

    /* Initialized data */
    .data ALIGN(4K) : {
        *(.data)
    }

    /* Uninitialized data (BSS) */
    .bss ALIGN(4K) : {
        *(.bss)
        *(COMMON)
    }

    /* Kernel symbols for debugging */
    .symtab ALIGN(8) : {
        *(.symtab)
        *(.strtab)
    }

    /* Kernel virtual base mapping */
    kernel_virt_base = 0xFFFFFFFF80000000;
    kernel_phys_base = .;
}