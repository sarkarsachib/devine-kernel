# Kernel Memory and Process Management - Usage Demo

This document demonstrates how to use the memory management and process/thread management subsystems.

## Memory Management

### Physical Frame Allocation

```rust
use kernel::memory::{PhysAddr, MemoryRegion, frame_allocator};

// Initialize the frame allocator with available memory regions
let regions = [
    MemoryRegion::new(PhysAddr::new(0x100000), 0x1000000),  // 16MB region
];
frame_allocator::init_frame_allocator(&regions);

// Allocate physical frames
let frame1 = frame_allocator::allocate_frame().unwrap();
let frame2 = frame_allocator::allocate_frame().unwrap();

// Deallocate when done
frame_allocator::deallocate_frame(frame1);
```

### Virtual Memory (x86_64)

```rust
use kernel::memory::paging::{X86_64PageTable, Page, PageFlags, PageTableMapper};
use kernel::memory::VirtAddr;

// Create or get the active page table
let mut page_table = X86_64PageTable::active();

// Map a virtual page to a physical frame
let page = Page::containing_address(VirtAddr::new(0x1000));
let frame = frame_allocator::allocate_frame().unwrap();
let flags = PageFlags::PRESENT | PageFlags::WRITABLE;

page_table.map_to(page, frame, flags).unwrap();

// Translate virtual to physical
let phys_addr = page_table.translate(VirtAddr::new(0x1050));

// Unmap when done
page_table.unmap(page).unwrap();
```

### Virtual Memory (ARM LPAE)

```rust
use kernel::memory::paging::{ArmLpaePageTable, Page, PageFlags};

let l0_frame = frame_allocator::allocate_frame().unwrap();
let mut page_table = ArmLpaePageTable::new(l0_frame);

let page = Page::containing_address(VirtAddr::new(0x2000));
let frame = frame_allocator::allocate_frame().unwrap();
let flags = PageFlags::PRESENT | PageFlags::WRITABLE;

page_table.map_to_lpae(page, frame, flags).unwrap();
```

### Heap Allocation

```rust
use kernel::memory::heap;
use alloc::vec::Vec;

// Initialize the heap
heap::init_heap();

// Now you can use standard Rust collections
let mut v = Vec::new();
v.push(1);
v.push(2);
v.push(3);
```

### NUMA Awareness

```rust
use kernel::memory::numa::{NumaNode, NumaTopology, NUMA_TOPOLOGY};

// Initialize NUMA topology
let mut topology = NUMA_TOPOLOGY.lock();
let node0 = NumaNode { id: 0 };
let node1 = NumaNode { id: 1 };

topology.add_node(node0);
topology.add_node(node1);

// Set distance matrix (lower is closer)
topology.set_distance(node0, node0, 10);  // Same node
topology.set_distance(node0, node1, 20);  // Remote node
topology.set_distance(node1, node0, 20);
topology.set_distance(node1, node1, 10);

// Allocate memory local to current NUMA node
use kernel::memory::numa::allocate_numa_local;
let local_memory = allocate_numa_local(4096);
```

## Process Management

### Creating a Process

```rust
use kernel::process::{create_process, Priority};
use alloc::string::String;

// Allocate a page table frame for the new process
let page_table_frame = frame_allocator::allocate_frame().unwrap();

// Create a new process
let pid = create_process(
    String::from("my_process"),
    page_table_frame
).unwrap();
```

### Creating Threads

```rust
use kernel::process::{create_thread, Priority};
use kernel::memory::VirtAddr;

// Allocate stack for the thread
let kernel_stack_frame = frame_allocator::allocate_frame().unwrap();
let kernel_stack = VirtAddr::new(kernel_stack_frame.start_address.0 + 4096);

// For user-space threads, also allocate a user stack
let user_stack_frame = frame_allocator::allocate_frame().unwrap();
let user_stack = VirtAddr::new(user_stack_frame.start_address.0 + 4096);

// Create a thread
let tid = create_thread(
    pid,                                    // Parent process
    Priority::Normal,                       // Thread priority
    VirtAddr::new(0x400000),               // Entry point
    kernel_stack,                          // Kernel stack
    Some(user_stack),                      // User stack (None for kernel threads)
).unwrap();
```

### Scheduler Operations

