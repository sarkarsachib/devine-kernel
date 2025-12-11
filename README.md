# Devine Kernel

A modern, cross-architecture kernel written in Rust with C++ performance-critical modules. Supports x86_64 and ARM64/aarch64 architectures.

## Features

- **Multi-architecture support**: x86_64 and aarch64 targets
- **Cargo workspace**: Modular design with separate crates for kernel core, boot, and architecture-specific code
- **C++ integration**: Performance-critical modules written in C++ via `cc`/`cxx` build scripts
- **Bootable binaries**: Integration with `bootimage` for creating bootable kernel images
- **Custom targets**: Bare-metal targets with custom linker scripts

## Prerequisites

Before building Devine, ensure you have the following tools installed:

### Required

1. **Rust Nightly Toolchain**
   ```bash
   curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh
   rustup default nightly
   rustup component add rust-src llvm-tools-preview
   ```

2. **LLVM/Clang** (for C++ compilation)
   ```bash
   # Ubuntu/Debian
   sudo apt install clang llvm lld
   
   # macOS
   brew install llvm
   
   # Arch Linux
   sudo pacman -S clang llvm lld
   ```

3. **cargo-binutils** (for binary manipulation)
   ```bash
   cargo install cargo-binutils
   ```

4. **bootimage** (for creating bootable images)
   ```bash
   cargo install bootimage
   ```

### Cross-compilation Tools

#### For x86_64 Target
- QEMU for testing
  ```bash
  # Ubuntu/Debian
  sudo apt install qemu-system-x86
  
  # macOS
  brew install qemu
  
  # Arch Linux
  sudo pacman -S qemu-system-x86
  ```

#### For aarch64 Target
- ARM64 cross-compilation toolchain
  ```bash
  # Ubuntu/Debian
  sudo apt install gcc-aarch64-linux-gnu g++-aarch64-linux-gnu
  
  # macOS
  brew install aarch64-elf-gcc
  
  # Arch Linux
  sudo pacman -S aarch64-linux-gnu-gcc
  ```

- QEMU for ARM64
  ```bash
  # Ubuntu/Debian
  sudo apt install qemu-system-aarch64
  
  # macOS
  brew install qemu
  
  # Arch Linux
  sudo pacman -S qemu-system-aarch64
  ```
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
devine/
├── crates/
│   ├── devine-kernel-core/    # Core kernel functionality
│   ├── devine-boot/            # Boot logic and multiboot2 support
│   ├── devine-arch/            # Architecture abstraction layer
│   ├── devine-arch-x86_64/     # x86_64-specific code
│   ├── devine-arch-aarch64/    # ARM64-specific code
│   └── devine-perf-cpp/        # C++ performance-critical modules
├── linker/
│   ├── x86_64.ld              # x86_64 linker script
│   └── aarch64.ld             # aarch64 linker script
├── x86_64-devine.json         # Custom x86_64 target specification
├── aarch64-devine.json        # Custom aarch64 target specification
└── .cargo/
    └── config.toml            # Cargo build configuration
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

### Build for x86_64

```bash
# Build the kernel for x86_64
cargo build --release --target x86_64-devine.json

# Create a bootable image
cargo bootimage --release --target x86_64-devine.json

# The bootable image will be in:
# target/x86_64-devine/release/bootimage-devine-kernel-core.bin
```

### Build for aarch64

```bash
# Set environment variables for cross-compilation
export CC_aarch64_unknown_none=aarch64-linux-gnu-gcc
export CXX_aarch64_unknown_none=aarch64-linux-gnu-g++

# Build the kernel for aarch64
cargo build --release --target aarch64-devine.json

# Extract the binary
cargo objcopy --release --target aarch64-devine.json -- -O binary target/aarch64-devine/release/devine-kernel-core.bin
```

### Build All Crates

```bash
# Build all workspace members
cargo build --workspace

# Build with release optimizations
cargo build --workspace --release
```

