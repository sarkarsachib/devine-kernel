# Kernel Memory and Process Management

A foundational operating system kernel with comprehensive memory management and process/thread scheduling subsystems.

## Features

### Memory Management

#### Physical Memory
- **Frame Allocator**: Bitmap-based physical frame allocator for efficient memory allocation
- **NUMA Awareness**: NUMA topology support with distance matrix and local allocation
- Configurable memory regions with multi-region support

#### Virtual Memory
- **x86_64 Recursive Paging**: Full 4-level page table implementation with recursive mapping
- **ARM LPAE**: Large Physical Address Extension support for ARM architectures
- Page table mapping/unmapping with proper TLB invalidation
- Support for 4KB, 2MB, and 1GB pages

#### Heap Allocator
- Linked-list heap allocator with first-fit algorithm
- Global allocator integration with Rust's `alloc` crate
- NUMA-aware allocation hooks for optimal memory placement

### Process Management

#### Process & Thread Model
- **Process**: Address space isolation with page tables
- **Thread**: Execution context with kernel and user stacks
- Multi-threaded process support
- Parent-child process relationships

#### Context Switching
- Full register context save/restore
- Separate implementations for x86_64 and ARM architectures
- User and kernel mode contexts

#### Scheduler
- **Preemptive**: Timer-driven scheduling with configurable time slices
- **Priority-aware**: 5 priority levels (Idle, Low, Normal, High, Realtime)
- Multi-level feedback queues for efficient scheduling
- Round-robin within priority levels
- CPU time tracking per thread

### System Calls

Exposed system calls for userspace interaction:
- `SYS_EXIT`: Process termination
- `SYS_FORK`: Process creation via fork
- `SYS_EXEC`: Program execution
- `SYS_WAIT`: Process synchronization
- `SYS_GETPID`: Get process ID
- `SYS_MMAP`: Memory mapping
- `SYS_MUNMAP`: Memory unmapping
- `SYS_BRK`: Heap management
- `SYS_CLONE`: Thread creation

### Architecture Support

#### x86_64
- Recursive page table mapping at index 511
- IDT (Interrupt Descriptor Table) setup
- PIC (Programmable Interrupt Controller) initialization
- Timer interrupt handling (100 Hz)

#### ARM (Planned)
- LPAE (Large Physical Address Extension)
- 4-level page tables (L0-L3)
- GIC (Generic Interrupt Controller) support

## Project Structure

```
kernel/
├── src/
│   ├── lib.rs              # Main kernel library
│   ├── memory/
│   │   ├── mod.rs          # Memory types and constants
│   │   ├── frame_allocator.rs  # Physical frame allocator
│   │   ├── heap.rs         # Heap allocator
│   │   ├── numa.rs         # NUMA topology support
│   │   └── paging/
│   │       ├── mod.rs      # Paging abstractions
│   │       ├── x86_64_paging.rs  # x86_64 implementation
│   │       └── arm_lpae.rs # ARM LPAE implementation
│   ├── process/
│   │   ├── mod.rs          # Process types and table
│   │   ├── thread.rs       # Thread management
│   │   ├── context.rs      # Context switching
│   │   └── scheduler.rs    # Preemptive scheduler
│   ├── arch/
│   │   ├── mod.rs          # Architecture abstraction
│   │   ├── x86_64.rs       # x86_64 support
│   │   └── interrupts.rs   # Interrupt handling
│   └── syscall/
│       └── mod.rs          # System call interface
└── tests/
    ├── integration_test.rs # Integration tests
    └── smoke_test.rs       # QEMU smoke tests
```

## Building

```bash
cargo build --release
```

## Testing

### Unit Tests
```bash
cargo test --lib
```

### Smoke Tests
```bash
cargo test --test smoke_test
```

The smoke tests verify:
- Frame allocation and deallocation
- Page table operations
- Process and thread creation
- Priority-based scheduling
- Multi-threaded processes

## Usage Example

```rust
use kernel::memory::{PhysAddr, MemoryRegion, frame_allocator};
use kernel::process::{create_process, create_thread, Priority};
use kernel::memory::VirtAddr;

// Initialize memory
let regions = [MemoryRegion::new(PhysAddr::new(0x100000), 0x1000000)];
frame_allocator::init_frame_allocator(&regions);

// Create a process
let frame = frame_allocator::allocate_frame().unwrap();
let pid = create_process("my_process".into(), frame).unwrap();

// Create a thread
let stack = frame_allocator::allocate_frame().unwrap();
let tid = create_thread(
    pid,
    Priority::Normal,
    VirtAddr::new(0x1000),  // Entry point
    VirtAddr::new(stack.start_address.0),
    None,
).unwrap();

// Add to scheduler
kernel::process::scheduler::add_thread(tid);
```

## Design Decisions

### Memory Management
- **Bitmap allocator**: Provides O(n) allocation with good cache locality
- **Recursive paging**: Eliminates need for separate kernel mappings of page tables
- **NUMA hooks**: Allows for future optimization of memory placement

### Process Management
- **Separate process/thread abstractions**: Clean separation of concerns
- **Priority-based scheduling**: Ensures high-priority tasks get CPU time
- **Preemptive**: Prevents CPU hogging by misbehaving tasks
- **Time slices based on priority**: Higher priority = longer time slices

## Future Enhancements

- Copy-on-write for fork()
- Demand paging
- Memory-mapped files
- Thread-local storage
- SMP (multi-processor) support
- Real-time scheduling guarantees
- Power management integration

## License

CC0 1.0 Universal