```rust
use kernel::process::scheduler;

// Add thread to scheduler
scheduler::add_thread(tid);

// Manually trigger scheduling (normally done by timer interrupt)
if let Some(next_tid) = scheduler::schedule() {
    println!("Scheduled thread: {:?}", next_tid);
}

// Current thread yields CPU
scheduler::yield_cpu();

// Block current thread (e.g., waiting for I/O)
scheduler::block_current_thread();

// Unblock a thread
scheduler::unblock_thread(tid);

// Remove thread from scheduler (when terminating)
scheduler::remove_thread(tid);
```

### Priority-Based Scheduling

```rust
use kernel::process::Priority;

// Create threads with different priorities
let high_priority_tid = create_thread(
    pid, Priority::High, entry_point, stack, None
).unwrap();

let normal_priority_tid = create_thread(
    pid, Priority::Normal, entry_point, stack, None
).unwrap();

let low_priority_tid = create_thread(
    pid, Priority::Low, entry_point, stack, None
).unwrap();

// Add all to scheduler
scheduler::add_thread(high_priority_tid);
scheduler::add_thread(normal_priority_tid);
scheduler::add_thread(low_priority_tid);

// High priority thread will be scheduled first
let next = scheduler::schedule();
assert_eq!(next, Some(high_priority_tid));
```

### Timer-Driven Preemption

```rust
use kernel::arch::interrupts;

// Initialize timer interrupts (100 Hz)
interrupts::init_timer();

// Timer interrupt handler calls:
// 1. scheduler::tick() - Updates time slices
// 2. scheduler::schedule() - Picks next thread if time slice expired
```

## System Calls

### Process Creation (Fork)

```rust
use kernel::syscall::{handle_syscall, SYS_FORK};

// Fork current process
let child_pid = handle_syscall(SYS_FORK, 0, 0, 0, 0, 0, 0).unwrap();
```

### Thread Creation (Clone)

```rust
use kernel::syscall::{handle_syscall, SYS_CLONE};

let flags = 0;
let stack = 0x7fffffff0000;
let tid = handle_syscall(SYS_CLONE, flags, stack, 0, 0, 0, 0).unwrap();
```

### Memory Mapping

```rust
use kernel::syscall::{handle_syscall, SYS_MMAP, SYS_MUNMAP};

// Map memory
let addr = 0x10000000;
let length = 4096;
let prot = 0x3;  // PROT_READ | PROT_WRITE
let flags = 0x1; // MAP_SHARED
let mapped_addr = handle_syscall(SYS_MMAP, addr, length, prot, flags, 0, 0).unwrap();

// Unmap memory
handle_syscall(SYS_MUNMAP, mapped_addr, length, 0, 0, 0, 0).unwrap();
```

### Get Process ID

```rust
use kernel::syscall::{handle_syscall, SYS_GETPID};

let pid = handle_syscall(SYS_GETPID, 0, 0, 0, 0, 0, 0).unwrap();
```

## Complete Example: Multi-Process System

```rust
// Initialize memory management
let regions = [MemoryRegion::new(PhysAddr::new(0x100000), 0x10000000)];
frame_allocator::init_frame_allocator(&regions);
heap::init_heap();

// Initialize scheduler
scheduler::init_scheduler();

// Create first process
let p1_frame = frame_allocator::allocate_frame().unwrap();
let pid1 = create_process("process_1".into(), p1_frame).unwrap();

// Create thread for process 1
let stack1 = frame_allocator::allocate_frame().unwrap();
let tid1 = create_thread(
    pid1,
    Priority::High,
    VirtAddr::new(0x400000),
    VirtAddr::new(stack1.start_address.0 + 4096),
    None
).unwrap();
scheduler::add_thread(tid1);

// Create second process
let p2_frame = frame_allocator::allocate_frame().unwrap();
let pid2 = create_process("process_2".into(), p2_frame).unwrap();

// Create thread for process 2
let stack2 = frame_allocator::allocate_frame().unwrap();
let tid2 = create_thread(
    pid2,
    Priority::Normal,
    VirtAddr::new(0x500000),
    VirtAddr::new(stack2.start_address.0 + 4096),
    None
).unwrap();
scheduler::add_thread(tid2);

// Initialize timer for preemptive scheduling
interrupts::init_timer();

// Start scheduling (high priority thread runs first)
let current = scheduler::schedule().unwrap();
assert_eq!(current, tid1);
```

## Testing

Run the comprehensive test suite:

```bash
# Run all unit tests
cargo test --lib

# Check compilation
cargo check

# Build release version
cargo build --release
```

The test suite includes:
- Frame allocation and deallocation
- Page table operations (x86_64 and ARM)
- NUMA topology management
- Process and thread creation
- Priority-based scheduling
- Context switching structures
- System call interfaces
