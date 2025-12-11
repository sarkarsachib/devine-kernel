# Devine Build System Documentation

## Overview

The Devine kernel uses a sophisticated build system that combines Cargo workspaces, custom target specifications, C++ integration, and cross-compilation support.

## Workspace Structure

### Root Cargo.toml
Defines the workspace with 6 member crates and shared dependencies:
- `bootloader`, `spin`, `volatile`, `bitflags` for Rust code
- `cc`, `cxx`, `cxx-build` for C++ integration

### Crate Dependencies Graph

```
devine-kernel-core
├── devine-arch (trait definitions)
└── devine-boot

devine-boot
└── devine-kernel-core

devine-arch (no dependencies)

devine-arch-x86_64
├── devine-arch
└── devine-perf-cpp

devine-arch-aarch64
├── devine-arch
└── devine-perf-cpp

devine-perf-cpp (C++ integration)
```

## Cross-Compilation Configuration

### .cargo/config.toml

**Build Settings:**
- Default target: `x86_64-devine.json`
- Unstable features: `build-std` for core, compiler_builtins, alloc
- Target-specific runners for QEMU

**Environment Variables:**
- `CC_x86_64_unknown_none=clang`
- `CXX_x86_64_unknown_none=clang++`
- `CC_aarch64_unknown_none=aarch64-linux-gnu-gcc`
- `CXX_aarch64_unknown_none=aarch64-linux-gnu-g++`

## Custom Target Specifications

### x86_64-devine.json
- LLVM target: `x86_64-unknown-none`
- Linker: `rust-lld` with `ld.lld` flavor
- Features: Disabled MMX/SSE, soft-float, no red zone
- Code model: `kernel`
- Linker script: `linker/x86_64.ld`

### aarch64-devine.json
- LLVM target: `aarch64-unknown-none`
- Linker: `rust-lld` with `ld.lld` flavor
- Features: Strict alignment, NEON, FP-ARMv8
- Code model: `small`
- Linker script: `linker/aarch64.ld`

## Linker Scripts

### x86_64.ld
- Entry point: `_start`
- Base address: 1MB (0x100000)
- Sections: boot, text, rodata, data, bss, got
- Alignment: 4KB pages
- Multiboot2 header support

### aarch64.ld
- Entry point: `_start`
- Base address: 0x40000000
- Special boot section: `.text.boot`
- BSS markers: `__bss_start`, `__bss_end`
- Kernel end marker: `__kernel_end`

## C++ Integration (devine-perf-cpp)

### build.rs Configuration

**Compiler Flags (all targets):**
- `-std=c++17`: C++17 standard
- `-fno-exceptions`: Disable exception handling
- `-fno-rtti`: Disable runtime type information
- `-nostdlib`: No standard library
- `-ffreestanding`: Freestanding environment

**Architecture-specific flags:**

**x86_64:**
- `-mno-red-zone`: Disable red zone for kernel code
- `-mno-mmx`: Disable MMX instructions
- `-mno-sse`: Disable SSE instructions

**aarch64:**
- `-march=armv8-a`: Target ARMv8-A architecture

### C++ to Rust FFI

Uses `cxx` bridge for safe FFI:
- `fast_hash()`: FNV-1a hash implementation
- `fast_memcpy()`: Architecture-optimized memory copy

## Build Profiles

### Development Profile
- Panic: abort (no unwinding)

### Release Profile
- Panic: abort
- LTO: enabled (link-time optimization)
- Optimization level: 3
- Codegen units: 1 (maximum optimization)

## Build Artifacts

### x86_64
```
target/x86_64-devine/release/
├── devine-kernel-core (ELF)
├── bootimage-devine-kernel-core.bin (bootable)
└── *.a (static libraries)
```

### aarch64
```
target/aarch64-devine/release/
├── devine-kernel-core (ELF)
├── devine-kernel-core.bin (raw binary)
└── *.a (static libraries)
```

## Bootable Image Generation

### x86_64 (bootimage)
1. Compile kernel to ELF
2. `bootimage` tool creates bootloader
3. Combines bootloader + kernel into bootable image
4. Output: `.bin` file for QEMU/real hardware

### aarch64 (manual)
1. Compile kernel to ELF
2. Extract raw binary with `cargo objcopy`
3. Load directly with QEMU `-kernel` flag

## Testing Workflow

### x86_64
```bash
cargo bootimage run --target x86_64-devine.json
# Uses bootimage runner from .cargo/config.toml
```

### aarch64
```bash
cargo build --target aarch64-devine.json
cargo objcopy -- -O binary target/aarch64-devine/release/devine.bin
qemu-system-aarch64 -machine virt -cpu cortex-a72 -kernel devine.bin
```

## Common Build Issues and Solutions

### Issue: `rust-lld` not found
**Solution:** `rustup component add llvm-tools-preview`

### Issue: C++ compiler not found
**Solution:** Install clang/gcc and ensure it's in PATH

### Issue: Cross-compilation fails
**Solution:** Install target-specific toolchain (e.g., `gcc-aarch64-linux-gnu`)

### Issue: Linker script not found
**Solution:** Ensure linker scripts exist in `linker/` directory relative to project root

### Issue: Build-std fails
**Solution:** Use Rust nightly and add `rust-src` component

## Extending the Build System

### Adding a New Architecture

1. Create target JSON: `{arch}-devine.json`
2. Create linker script: `linker/{arch}.ld`
3. Add crate: `crates/devine-arch-{arch}/`
4. Update `.cargo/config.toml` with compiler variables
5. Document in README.md

### Adding C++ Modules

1. Add source to `crates/devine-perf-cpp/src/cpp/`
2. Add header to `crates/devine-perf-cpp/include/`
3. Update `build.rs` to compile new files
4. Add FFI bridge in `lib.rs` using `#[cxx::bridge]`
5. Export safe Rust wrappers

### Custom Build Scripts

All crates can have `build.rs` files. Common uses:
- Generate code from specifications
- Compile assembly files
- Configure based on target architecture
- Run pre-build checks
