ENTRY(_start)

SECTIONS {
    . = 1M;

    .boot :
    {
        KEEP(*(.multiboot_header))
    }

    .text : ALIGN(4K)
    {
        *(.text .text.*)
    }

    .rodata : ALIGN(4K)
    {
        *(.rodata .rodata.*)
    }

    .data : ALIGN(4K)
    {
        *(.data .data.*)
    }

    .bss : ALIGN(4K)
    {
        *(COMMON)
        *(.bss .bss.*)
    }

    .got : ALIGN(4K)
    {
        *(.got)
    }

    .got.plt : ALIGN(4K)
    {
        *(.got.plt)
    }

    /DISCARD/ : {
        *(.eh_frame)
        *(.note*)
    }
}
