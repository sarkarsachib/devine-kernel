# Kernel Boot and Architecture Initialization

This project implements low-level boot paths for both x86_64 and ARM64 architectures, including:

- Assembly stubs for CPU initialization
- Linker scripts for memory layout
- Hardware information discovery and passing to Rust
- QEMU simulation support for both platforms
- GDT/IDT setup for x86_64
- Exception vector initialization for ARM64

## Project Structure

```
├── src/
│   ├── main.rs              # Rust kernel entry point (kmain)
│   ├── hwinfo.rs            # Hardware information structures
│   ├── x86_64/
│   │   ├── boot.s           # x86_64 assembly boot code (Multiboot2)
│   │   ├── linker.ld        # x86_64 linker script
│   │   ├── gdt.rs           # Global Descriptor Table setup
│   │   ├── idt.rs           # Interrupt Descriptor Table setup
│   │   ├── mod.rs           # x86_64 module
│   │   └── multiboot2.h     # Multiboot2 header definitions
│   └── arm64/
│       ├── boot.s           # ARM64 assembly boot code
│       ├── linker.ld        # ARM64 linker script
│       └── mod.rs           # ARM64 module
├── scripts/
│   ├── qemu-x86_64.sh       # QEMU launcher for x86_64
│   └── qemu-arm64.sh        # QEMU launcher for ARM64
├── build.sh                 # Build script for both architectures
├── limine.cfg               # Limine bootloader configuration
├── Cargo.toml               # Rust project manifest
└── README.md                # This file
```

## Building

### Prerequisites

For x86_64:
- `i686-linux-gnu` cross-compiler and binutils
- Rust with `i686-unknown-linux-gnu` target

For ARM64:
- `aarch64-linux-gnu` cross-compiler and binutils
- Rust with `aarch64-unknown-linux-gnu` target

Install targets:
```bash
rustup target add i686-unknown-linux-gnu
rustup target add aarch64-unknown-linux-gnu
```

### Build x86_64 Kernel

```bash
./build.sh x86_64
```

This will produce `build/x86_64/kernel.elf`

### Build ARM64 Kernel

```bash
./build.sh arm64
```

This will produce `build/arm64/kernel.elf`

## Running on QEMU

### x86_64

```bash
chmod +x scripts/qemu-x86_64.sh
./scripts/qemu-x86_64.sh
```

The script will launch QEMU with:
- 256MB RAM
- Serial output to stdout
- No graphical display
- Kernel loaded via Multiboot2 protocol

### ARM64

```bash
chmod +x scripts/qemu-arm64.sh
./scripts/qemu-arm64.sh
```

The script will launch QEMU with:
- 256MB RAM
- Serial output to stdout
- No graphical display
- Cortex-A72 CPU emulation

## Boot Architecture

### x86_64 Boot Flow

1. **Bootloader** (e.g., QEMU BIOS, Limine)
   - Loads kernel ELF at 0x100000
   - Sets up initial stack
   - Enters 32-bit protected mode

2. **Assembly (_start)**
   - Sets up 32-bit stack at `stack_top`
   - Calls `setup_paging()` to configure page tables
   - Jumps to `kmain()` in Rust

3. **Paging Setup**
   - Clears L2, L3, L4 page tables
   - Identity maps first 1GB (0x00000000-0x3FFFFFFF)
   - Enables paging and long mode

4. **GDT/IDT**
   - Currently set up as stubs in `gdt.rs` and `idt.rs`
   - Can be extended with interrupt handlers

### ARM64 Boot Flow

1. **Bootloader** (QEMU/U-Boot)
   - Loads kernel ELF at 0x80000000
   - Enters EL1 (Exception Level 1)
   - Provides CPU in expected state

2. **Assembly (_start)**
   - Checks MPIDR_EL1 to identify primary CPU
   - Initializes exception vectors
   - Clears BSS section
   - Jumps to `kmain()` in Rust

3. **Exception Vectors**
   - Defined at aligned 2KB boundary (11 bits)
   - 8 exception vectors for EL1
   - Each vector is 128 bytes (2^7)

## Hardware Information

The `hwinfo::HardwareInfo` structure passed to `kmain()` contains:

- **Memory Regions**: Memory map from bootloader
- **CPU Info**: Vendor ID, family, model, stepping
- **Framebuffer**: Address, resolution, pitch, and format (if available)

Currently, bootloader integration for passing this data is simplified.
Extend with:
- Limine bootloader protocol parsing
- Multiboot2 memory map extraction
- Device tree parsing for ARM64

## Future Improvements

- [ ] Full Limine protocol support with memory map extraction
- [ ] Multiboot2 tag parsing
- [ ] Device tree (FDT) support for ARM64
- [ ] Page table management abstractions
- [ ] More complete exception handling
- [ ] ACPI/UEFI integration
- [ ] SMP (multi-CPU) support
- [ ] Virtual memory abstraction layer

## References

- [OSDev x86_64 Boot](https://wiki.osdev.org/Creating_a_64-bit_kernel)
- [ARM64 ARM - Exception Handling](https://developer.arm.com/documentation)
- [Limine Bootloader](https://github.com/limine-bootloader/limine)
- [Multiboot2 Specification](https://www.gnu.org/software/grub/manual/multiboot2.html)
- [QEMU x86_64 Emulation](https://qemu-project.gitlab.io/qemu/system/qemu-manpage.html)
- [QEMU ARM Emulation](https://qemu-project.gitlab.io/qemu/system/arm-targets.html)

## License

This project is licensed under the CC0 1.0 Universal license.
