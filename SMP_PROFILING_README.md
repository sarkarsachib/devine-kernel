# SMP and Profiling Implementation

## Overview

This document describes the implementation of Symmetric Multi-Processing (SMP) and profiling support in the Devine kernel.

## Components Implemented

### 1. Per-CPU Data Management (`src/cpu/percpu.rs`)

Tracks per-CPU state including:
- CPU IDs (logical ID and APIC/GIC ID)
- Kernel stack pointers
- Interrupt statistics (IRQ count, timer ticks)
- Context switch counters
- Profiling cycle counters

**Key Functions:**
- `init_cpu_manager()` - Initialize the global CPU manager
- `register_cpu()` - Register a CPU with the system
- `get_current_cpu_info()` - Get current CPU information
- `increment_irq_count()` - Increment interrupt counter for profiling
- `increment_timer_ticks()` - Track timer interrupts per CPU

### 2. Spinlock Implementation (`src/lib.rs`)

A lock-free spinlock implementation for SMP safety:
- Uses atomic compare-and-swap for fast acquisition
- Supports PAUSE instruction on x86_64 and YIELD on ARM64
- Provides RAII guard for automatic release
- Interrupt guard for disabling/enabling interrupts atomically

**Key Types:**
- `Spinlock<T>` - Generic spinlock for any data type
- `SpinlockGuard<T>` - RAII guard ensuring lock release
- `InterruptGuard` - Disables interrupts for duration of scope

### 3. CPU Enumeration

#### x86_64 (`src/x86_64/cpu.rs`)
- CPUID instruction parsing for CPU information
- MSR (Model-Specific Register) read/write
- APIC ID extraction
- Placeholder for ACPI MADT table parsing

#### ARM64 (`src/arch/arm64/cpu.rs`)
- MPIDR_EL1 register parsing
- PSCI (Power State Coordination Interface) support
- SCTLR_EL1 manipulation for MMU control
- Placeholder for FDT parsing

### 4. Inter-Processor Interrupts (IPI) (`src/arch/ipi.rs`)

Unified IPI interface across architectures:

```rust
pub enum IpiKind {
    Reschedule,
    Shutdown,
    Halt,
    Timer,
    Call,
}

pub fn send_ipi(cpu_id: u32, kind: IpiKind) -> bool;
pub fn broadcast_ipi(kind: IpiKind) -> u32;
```

### 5. Application Processor Boot

#### x86_64 (`src/x86_64/ap_boot.rs`)
- INIT and Startup IPI sequence
- Real mode to protected mode transition
- Long mode (64-bit) setup

#### ARM64 (`src/arm64/ap_boot.rs`)
- PSCI CPU_ON interface
- Optional FDT-based CPU discovery

### 6. Per-CPU Scheduler (`src/cpu/scheduler_smp.rs`)

Multi-CPU scheduler implementation:
- Per-CPU run queues (max 64 tasks per queue)
- Task enqueueing/dequeueing
- Load balancing with work stealing
- Periodic rebalancing

**Key Functions:**
- `enqueue_task()` - Add task to a CPU's run queue
- `dequeue_task()` - Remove task from run queue
- `load_balance()` - Steal tasks from busy CPUs
- `rebalance_all()` - Global load balancing

### 7. CPU Initialization (`src/cpu/init.rs`)

Brings together CPU enumeration and AP boot:

```rust
pub fn init();          // Initialize CPU subsystem
pub fn boot_aps();      // Bring up application processors
pub fn cpu_count();     // Get number of online CPUs
pub fn current_cpu();   // Get current CPU ID
```

### 8. Profiling Integration

The existing profiling infrastructure (`src/kernel/profiler.rs`) has been extended to support:
- Per-CPU cycle counters
- Per-CPU interrupt statistics
- Sampling hooks around scheduler, VFS, and syscall paths

**Profiling API:**
```rust
pub fn init();           // Initialize profiler
pub fn is_enabled();     // Check if profiling is active
pub fn enable/disable(); // Control profiling

// Macros for instrumentation:
profile_start!("label");
profile_end!("label");
profile_count!("counter");
```

## Architecture-Specific Details

### x86_64 SMP

1. **CPU Enumeration:**
   - Primary: CPUID leaf 0x0B (extended topology)
   - Fallback: CPUID leaf 0x04 (cache info)
   - Future: ACPI MADT table parsing

2. **APIC Operations:**
   - Local APIC base address from IA32_APIC_BASE MSR
   - IPI delivery via ICR (Interrupt Command Register)
   - INIT and Startup IPI sequences

