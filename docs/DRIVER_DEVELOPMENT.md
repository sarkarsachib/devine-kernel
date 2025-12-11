# Driver Development Guide

This guide explains how to develop and integrate device drivers in the Devine kernel.

## Overview

The Devine kernel provides a comprehensive driver framework with:

- Hardware enumeration (PCI for x86_64, Device Tree for ARM64)
- Unified device registration API
- VFS/devfs integration for userspace access
- Profiling and debugging support

## Architecture

### Device Enumeration

#### x86_64 - PCI Bus
```c
// Automatically scans PCI bus during boot
void pci_init(void);

// Find devices by vendor/device ID
pci_device_t* pci_find_device(u16 vendor_id, u16 device_id);

// Find devices by class
pci_device_t* pci_find_class(u8 class_code, u8 subclass);
```

#### ARM64 - Device Tree
```c
// Parse device tree during boot
void dt_init(void* dtb_addr);

// Find devices by compatible string
dt_device_t* dt_find_device(const char* compatible);
```

### Device Registration

All drivers register with the device subsystem:

```c
typedef struct device_ops {
    // Character device operations
    i32 (*read)(void* device, u64 offset, u64 size, void* buffer);
    i32 (*write)(void* device, u64 offset, u64 size, const void* buffer);
    i32 (*ioctl)(void* device, u32 cmd, void* arg);
    
    // Block device operations
    i32 (*read_block)(void* device, u64 block_num, void* buffer);
    i32 (*write_block)(void* device, u64 block_num, const void* buffer);
    i32 (*ioctl_block)(void* device, u32 cmd, void* arg);
    
    // Device management
    i32 (*open)(void* device, u32 flags);
    i32 (*close)(void* device);
} device_ops_t;

// Register a device
i32 device_register(const char* name, u32 type, device_ops_t* ops, void* data);
```

Device types:
- `DEVICE_CHAR` - Character devices (serial, console, etc.)
- `DEVICE_BLOCK` - Block devices (disk, flash, etc.)
- `DEVICE_NETWORK` - Network devices

### VFS Integration

Create device nodes in `/dev`:

```c
// Create a device node
void vfs_create_device_node(const char* path, u64 mode, u32 major, u32 minor);

// Examples:
vfs_create_device_node("/dev/vda", S_IFBLK | 0660, major, 0);
vfs_create_device_node("/dev/tty", S_IFCHR | 0660, major, 0);
vfs_create_device_node("/dev/eth0", S_IFCHR | 0660, major, 0);
```

## Implemented Drivers

### Block Drivers

#### VirtIO Block (`virtio_blk.c`)

VirtIO block device driver for disk access.

```c
void virtio_blk_init(void* pci_dev);
```

Features:
- DMA-capable block I/O
- 512-byte sector size
- Read/write operations
- Registered as `/dev/vda`

### Network Drivers

#### VirtIO Network (`virtio_net.c`)

VirtIO network device driver.

```c
void virtio_net_init(void* pci_dev);
```

Features:
- RX/TX ring buffers
- Interrupt-driven packet flow
- MAC address configuration
- Registered as `/dev/eth0`

### Console/TTY Drivers

#### 16550 UART (`uart16550.c`) - x86_64

Standard PC serial port driver.

```c
void uart16550_init(void);
```

Features:
- COM1 (0x3F8) support
- Buffered I/O
- Interrupt support
- Registered as `/dev/ttyS0` and `/dev/tty`

#### PL011 UART (`pl011.c`) - ARM64

ARM PrimeCell UART driver.

```c
void pl011_init(void* dt_dev);
```

Features:
- MMIO-based access
- Buffered I/O
- FIFO support
- Registered as `/dev/ttyAMA0` and `/dev/tty`

## Writing a New Driver

### Step 1: Create Driver File

Create a new file in the appropriate directory:
- Block: `src/drivers/block/mydriver.c`
- Network: `src/drivers/net/mydriver.c`
- Character: `src/drivers/char/mydriver.c`

### Step 2: Implement Device Operations

