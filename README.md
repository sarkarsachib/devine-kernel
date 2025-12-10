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
