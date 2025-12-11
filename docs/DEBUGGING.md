# Debugging Guide

This document describes the debugging tools and workflows available for the Devine kernel.

## Table of Contents

- [Overview](#overview)
- [GDB Integration](#gdb-integration)
- [Serial Debugger](#serial-debugger)
- [Profiling](#profiling)
- [QEMU Options](#qemu-options)
- [Common Debugging Scenarios](#common-debugging-scenarios)

## Overview

The Devine kernel provides multiple debugging facilities:

1. **GDB Stub**: Remote debugging via GDB
2. **Serial Debugger**: Simple command-line debugger via serial console
3. **Profiler**: Performance profiling and tracing
4. **QEMU Integration**: Hardware simulation with debugging support

## GDB Integration

### Starting with GDB Support

Launch the kernel with GDB support enabled:

```bash
# x86_64
make qemu-x86_64 GDB=1

# ARM64
make qemu-arm64 GDB=1
```

Or manually:

```bash
# x86_64
./scripts/qemu-x86_64.sh -s -S

# ARM64
./scripts/qemu-arm64.sh -s -S
```

The `-s` flag enables the GDB server on port 1234, and `-S` pauses the CPU at startup.

### Connecting GDB

In a separate terminal:

```bash
# x86_64
gdb build/x86_64/kernel.elf
(gdb) target remote :1234
(gdb) break kmain
(gdb) continue

# ARM64
gdb-multiarch build/arm64/kernel.elf
(gdb) target remote :1234
(gdb) break kmain
(gdb) continue
```

### Useful GDB Commands

```gdb
# Set breakpoint
break function_name
break file.c:line_number

# Step through code
step         # Step into functions
next         # Step over functions
continue     # Continue execution

# Inspect variables
print variable
print *pointer
info registers
info threads

# Memory inspection
x/10x address    # Examine 10 hex words
x/10i address    # Disassemble 10 instructions

# Backtrace
backtrace
frame N          # Switch to frame N
```

## Serial Debugger

The kernel includes a simple serial debugger accessible via the console.

### Commands

- `help` - Show available commands
- `regs` - Display CPU registers
- `mem <addr> <len>` - Display memory contents
- `break <addr>` - Set breakpoint
- `cont` - Continue execution
- `step` - Single-step execution
- `bt` - Show backtrace

### Example Session

```
> help
Available commands:
  help     - Show this help
  regs     - Display registers
  mem      - Display memory
  break    - Set breakpoint
  cont     - Continue execution
  step     - Single-step
  bt       - Show backtrace

> regs
RAX: 0000000000000000  RBX: 0000000000000000
RCX: 0000000000000000  RDX: 0000000000000000
...

> mem 0xFFFFFFFF80000000 64
FFFF FFFF 8000 0000: 4D 5A 90 00 03 00 00 00
FFFF FFFF 8000 0008: 04 00 00 00 FF FF 00 00
...
```

## Profiling

### Enabling Profiling

Build with profiling enabled:

```bash
export PROFILER_ENABLED=1
make build-x86_64
```

Or use the cargo feature:

```bash
cargo build --features profiling --target x86_64-devine.json
```

### Adding Tracepoints

In Rust code:

```rust
use devine_kernel::profile_start;
use devine_kernel::profile_end;
use devine_kernel::profile_count;

fn my_function() {
    profile_start!(b"my_function\0");
    
    // Function code here
    profile_count!(b"operations\0");
    
    profile_end!(b"my_function\0");
}

// Or use RAII-style timing
fn my_timed_function() {
    let _timer = profiler::Timer::new(b"my_timed_function\0");
    // Function code here
    // Timer automatically stops when dropped
}
```

In C code:

```c
#include "profiler.h"

void my_function(void) {
    PROFILE_START("my_function");
    
    // Function code here
    PROFILE_COUNT("operations");
    
    PROFILE_END("my_function");
}
```

### Reading Profiling Data

Profiling data is available via:

1. Serial output during execution
2. Memory-mapped profiling buffer
3. Export to host via QEMU monitor

## QEMU Options

### Standard Debug Options

```bash
# Enable GDB server on port 1234, pause at start
-s -S

# Add serial console
-serial stdio

# Redirect serial to file
-serial file:serial.log

# Monitor console
-monitor telnet:localhost:4444,server,nowait

# Trace execution (warning: very verbose)
-d in_asm,int,cpu

# Log to file
-D qemu.log
```

### Block Device Options

```bash
# Add a disk image
-drive file=disk.img,format=raw,if=virtio

# Create a disk image
qemu-img create -f raw disk.img 100M
```

### Network Options

```bash
# User-mode networking
-netdev user,id=net0 -device virtio-net-pci,netdev=net0

# Tap networking (requires root)
-netdev tap,id=net0,ifname=tap0,script=no,downscript=no \
-device virtio-net-pci,netdev=net0
```

### Full Example

```bash
qemu-system-x86_64 \
    -kernel build/x86_64/kernel.elf \
    -m 256M \
    -serial stdio \
    -display none \
    -no-reboot \
    -s -S \
    -drive file=disk.img,format=raw,if=virtio \
    -netdev user,id=net0 -device virtio-net-pci,netdev=net0
```

## Common Debugging Scenarios

### Kernel Panic / Crash

1. Run with GDB enabled: `-s -S`
2. Connect GDB and set breakpoints at panic handlers
3. Examine backtrace and registers when panic occurs

```gdb
break kernel_panic
continue
# When panic occurs:
backtrace
info registers
```

### Memory Corruption

1. Enable memory debugging in kernel config
2. Use GDB to set watchpoints on memory regions:

```gdb
watch *(u64*)0xFFFFFFFF80001000
continue
# Execution breaks when memory is written
```

### Performance Issues

1. Enable profiling
2. Run stress tests
3. Analyze profiling output to identify hotspots
4. Use `perf` counters if available

### Driver Issues

1. Enable driver debug logging
2. Check device enumeration in boot log
3. Verify PCI/DT device detection
4. Monitor interrupt handling
5. Check DMA buffer contents

### Interrupt Issues

1. Set breakpoint in interrupt handlers
2. Check IDT/GIC configuration
3. Verify interrupt routing
4. Monitor interrupt frequency

```gdb
break interrupt_handler
commands
  silent
  printf "IRQ: %d\n", irq_number
  continue
end
```

## Stress Testing

Run driver stress tests:

```bash
# Build with tests
make test-drivers

# Run specific test
./tests/run_driver_stress.sh block
./tests/run_driver_stress.sh network
./tests/run_driver_stress.sh all
```

## Tips and Tricks

### Debugging Boot Issues

1. Check bootloader logs
2. Verify memory layout in linker script
3. Ensure proper GDT/IDT setup
4. Check initial page tables

### Debugging Concurrency Issues

1. Use GDB to examine all threads: `info threads`
2. Check lock states and ownership
3. Look for deadlocks with lock ordering graph
4. Enable lock debugging in kernel config

### Debugging File System Issues

1. Enable VFS debug logging
2. Trace system calls
3. Verify mount points
4. Check inode/dentry caches

### Performance Profiling

1. Use hardware performance counters
2. Profile specific code paths
3. Measure cache misses and branch mispredictions
4. Analyze interrupt latency

## Additional Resources

- [GDB Documentation](https://sourceware.org/gdb/documentation/)
- [QEMU Documentation](https://www.qemu.org/docs/master/)
- [Linux Kernel Debugging Guide](https://www.kernel.org/doc/html/latest/dev-tools/kgdb.html)

## Troubleshooting

### GDB Won't Connect

- Verify QEMU is listening: `netstat -ln | grep 1234`
- Check firewall settings
- Ensure correct architecture (gdb vs gdb-multiarch)

### No Serial Output

- Check QEMU serial configuration
- Verify UART initialization in kernel
- Try different QEMU versions

### Profiling Data Not Available

- Verify profiling is enabled at compile time
- Check environment variable: `PROFILER_ENABLED=1`
- Ensure profiler initialization runs

### Symbols Not Loading

- Build with debug symbols: `-g`
- Check ELF file has symbols: `nm kernel.elf`
- Load symbols manually: `symbol-file kernel.elf`
