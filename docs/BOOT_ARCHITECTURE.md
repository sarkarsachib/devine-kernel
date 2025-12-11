# Boot Architecture Documentation

This document describes the complete boot architecture for both x86_64 and ARM64 processors.

## Overview

The kernel boot process is split into three phases:

1. **Bootloader Phase**: External bootloader loads the kernel into memory
2. **Assembly Phase**: Low-level initialization in assembly code
3. **Rust Phase**: High-level initialization in Rust code

## x86_64 Boot Sequence

### Multiboot2 Header

The kernel starts with a Multiboot2 header at the beginning of the `.multiboot` section:

```
Offset  | Field           | Value
--------|-----------------|--------
0x00    | magic           | 0x1BADB002
0x04    | flags           | ALIGN | MEMINFO
0x08    | checksum        | -(magic + flags)
```

The header is located at virtual address 0x00100000 (1MB), following the Multiboot2 specification.

### Protected Mode (32-bit) Entry

When control passes to `_start`:

1. CPU is in 32-bit protected mode
2. Interrupts are disabled (CLI)
3. Stack pointer is invalid - must be initialized

### Assembly Entry Point (`src/x86_64/boot.s`)

```assembly
_start:
    cli                    ; Disable interrupts
    mov $stack_top, %esp   ; Initialize stack (grows downward)
    push %ebx              ; Save multiboot info
    push %eax              ; Save magic number
    call setup_paging      ; Configure page tables
    pop %eax               ; Restore magic
    pop %ebx               ; Restore multiboot info
    push %ebx              ; Pass as argument to kmain
    call kmain
    hlt
```

### Paging Setup (`setup_paging`)

x86_64 requires paging to be enabled before entering 64-bit long mode. The setup:

1. **Clear Page Tables**: Zero out L2, L3, L4 tables
2. **Identity Mapping**: Map first 1GB of physical memory (0x00000000-0x3FFFFFFF)
   - L4 entry 0: points to L3 table
   - L3 entry 0: points to L2 table  
   - L2 entries 0-511: 2MB huge pages with present and writable bits

3. **Enable PAE** (Physical Address Extension):
   ```
   mov %cr4, %eax
   or $0x20, %eax         ; Set PAE bit (bit 5)
   mov %eax, %cr4
   ```

4. **Enable Long Mode**:
   ```
   mov $0xC0000080, %ecx  ; IA32_EFER MSR
   rdmsr
   or $0x100, %eax        ; Set LME bit
   wrmsr
   ```

5. **Enable Paging**:
   ```
   mov %cr0, %eax
   or $0x80000001, %eax   ; Set PG and PE bits
   mov %eax, %cr0
   ```

6. **Load CR3** (Page Directory Base Register):
   ```
   mov $boot_page_table_l4, %eax
   mov %eax, %cr3
   ```

### GDT/IDT Setup

After paging is enabled, the code transitions from 32-bit to 64-bit mode:

```assembly
ljmp $0x08, $1f  ; Long jump using GDT selector 0x08 (kernel code)
1:               ; Now in 64-bit mode
```

The Global Descriptor Table (GDT) is defined in `src/x86_64/gdt.rs`:

| Index | Descriptor | Type | Privilege | Function |
|-------|-----------|------|-----------|----------|
| 0     | Null      | —    | —         | Required by x86 spec |
| 1     | Code (32) | Code | Ring 0    | Kernel code (32-bit) |
| 2     | Data (32) | Data | Ring 0    | Kernel data/stack |
| 3     | Code (64) | Code | Ring 3    | User code (64-bit) |
| 4     | Data (64) | Data | Ring 3    | User data/stack |

The Interrupt Descriptor Table (IDT) is defined in `src/x86_64/idt.rs`:

- 256 entries for interrupt vectors 0-255
- Currently all entries are zero (disabled)
- Can be extended to handle specific interrupts

### Jump to Rust `kmain()`