```c
#include "../../include/types.h"
#include "../../include/console.h"

typedef struct my_device {
    u64 base_addr;
    // Device-specific state
} my_device_t;

static i32 my_read(void* device, u64 offset, u64 size, void* buffer) {
    my_device_t* dev = (my_device_t*)device;
    // Implement read operation
    return size;
}

static i32 my_write(void* device, u64 offset, u64 size, const void* buffer) {
    my_device_t* dev = (my_device_t*)device;
    // Implement write operation
    return size;
}

static device_ops_t my_ops = {
    .read = my_read,
    .write = my_write,
    .ioctl = NULL,
    // ... other operations
};
```

### Step 3: Implement Initialization

```c
void my_driver_init(void* hw_device) {
    console_print("Initializing my driver...\n");
    
    my_device_t* dev = (my_device_t*)malloc(sizeof(my_device_t));
    if (!dev) {
        console_print("  Failed to allocate device\n");
        return;
    }
    
    // Initialize device state
    dev->base_addr = /* get from hw_device */;
    
    // Register with device subsystem
    i32 major = device_register("mydevice", DEVICE_CHAR, &my_ops, dev);
    if (major < 0) {
        console_print("  Failed to register device\n");
        free(dev);
        return;
    }
    
    // Create VFS node
    vfs_create_device_node("/dev/mydevice", S_IFCHR | 0660, major, 0);
    
    console_print("  My driver registered successfully\n");
}
```

### Step 4: Hook into Enumeration

For PCI devices (x86_64), add to `pci.c`:

```c
void device_register_pci(pci_device_t* pci_dev) {
    // ... existing code ...
    
    if (pci_dev->vendor_id == MY_VENDOR_ID && 
        pci_dev->device_id == MY_DEVICE_ID) {
        console_print("    Found my device\n");
        my_driver_init(pci_dev);
    }
}
```

For Device Tree (ARM64), add to `devicetree.c`:

```c
void device_register_dt(dt_device_t* dt_dev) {
    // ... existing code ...
    
    if (strncmp(dt_dev->compatible, "myvendor,mydevice", 17) == 0) {
        console_print("    Found my device\n");
        my_driver_init(dt_dev);
    }
}
```

## Profiling Support

Add profiling to your driver:

```c
#include "../include/profiler.h"

static i32 my_read(void* device, u64 offset, u64 size, void* buffer) {
    PROFILE_START("my_driver_read");
    
    // ... driver code ...
    PROFILE_COUNT("my_driver_reads");
    
    PROFILE_END("my_driver_read");
    return size;
}
```

Build with profiling:
```bash
export PROFILER_ENABLED=1
make build-x86_64
```

## Testing

### Unit Testing

Create tests in `tests/`:

```rust
// tests/my_driver_test.rs
#[test]
fn test_my_driver_read() {
    // Test code
}
```

### Stress Testing

Add to driver stress test script:

```bash
# tests/run_driver_stress.sh

test_my_driver() {
    echo "Testing my driver..."
    # Test code
}
```

Run tests:
```bash
make test-drivers
```

### Debugging

Use GDB for debugging:

```bash
# Terminal 1
make qemu-debug-x86_64

# Terminal 2
gdb build/x86_64/kernel.elf
(gdb) target remote :1234
(gdb) break my_driver_init
(gdb) continue
```

## Best Practices

1. **Error Handling**: Always check return values and handle errors gracefully
2. **Memory Management**: Free allocated memory on errors
3. **Logging**: Use `console_print` for status messages
4. **Profiling**: Add profiling points for performance-critical paths
5. **Testing**: Write comprehensive tests for your driver
6. **Documentation**: Document device-specific behavior and quirks

## Common Pitfalls

1. **Forgetting to register device nodes**: Always call `vfs_create_device_node`
2. **Not checking malloc failures**: Always check if malloc returns NULL
3. **Missing interrupt handling**: Set up interrupts if your device uses them
4. **DMA buffer alignment**: Ensure DMA buffers are properly aligned
5. **Endianness**: Be aware of endianness when accessing hardware registers

## Resources

- [VirtIO Specification](https://docs.oasis-open.org/virtio/virtio/v1.1/virtio-v1.1.html)
- [PCI Local Bus Specification](https://pcisig.com/specifications)
- [Device Tree Specification](https://www.devicetree.org/)
- [Linux Device Drivers](https://lwn.net/Kernel/LDD3/)

## Support

For questions or issues:
1. Check the debugging guide: `docs/DEBUGGING.md`
2. Review existing drivers for examples
3. Use the serial debugger for runtime inspection
