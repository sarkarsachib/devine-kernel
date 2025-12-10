# Devine Kernel - Quick Start Guide

## One-Time Setup

```bash
# Install Rust nightly
curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh
rustup default nightly

# Install required components and tools
make install-tools

# Install system dependencies (Ubuntu/Debian)
sudo apt install qemu-system-x86 qemu-system-aarch64 \
                 clang llvm lld \
                 gcc-aarch64-linux-gnu g++-aarch64-linux-gnu

# Or on macOS
brew install qemu llvm
```

## Build Commands

```bash
# Build for x86_64
make build-x86_64

# Build for aarch64
make build-aarch64

# Create bootable x86_64 image
make bootimage-x86_64

# Check all crates compile
make check
```

## Running

```bash
# Run x86_64 kernel in QEMU
make run-x86_64

# Run aarch64 kernel in QEMU
make run-aarch64
```

## Manual Build (without Makefile)

### x86_64

```bash
# Build
cargo build --release --target x86_64-devine.json

# Create bootable image
cargo bootimage --release --target x86_64-devine.json

# Run in QEMU
qemu-system-x86_64 \
    -drive format=raw,file=target/x86_64-devine/release/bootimage-devine-kernel-core.bin \
    -serial stdio
```

### aarch64

```bash
# Build
cargo build --release --target aarch64-devine.json

# Extract binary
cargo objcopy --release --target aarch64-devine.json -- \
    -O binary target/aarch64-devine/release/devine-kernel-core.bin

# Run in QEMU
qemu-system-aarch64 \
    -machine virt \
    -cpu cortex-a72 \
    -kernel target/aarch64-devine/release/devine-kernel-core \
    -serial stdio
```

## Project Structure

```
devine/
├── crates/               # Workspace members
│   ├── devine-kernel-core    # Core kernel
│   ├── devine-boot           # Boot logic
│   ├── devine-arch           # Arch abstractions
│   ├── devine-arch-x86_64    # x86_64 impl
│   ├── devine-arch-aarch64   # ARM64 impl
│   └── devine-perf-cpp       # C++ modules
├── linker/              # Linker scripts
│   ├── x86_64.ld
│   └── aarch64.ld
├── .cargo/
│   └── config.toml      # Build configuration
├── x86_64-devine.json   # x86_64 target spec
├── aarch64-devine.json  # aarch64 target spec
├── Cargo.toml           # Workspace definition
├── Makefile             # Build automation
└── README.md            # Full documentation
```

## Common Tasks

### Add a new kernel module

1. Create module file: `crates/devine-kernel-core/src/mymodule.rs`
2. Add to lib.rs: `pub mod mymodule;`
3. Use in kernel: `use devine_kernel_core::mymodule;`

### Add x86_64-specific code

1. Create file: `crates/devine-arch-x86_64/src/mydevice.rs`
2. Add to lib.rs: `pub mod mydevice;`
3. Use in architecture implementation

### Add C++ performance code

1. Add header: `crates/devine-perf-cpp/include/myalgo.hpp`
2. Add source: `crates/devine-perf-cpp/src/cpp/myalgo.cpp`
3. Update build.rs to compile it
4. Add to FFI bridge in lib.rs
5. Create Rust wrapper functions

## Troubleshooting

### "rust-lld not found"
```bash
rustup component add llvm-tools-preview
```

### "cannot find -lgcc"
```bash
# Install cross-compilation toolchain
sudo apt install gcc-aarch64-linux-gnu
```

### "bootimage not found"
```bash
cargo install bootimage
```

### Build fails with linker errors
- Check that linker scripts exist in `linker/` directory
- Verify paths in target JSON files

## Next Steps

1. Read [README.md](README.md) for comprehensive documentation
2. Check [BUILD_SYSTEM.md](BUILD_SYSTEM.md) for build system details
3. See [CONTRIBUTING.md](CONTRIBUTING.md) for contribution guidelines
4. Explore the codebase starting with `devine-kernel-core`

## Resources

- **Rust Embedded**: https://rust-embedded.github.io/book/
- **OSDev Wiki**: https://wiki.osdev.org/
- **Writing an OS in Rust**: https://os.phil-opp.com/

## Getting Help

- Open an issue on GitHub
- Check existing issues for similar problems
- Read the full documentation