Finally, control transfers to the Rust kernel main function:

```c
extern "C" fn kmain(hw_info: *const HardwareInfo) -> !
```

The parameter contains hardware information discovered by the bootloader.

## ARM64 Boot Sequence

### Device Tree or Bootloader Handoff

ARM64 processors typically boot through one of:
- **U-Boot**: Open-source bootloader
- **UEFI**: Unified Extensible Firmware Interface
- **QEMU**: Directly loads kernel with device tree

When `_start` is entered:

1. CPU is in EL1 (Exception Level 1) - hypervisor mode
2. MMU is likely disabled
3. Stack pointer is uninitialized

### Assembly Entry Point (`src/arm64/boot.s`)

```assembly
_start:
    mrs x0, mpidr_el1      ; Read Multiprocessor ID register
    and x0, x0, #0xFF      ; Extract CPU ID
    cbz x0, primary_cpu    ; If CPU 0, continue; else halt
    
primary_cpu:
    ldr x0, =stack_top     ; Load stack address
    mov sp, x0             ; Set stack pointer
    bl setup_exceptions    ; Initialize exception vectors
    bl clear_bss           ; Clear BSS section
    mov x0, xzr            ; Clear first argument
    b kmain                ; Jump to Rust kernel
```

### Exception Vector Setup

ARM64 uses exception vectors for interrupts and exceptions. The `VBAR_EL1` register points to the exception vector table:

- Base address must be 2KB (11-bit) aligned
- Contains 8 exception vectors for EL1
- Each vector has 128 bytes (2^7) of space

Vectors (in order):
1. EL1 Synchronous (Current SP_EL0) - Exceptions from application
2. EL1 IRQ (Current SP_EL0)
3. EL1 FIQ (Current SP_EL0)
4. EL1 SError (Current SP_EL0) - System error
5. EL1 Synchronous (Current SP_ELx) - Exceptions from kernel
6. EL1 IRQ (Current SP_ELx)
7. EL1 FIQ (Current SP_ELx)
8. EL1 SError (Current SP_ELx)

Currently, all vectors point to `exception_handler` which halts the CPU:

```assembly
.section .text
.align 11                  ; 2KB alignment
exception_vectors:
    b exception_handler
    .align 7               ; Each vector gets 128 bytes
    
    b exception_handler
    .align 7
    
    ; ... (6 more vectors)
    
exception_handler:
    hlt
    b exception_handler
```

### BSS Clearing

The BSS (Block Started by Symbol) section contains uninitialized data that must be zeroed:

```assembly
clear_bss:
    ldr x0, =bss_start     ; Load start of BSS
    ldr x1, =bss_end       ; Load end of BSS
    mov x2, xzr            ; Clear x2
    
clear_loop:
    cmp x0, x1             ; Check if we've reached the end
    b.ge clear_done        ; If x0 >= x1, we're done
    str x2, [x0]           ; Store zero at [x0]
    add x0, x0, #8         ; Move to next 8-byte word
    b clear_loop
    
clear_done:
    ret
```

### Jump to Rust `kmain()`

Finally, control transfers to the Rust kernel main function with no arguments:

```c
extern "C" fn kmain(_hw_info: *const HardwareInfo) -> !
```

For ARM64, hardware info is typically obtained from the device tree or bootloader protocols, not passed directly.

## Memory Layout

### x86_64 Memory Layout

```
Virtual Address Space:
+--------------------+ 0xFFFFFFFFFFFFFFFF
|   Kernel Space     |
+--------------------+ 0xFFFF800000000000
|                    |
|   Unused           |
|                    |
+--------------------+ 0x0000000100000000
|                    |
|   User Space       |
|                    |
+--------------------+ 0x0000000000000000

Physical Memory (after identity mapping):
+--------------------+ 0x40000000 (1GB)
|   Unused           |
+--------------------+ 0x00400000 (4MB)
|   Kernel BSS       |
|   Kernel Data      |
|   Kernel Code      |
|   Multiboot Header |
+--------------------+ 0x00000000

Loading:
Bootloader loads kernel at 0x00100000 (1MB)
Identity map: 0x00100000 -> 0x00100000
```

