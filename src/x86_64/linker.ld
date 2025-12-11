/* x86_64 Multiboot2 Linker Script */
OUTPUT_FORMAT("elf32-i386")
OUTPUT_ARCH(i386)
ENTRY(_start)

SECTIONS
{
    . = 0x00100000;

    .multiboot :
    {
        *(.multiboot)
    }

    .text :
    {
        *(.text)
        *(.text.*)
    }

    .rodata :
    {
        *(.rodata)
        *(.rodata.*)
    }

    .data :
    {
        *(.data)
        *(.data.*)
    }

    .bss :
    {
        *(COMMON)
        *(.bss)
        *(.bss.*)
    }

    /DISCARD/ : { *(.note.GNU-stack) }
}