## Running

### Run on x86_64 (QEMU)

```bash
# Using bootimage runner (configured in .cargo/config.toml)
cargo bootimage run --release --target x86_64-devine.json

# Or manually with QEMU
qemu-system-x86_64 \
    -drive format=raw,file=target/x86_64-devine/release/bootimage-devine-kernel-core.bin \
    -serial stdio \
    -display none
```

### Run on aarch64 (QEMU)

```bash
# Run with QEMU
qemu-system-aarch64 \
    -machine virt \
    -cpu cortex-a72 \
    -kernel target/aarch64-devine/release/devine-kernel-core \
    -serial stdio \
    -display none
```

## Development

### Check Code

```bash
# Check all workspace crates
cargo check --workspace

# Check for specific target
cargo check --target x86_64-devine.json
cargo check --target aarch64-devine.json
```

### Clean Build Artifacts

```bash
# Clean all build artifacts
cargo clean

# Clean specific target
cargo clean --target x86_64-devine.json
```

### Building Individual Crates

```bash
# Build a specific crate
cargo build -p devine-kernel-core
cargo build -p devine-arch-x86_64
cargo build -p devine-perf-cpp
```

## Architecture-Specific Notes

### x86_64

- Uses Multiboot2 boot protocol
- Initializes GDT, IDT, and PIC
- VGA text mode for output
- Serial port COM1 for debugging

### aarch64

- Boots at physical address 0x40000000
- Uses GICv2 interrupt controller
- UART PL011 for console output
- Designed for QEMU virt machine

## C++ Integration

The `devine-perf-cpp` crate demonstrates C++ integration for performance-critical operations:

- **FNV-1a hashing**: Fast hash computation
- **Optimized memory operations**: Architecture-specific memory copy routines
- **Build integration**: Uses `cc` crate for C++ compilation and `cxx` for FFI bindings

The build script (`build.rs`) automatically configures compiler flags based on the target architecture:
- x86_64: Disables red zone, MMX, and SSE
- aarch64: Enables ARMv8-A features

## Linker Scripts

Custom linker scripts define the memory layout for each architecture:

- **x86_64.ld**: Places kernel at 1MB physical address, aligns sections to 4KB pages
- **aarch64.ld**: Places kernel at 0x40000000, includes special boot section

## Target Specifications

The custom target JSON files define bare-metal compilation targets:

- **x86_64-devine.json**: No OS, no red zone, software floating point
- **aarch64-devine.json**: No OS, strict alignment, NEON/FP support

## Troubleshooting

### Build Errors

**Error: `rust-lld` not found**
```bash
rustup component add llvm-tools-preview
```

**Error: C++ compiler not found**
```bash
# Make sure clang/gcc is in PATH
which clang   # or gcc
```

**Error: Cross-compilation toolchain missing**
```bash
# For aarch64
sudo apt install gcc-aarch64-linux-gnu g++-aarch64-linux-gnu
```

### Runtime Issues

**QEMU won't start**
- Verify QEMU is installed: `qemu-system-x86_64 --version`
- Check binary format: `file target/x86_64-devine/release/bootimage-*.bin`

**Kernel doesn't boot**
- Check linker script is being used
- Verify entry point with: `cargo nm --release -- | grep _start`

## Contributing

Contributions are welcome! Please ensure:
1. Code builds for both x86_64 and aarch64
2. C++ code follows kernel coding standards (no exceptions, no RTTI, freestanding)
3. Architecture-specific code is properly isolated in respective crates

## License

This project is licensed under the CC0 1.0 Universal (Public Domain).

## Resources

- [OSDev Wiki](https://wiki.osdev.org/)
- [Rust Embedded Book](https://rust-embedded.github.io/book/)
- [Writing an OS in Rust](https://os.phil-opp.com/)
- [ARM Architecture Reference Manual](https://developer.arm.com/documentation/)
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