### ARM64 Memory Layout

```
Virtual Address Space:
+--------------------+ 0xFFFFFFFFFFFFFFFF
|   Kernel Space     |
+--------------------+ 0xFFFF000000000000
|                    |
|   Unused           |
|                    |
+--------------------+ 0x0000000100000000
|                    |
|   User Space       |
|                    |
+--------------------+ 0x0000000000000000

Physical Memory:
+--------------------+ 0x80400000
|   Kernel BSS       |
|   Kernel Data      |
|   Kernel Code      |
+--------------------+ 0x80000000
|   Firmware Region  |
+--------------------+ 0x00000000

Loading:
QEMU/U-Boot loads kernel at 0x80000000 (2GB)
MMU initially disabled (identity mapping)
```

## Hardware Information Passing

### x86_64 (Multiboot2)

The bootloader passes:
- `%eax = 0x36d76289` (Multiboot2 magic)
- `%ebx = address of multiboot info structure`

The info structure contains:
- Total size of info
- Reserved field
- Tags (memory map, framebuffer info, etc.)

Currently simplified - only pointer is passed to `kmain()`.

### ARM64 (Device Tree)

ARM64 typically receives:
- `x0`: Physical address of device tree blob (DTB)
- `x1`: Reserved (0)
- `x2`: Reserved (0)
- `x3`: Reserved (0)

The DTB contains complete system configuration.

Currently, hardware info is not extracted from device tree in this implementation.

## Stack Layout

### Before Calling kmain

The stack frame when entering kmain:

```
SP -> [Return address of kmain] (pushed by CALL instruction)
      ...
      [Previous stack frames]
```

### Local Variables

The kernel can use stack space for:
- Page table storage during paging setup
- Local variables in kmain
- Function call parameters

## Boot Linker Scripts

### x86_64 (`src/x86_64/linker.ld`)

```
SECTIONS {
    . = 0x00100000;           # Load at 1MB
    
    .multiboot : {
        *(.multiboot)         # Multiboot header must be first
    }
    
    .text : {
        *(.text)              # Kernel code
    }
    
    .rodata : {
        *(.rodata)            # Read-only data
    }
    
    .data : {
        *(.data)              # Initialized data
    }
    
    .bss : {
        *(COMMON)
        *(.bss)               # Uninitialized data
    }
}
```

### ARM64 (`src/arm64/linker.ld`)

```
SECTIONS {
    . = 0x80000000;           # Load at 2GB (typical ARM load address)
    
    .text : {
        *(.text.boot)         # Boot code (must be first)
        *(.text)              # Kernel code
    }
    
    .rodata : {
        *(.rodata)            # Read-only data
    }
    
    .data : {
        *(.data)              # Initialized data
    }
    
    .bss : {
        *(COMMON)
        *(.bss)               # Uninitialized data
    }
}
```

## Future Enhancements

### x86_64
- Full Multiboot2 tag parsing
- ACPI table detection
- SMBIOS information extraction
- GDT/IDT dynamic loading with proper handlers
- 64-bit paging setup with proper page attributes
- Bootloader selection (Limine, GRUB, custom)

### ARM64
- Device tree (FDT) parsing
- PSCI (Power State Coordination Interface) for CPU control
- GICv2/v3 (Generic Interrupt Controller) support
- System register initialization
- SMP (Symmetric Multi-Processing) support

## References

- [OSDev x86_64](https://wiki.osdev.org/Main_Page)
- [AMD64 Architecture](https://www.amd.com/en/technologies/amd64)
- [ARM Architecture Reference Manual](https://developer.arm.com/documentation)
- [Multiboot2 Specification](https://www.gnu.org/software/grub/manual/multiboot2/html_node/index.html)
