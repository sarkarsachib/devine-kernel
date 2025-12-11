/* ARM64 Linker Script */
OUTPUT_FORMAT("elf64-littleaarch64")
OUTPUT_ARCH(aarch64)
ENTRY(_start)

SECTIONS
{
    . = 0x80000000;

    .text :
    {
        *(.text.boot)
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