3. **AP Boot Sequence:**
   ```
   1. Copy AP startup code to 0x8000 (low memory)
   2. Send INIT IPI to AP
   3. Wait 10ms
   4. Send Startup IPI with vector pointing to 0x8000
   5. AP transitions: Real Mode → Protected Mode → Long Mode
   6. Calls ap_startup_main(cpu_id)
   ```

### ARM64 SMP

1. **CPU Enumeration:**
   - MPIDR_EL1 register for current CPU ID
   - Device Tree or PSCI for CPU count
   - GIC redistributor for per-CPU interrupt handling

2. **PSCI Interface:**
   - PSCI_VERSION to detect PSCI support
   - PSCI_CPU_ON to bring CPUs online
   - PSCI_AFFINITY_INFO to query CPU status

3. **AP Boot Sequence:**
   ```
   1. Call PSCI_CPU_ON with target CPU and entry point
   2. AP begins execution at entry point (typically in 64-bit mode)
   3. Calls ap_startup_main(cpu_id)
   ```

## Usage

### Enabling SMP

In your kernel main initialization:

```rust
// Initialize the CPU subsystem
cpu::init::init();

// Bring up application processors
cpu::init::boot_aps();

// Query system
let cpu_count = cpu::init::cpu_count();
println!("System has {} CPUs", cpu_count);
```

### Using Spinlocks

```rust
use kernel::lib::spinlock::Spinlock;

// Create a protected resource
let counter = Spinlock::new(0u32);

// Acquire lock (blocks if already locked)
{
    let mut count = counter.lock();
    *count += 1;
}  // Lock automatically released when guard goes out of scope

// Try to acquire without blocking
if let Some(mut count) = counter.try_lock() {
    *count += 1;
}
```

### Using IPIs

```rust
use kernel::arch::ipi::{send_ipi, IpiKind};

// Send reschedule IPI to CPU 1
send_ipi(1, IpiKind::Reschedule);

// Broadcast halt to all CPUs
broadcast_ipi(IpiKind::Halt);
```

### Per-CPU Scheduling

```rust
use kernel::cpu::scheduler_smp;

let scheduler = scheduler_smp::get_scheduler().unwrap();

// Add task to CPU 0's run queue
let task = TaskRef { task_id: 1, priority: 50 };
scheduler.enqueue_task(0, task);

// Get next task from CPU 0
if let Some(next_task) = scheduler.dequeue_task(0) {
    // Execute task
}
```

## Performance Considerations

1. **Lock Contention:**
   - Spinlocks are appropriate for short critical sections
   - For long-held locks, consider reader-writer locks
   - Per-CPU data avoids contention

2. **Cache Locality:**
   - Per-CPU data structures keep hot data local
   - Task queues in L1 cache for best performance
   - Consider NUMA on multi-socket systems

3. **IPI Overhead:**
   - IPIs are expensive (1000+ cycles)
   - Use coalescing for multiple IPIs
   - Consider batching reschedule requests

## Testing

### Compile-Time Verification

```bash
cargo build --release
```

### Runtime Testing

1. **Single-CPU Boot:**
   ```bash
   make qemu-x86_64
   ```

2. **Multi-CPU Boot:**
   ```bash
   # Build QEMU command with -smp flag
   GDB=1 make qemu-x86_64  # Will show CPU enumeration
   ```

3. **Stress Testing:**
   ```bash
   # Spawn many threads (depends on test harness)
   make test-smp-stress
   ```

## Future Enhancements

1. **ACPI Support:**
   - Full ACPI MADT table parsing
   - CPU hotplug support
   - NUMA awareness

2. **Advanced Load Balancing:**
   - Work stealing scheduler
   - Cache-aware task placement
   - Energy-efficient scheduling

3. **Performance Monitoring:**
   - PMU (Performance Monitoring Unit) integration
   - Per-CPU performance counters
   - Tracing infrastructure

4. **Fault Tolerance:**
   - CPU offline handling
   - Graceful degradation
   - Recovery from AP boot failures

## Debugging

### Enable Verbose CPU Enumeration

Set environment variable before booting:
```bash
KERNEL_DEBUG=1 make qemu-x86_64
```

### GDB Debugging

```bash
# Terminal 1: Start QEMU with GDB stub
make qemu-debug-x86_64

# Terminal 2: Connect with GDB
gdb kernel.elf
(gdb) target remote localhost:1234
(gdb) break ap_startup_main
(gdb) continue
```

## References

- Intel SDM (Software Developer Manual) - Volume 3, Chapter 8 (Interrupt Handling)
- Intel ACPI Specification - Multiple APIC Description Table
- ARM Architecture Reference Manual - ARMv8-A (Chapter D7: PSCI)
- Linux Kernel - arch/x86/kernel/smpboot.c, arch/arm64/kernel/smp.c
