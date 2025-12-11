# Contributing to Devine

Thank you for your interest in contributing to the Devine kernel project!

## Getting Started

1. Fork the repository
2. Clone your fork
3. Install prerequisites (see README.md)
4. Create a feature branch
5. Make your changes
6. Test on both architectures (if applicable)
7. Submit a pull request

## Code Style

### Rust Code

- Use `#![no_std]` for all kernel crates
- No comments unless absolutely necessary - code should be self-documenting
- Use `const fn new()` for static initialization
- Minimize `unsafe` code and document why it's needed
- Follow existing patterns for architecture abstraction

**Example:**
```rust
#[repr(C)]
pub struct DeviceRegister {
    control: Volatile<u32>,
    status: Volatile<u32>,
}

impl DeviceRegister {
    pub const fn new(base: usize) -> Self {
        Self {
            control: Volatile::new(base as *mut u32),
            status: Volatile::new((base + 4) as *mut u32),
        }
    }
}
```

### C++ Code

- Must be freestanding (no standard library)
- No exceptions (`-fno-exceptions`)
- No RTTI (`-fno-rtti`)
- Use `extern "C"` for FFI functions
- Minimal/no comments

**Example:**
```cpp
extern "C" {
    void device_init(uint32_t base) {
        volatile uint32_t* ctrl = reinterpret_cast<volatile uint32_t*>(base);
        *ctrl = 1;
    }
}
```

## Architecture Guidelines

### Adding Architecture-Specific Code

1. Create new crate: `crates/devine-arch-{arch}/`
2. Implement `ArchOps` trait from `devine-arch`
3. Add architecture-specific modules (interrupt controller, MMU, etc.)
4. Update build configuration

### Cross-Platform Code

- Put shared abstractions in `devine-arch`
- Put common kernel logic in `devine-kernel-core`
- Use conditional compilation when needed:
  ```rust
  #[cfg(target_arch = "x86_64")]
  use devine_arch_x86_64::X86_64 as Arch;
  
  #[cfg(target_arch = "aarch64")]
  use devine_arch_aarch64::AArch64 as Arch;
  ```

## Testing

### Before Submitting

- [ ] Code builds for x86_64: `cargo build --target x86_64-devine.json`
- [ ] Code builds for aarch64: `cargo build --target aarch64-devine.json`
- [ ] Check passes: `cargo check --workspace`
- [ ] Bootable image creates successfully (if applicable)
- [ ] Tested in QEMU (if applicable)

### Testing Commands

```bash
# Check all crates
make check

# Test x86_64 build
make build-x86_64

# Test aarch64 build
make build-aarch64

# Run in QEMU
make run-x86_64
make run-aarch64
```

## Pull Request Guidelines

### PR Title Format

- `[arch]` prefix for architecture-specific changes
- `[core]` for kernel core changes
- `[build]` for build system changes
- `[docs]` for documentation

**Examples:**
- `[x86_64] Add APIC support`
- `[core] Implement heap allocator`
- `[build] Update linker scripts for new memory layout`

### PR Description

Include:
1. What problem does this solve?
2. What approach did you take?
3. Testing performed
4. Any breaking changes?
5. Related issues (if any)

### Code Review Process

1. Automated checks must pass
2. At least one maintainer approval required
3. Builds successfully for all targets
4. Follows code style guidelines

## Common Patterns

### Spin Locks

```rust
use spin::Mutex;

pub static DEVICE: Mutex<Device> = Mutex::new(Device::new());

pub fn do_something() {
    let mut device = DEVICE.lock();
    device.operation();
}
```

### MMIO Access

```rust
use volatile::Volatile;

#[repr(C)]
struct Registers {
    control: Volatile<u32>,
}

unsafe {
    let regs = &mut *(0x1000_0000 as *mut Registers);
    regs.control.write(1);
}
```

### Bitflags

```rust
bitflags::bitflags! {
    pub struct Flags: u32 {
        const ENABLE = 1 << 0;
        const READY = 1 << 1;
        const ERROR = 1 << 2;
    }
}
```

## C++ Integration

### Adding C++ Functions

1. Add function to `.cpp` file with `extern "C"`
2. Add declaration to `.hpp` header
3. Add to `#[cxx::bridge]` in Rust
4. Create safe Rust wrapper

**Example:**

C++ (`crypto.cpp`):
```cpp
extern "C" {
    uint64_t compute_checksum(const uint8_t* data, size_t len) {
        // Implementation
    }
}
```

Header (`crypto.hpp`):
```cpp
extern "C" {
    uint64_t compute_checksum(const uint8_t* data, size_t len);
}
```

Rust bridge (`lib.rs`):
```rust
#[cxx::bridge]
mod ffi {
    unsafe extern "C++" {
        include!("crypto.hpp");
        fn compute_checksum(data: &[u8]) -> u64;
    }
}

pub fn checksum(data: &[u8]) -> u64 {
    ffi::compute_checksum(data)
}
```

## Commit Messages

Use conventional commits format:

```
<type>(<scope>): <subject>

<body>

<footer>
```

**Types:**
- `feat`: New feature
- `fix`: Bug fix
- `docs`: Documentation changes
- `refactor`: Code refactoring
- `perf`: Performance improvement
- `test`: Test changes
- `build`: Build system changes

**Example:**
```
feat(x86_64): add APIC timer support

Implement APIC timer for high-precision timing on x86_64.
Replaces PIT for time keeping.

Closes #123
```

## Questions?

- Open an issue for questions
- Check existing issues and PRs
- Read the documentation in README.md and BUILD_SYSTEM.md

## License

By contributing, you agree that your contributions will be licensed under the CC0 1.0 Universal (Public Domain) license.
