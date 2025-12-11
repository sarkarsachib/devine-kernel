# Quick Start Guide

This guide will help you build and run the kernel on both x86_64 and ARM64.

## Prerequisites

### System Requirements
- Linux system (Ubuntu 20.04+ recommended)
- At least 4GB RAM
- ~2GB disk space

### Install Build Tools

#### For x86_64
```bash
sudo apt-get install -y \
    build-essential \
    gcc-i686-linux-gnu \
    binutils-i686-linux-gnu \
    qemu-system-x86
```

#### For ARM64
```bash
sudo apt-get install -y \
    gcc-aarch64-linux-gnu \
    binutils-aarch64-linux-gnu \
    qemu-system-arm
```

#### Install Rust
```bash
curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh
source $HOME/.cargo/env
```

### Add Rust Targets
```bash
rustup target add i686-unknown-linux-gnu
rustup target add aarch64-unknown-linux-gnu
```

## Building

### Build x86_64 Kernel
```bash
./build.sh x86_64
```

Output: `build/x86_64/kernel.elf`

### Build ARM64 Kernel
```bash
./build.sh arm64
```

Output: `build/arm64/kernel.elf`

### Build Both
```bash
make all
```

## Running on QEMU

### x86_64
```bash
./scripts/qemu-x86_64.sh
```

Expected output:
```
Launching x86_64 kernel on QEMU...
Kernel: build/x86_64/kernel.elf
```

The kernel will boot and then halt (infinite loop).

### ARM64
```bash
./scripts/qemu-arm64.sh
```

Expected output:
```
Launching ARM64 kernel on QEMU...
Kernel: build/arm64/kernel.elf
```

The kernel will boot and then halt (infinite loop).

## Debugging

### With QEMU GDB Support

x86_64:
```bash
./scripts/qemu-x86_64.sh -s -S &
gdb build/x86_64/kernel.elf
(gdb) target remote localhost:1234
(gdb) break _start
(gdb) continue
```

ARM64:
```bash
./scripts/qemu-arm64.sh -s -S &
gdb build/arm64/kernel.elf
(gdb) target remote localhost:1234
(gdb) break _start
(gdb) continue
```

### Verbose Output

Add `-d` flag for QEMU debug output:
```bash
./scripts/qemu-x86_64.sh -d int
./scripts/qemu-arm64.sh -d exec
```

## Project Structure

```
.
├── Cargo.toml              # Rust project manifest
├── Makefile                # Build automation
├── build.sh                # Build script
├── limine.cfg              # Limine bootloader config
├── src/
│   ├── main.rs             # Rust entry point
│   ├── lib.rs              # Library exports
│   ├── hwinfo.rs           # Hardware info structures
│   ├── x86_64/
│   │   ├── boot.s          # Assembly boot code
│   │   ├── linker.ld       # Linker script
│   │   ├── gdt.rs          # GDT setup
│   │   └── idt.rs          # IDT setup
│   └── arm64/
│       ├── boot.s          # Assembly boot code
│       ├── linker.ld       # Linker script
│       └── mod.rs          # ARM64 module
├── scripts/
│   ├── qemu-x86_64.sh      # QEMU x86_64 launcher
│   └── qemu-arm64.sh       # QEMU ARM64 launcher
└── docs/
    ├── BOOT_ARCHITECTURE.md # Detailed boot documentation
    └── QUICK_START.md       # This file
```

## Troubleshooting

### "Command not found: i686-linux-gnu-gcc"

Install the cross-compiler:
```bash
sudo apt-get install gcc-i686-linux-gnu
```

### "Error: linker 'i686-linux-gnu-gcc' not found"

Ensure the `.cargo/config.toml` has the correct linker specified. Update it with:
```bash
rustup update
```

### QEMU crashes during boot

- Ensure `build/x86_64/kernel.elf` or `build/arm64/kernel.elf` exists
- Check QEMU version: `qemu-system-x86_64 --version`
- Try running with `-nographic` flag

### Assembly errors during build

- Ensure correct assembler for the architecture
- For x86_64: `i686-linux-gnu-as --version`
- For ARM64: `aarch64-linux-gnu-as --version`

## Next Steps

1. Read the [BOOT_ARCHITECTURE.md](BOOT_ARCHITECTURE.md) for detailed boot information
2. Add interrupt handlers to x86_64 IDT in `src/x86_64/idt.rs`
3. Implement basic console driver
4. Add memory management (page allocator, heap)
5. Set up timer interrupts
6. Implement basic file system
7. Add user mode support

## References

- [OSDev Wiki](https://wiki.osdev.org/)
- [Multiboot2 Specification](https://www.gnu.org/software/grub/manual/multiboot2/)
- [ARM Architecture Manual](https://developer.arm.com/documentation/)
- [QEMU Documentation](https://qemu-project.gitlab.io/qemu/index.html)
