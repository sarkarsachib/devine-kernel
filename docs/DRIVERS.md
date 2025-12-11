# Driver Development Guide

This document provides a comprehensive guide to the Devine kernel's driver architecture, device enumeration, and how to develop new drivers.

## Table of Contents

- [Architecture Overview](#architecture-overview)
- [Device Enumeration](#device-enumeration)
- [Driver Implementation](#driver-implementation)
- [Built-in Drivers](#built-in-drivers)
- [Device Integration](#device-integration)
- [Testing Drivers](#testing-drivers)
- [Troubleshooting](#troubleshooting)

## Architecture Overview

The Devine kernel uses a modular driver architecture with the following components:

```
┌─────────────────────────────────────────────────────────────┐
│                    Hardware Abstraction Layer                │
├─────────────────────────────────────────────────────────────┤
│  x86_64              │            ARM64                      │
│  ├─ PCI/PCIe Enum    │  ├─ Device Tree Parser               │
│  ├─ APIC/IRQ         │  ├─ GIC Interrupt Controller         │
│  └─ UART 16550       │  └─ PL011 UART                       │
└─────────────────────────────────────────────────────────────┘
                          ↓
┌─────────────────────────────────────────────────────────────┐
│                    Device Driver Layer                       │
├─────────────────────────────────────────────────────────────┤
│  ├─ Block Devices (VirtIO-blk)                              │
│  ├─ Network Devices (VirtIO-net)                            │
│  ├─ Character Devices (UART, TTY)                           │
│  └─ Device Discovery & Registration                         │
└─────────────────────────────────────────────────────────────┘
                          ↓
┌─────────────────────────────────────────────────────────────┐
│                 Virtual File System (VFS)                   │
├─────────────────────────────────────────────────────────────┤
│  ├─ /dev/  - Device nodes                                   │
│  ├─ /dev/vda - Block device                                 │
│  ├─ /dev/eth0 - Network device                              │
│  └─ /dev/tty* - Terminal devices                            │
└─────────────────────────────────────────────────────────────┘
```

## Device Enumeration

### PCI/PCIe Enumeration (x86_64)

The kernel scans the PCI bus at boot time to discover hardware devices.

**Location**: `src/drivers/pci.c`

**Process**:
1. Iterate through PCI buses (0-255)
2. For each device, probe the configuration space
3. Read vendor ID, device ID, class code
4. Extract BAR (Base Address Register) information
5. Register device via `device_register()` API

**Key Functions**:
- `pci_init()` - Main entry point
- `pci_scan_bus()` - Scan a single PCI bus
- `pci_probe_device()` - Probe a device and register it
- `pci_config_read/write()` - Low-level PCI config access

**Device Matching**:
```c
// Match by vendor and device ID
pci_device_t *dev = pci_find_device(0x1AF4, 0x1001);  // VirtIO block

// Match by class code
pci_device_t *dev = pci_find_class(0x01, 0x00);  // Mass storage
```

### Device Tree Enumeration (ARM64)

The kernel parses the device tree to discover hardware.

**Location**: `src/drivers/devicetree.c`

**Process**:
1. Locate device tree in memory (passed by bootloader)
2. Parse FDT (Flattened Device Tree) header
3. Traverse device tree nodes
4. Extract device properties
5. Register devices

**Key Functions**:
- `fdt_init()` - Initialize device tree parser
- `fdt_find_node()` - Find node by name
- `fdt_get_property()` - Extract property value
- `fdt_walk()` - Iterate device tree nodes

## Driver Implementation

### Basic Driver Structure

Each driver implements the `device_ops_t` interface:

```c
typedef struct {
    // Character device operations
    i32 (*read)(void* device, u64 offset, u64 size, void* buffer);
    i32 (*write)(void* device, u64 offset, u64 size, const void* buffer);
    i32 (*ioctl)(void* device, u32 cmd, void* arg);
    
    // Block device operations
    i32 (*read_block)(void* device, u64 block_num, void* buffer);
    i32 (*write_block)(void* device, u64 block_num, const void* buffer);
    
    // Device management
    i32 (*open)(void* device, u32 flags);
    i32 (*close)(void* device);
    i32 (*probe)(void* device);
    i32 (*remove)(void* device);
} device_ops_t;
```

### Step-by-Step Driver Development

#### 1. Create Driver File

```bash
mkdir -p src/drivers/myclass
touch src/drivers/myclass/mydriver.c
```

#### 2. Implement Device Operations

```c
#include "include/types.h"

typedef struct {
    u32 registers_base;
    u32 major_number;
    // ... device state
} mydriver_device_t;

i32 mydriver_read(void* device, u64 offset, u64 size, void* buffer) {
    mydriver_device_t* dev = (mydriver_device_t*)device;
    
    // Read operation implementation
    // Access hardware at dev->registers_base
    
    return size;
}

i32 mydriver_write(void* device, u64 offset, u64 size, const void* buffer) {
    // Write operation implementation
    return size;
}

static device_ops_t mydriver_ops = {
    .read = mydriver_read,
    .write = mydriver_write,
    .ioctl = mydriver_ioctl,
    // ... other operations
};
```

#### 3. Register Device

```c
void mydriver_init(void) {
    mydriver_device_t* dev = malloc(sizeof(mydriver_device_t));
    
    // Initialize device state
    dev->registers_base = MMIO_BASE;
    
    // Register with kernel
    i32 major = device_register("mydevice", DEVICE_CHAR, &mydriver_ops, dev);
    
    if (major > 0) {
        console_print("MyDriver initialized (major=");
        console_print_dec(major);
        console_print(")\n");
    }
}
```

#### 4. Create VFS Device Node

```c
void mydriver_create_node(u32 major, u32 minor) {
    vfs_create_device_node("/dev/mydev", 0666, major, minor);
}
```

#### 5. Hook Into Hardware Enumeration

**For PCI devices (x86_64)**:

```c
void device_register_pci(pci_device_t* pci_dev) {
    if (pci_dev->vendor_id == 0xMYVENDOR && 
        pci_dev->device_id == 0xMYDEVICE) {
        mydriver_init_pci(pci_dev);
    }
}
```

**For Device Tree devices (ARM64)**:

```c
void dt_probe_node(const char* node_name) {
    if (strcmp(node_name, "my-device") == 0) {
        mydriver_init_dt();
    }
}
```

## Built-in Drivers

### Block Drivers

#### VirtIO Block (VirtIO-blk)

**Files**: 
- `src/drivers/block/virtio_blk.c`

**Features**:
- DMA-capable request handling
- Read/write operations via virtio queue
- Integration with block cache
- Device node: `/dev/vda`, `/dev/vdb`, etc.

**Usage**:
```bash
# Read a block
cat /dev/vda | head -c 4096

# Write to block device
dd if=file.img of=/dev/vda bs=4096
```

### Network Drivers

#### VirtIO Network (VirtIO-net)

**Files**:
- `src/drivers/net/virtio_net.c`

**Features**:
- RX/TX ring buffers
- Interrupt-driven packet flow
- Loopback test capability
- Device node: `/dev/eth0`, `/dev/eth1`, etc.

**Usage**:
```bash
# Send a packet
echo "test data" > /dev/eth0

# Receive packets
cat /dev/eth0 &
# Send data to trigger receive
```

### Terminal/Console Drivers

#### 16550 UART (x86_64)

**Files**:
- `src/drivers/tty/uart16550.c`

**Features**:
- COM port initialization
- Buffered I/O
- Interrupt-driven character I/O
- Device nodes: `/dev/ttyS0`, `/dev/tty`

#### PL011 UART (ARM64)

**Files**:
- `src/drivers/tty/pl011.c`

**Features**:
- MMIO register access
- Buffered I/O
- Device nodes: `/dev/ttyAMA0`, `/dev/tty`

## Device Integration

### Device Registration Flow

```
1. Hardware Discovery
   ├─ PCI enumeration (x86_64)
   └─ Device Tree parsing (ARM64)
           ↓
2. Device Probing
   └─ Match against known drivers
           ↓
3. Driver Initialization
   └─ Allocate and initialize device state
           ↓
4. VFS Integration
   └─ Create /dev device nodes
           ↓
5. Ready for Use
   └─ Applications access via /dev/*
```

### Adding Device Match Tables

Create match tables to discover devices:

```c
// PCI match table
const struct {
    u16 vendor_id;
    u16 device_id;
} pci_match_table[] = {
    { 0x1AF4, 0x1001 },  // VirtIO block
    { 0x1AF4, 0x1000 },  // VirtIO network
    { 0, 0 }  // End marker
};

// Device Tree match table
const char *dt_match_table[] = {
    "virtio,device",
    "ns16550",
    NULL
};
```

## Testing Drivers

### Stress Tests

The kernel provides stress test scripts for driver validation:

```bash
# Run all driver stress tests
make test-drivers

# Run specific test
./tests/run_driver_stress.sh block x86_64
./tests/run_driver_stress.sh network x86_64
./tests/run_driver_stress.sh tty x86_64
```

### Manual Testing

#### 1. Boot Kernel

```bash
make qemu-x86_64
```

#### 2. Verify Device Enumeration

Watch for messages like:
```
Scanning PCI bus...
  PCI 0:0:0 - Vendor: 0x8086 Device: 0x1237 Class: 0x6
  PCI 0:1:0 - Vendor: 0x1AF4 Device: 0x1001 Class: 0x1
    Found VirtIO block device
```

#### 3. Check Device Nodes

```bash
# List devices
ls /dev/vda /dev/eth0 /dev/ttyS0

# Check device types
cat /dev/vda | xxd | head

# Test network loopback
echo "hello" > /dev/eth0
cat /dev/eth0
```

### Debugging with GDB

```bash
# Terminal 1: Start kernel with GDB support
make qemu-debug-x86_64

# Terminal 2: Connect GDB
gdb build/x86_64/kernel.elf
(gdb) target remote :1234
(gdb) break device_init
(gdb) continue

# Inspect device list
(gdb) print devices
(gdb) print devices->next
```

## Performance Optimization

### Profiling Driver Code

Enable profiling for performance analysis:

```bash
export PROFILER_ENABLED=1
make build-x86_64
```

Add tracepoints to driver code:

```c
#include "profiler.h"

void mydriver_read(void* device, u64 offset, u64 size, void* buffer) {
    PROFILE_START("mydriver_read");
    
    // Read operation
    
    PROFILE_END("mydriver_read");
}
```

### Interrupt Latency

Measure interrupt handling latency:

```c
PROFILE_START("irq_handler");
// Handle interrupt
PROFILE_END("irq_handler");
```

## Common Issues and Solutions

### Device Not Detected

**Symptom**: Device not appearing in enumeration output

**Solutions**:
1. Verify QEMU is configured with the device
2. Check PCI/Device Tree match logic
3. Enable debug logging in enumeration code
4. Verify BAR/memory addresses are correct

### Device Node Not Created

**Symptom**: `/dev/mydevice` not present after boot

**Solutions**:
1. Verify `vfs_create_device_node()` is called
2. Check VFS is initialized before device creation
3. Verify major/minor numbers are valid
4. Check device path is correct

### Interrupt Not Firing

**Symptom**: Device interrupts never trigger

**Solutions**:
1. Verify interrupt line is configured in driver
2. Check IRQ handler is registered correctly
3. Verify interrupt controller is initialized
4. Enable interrupt debugging

### DMA Issues

**Symptom**: DMA transfers fail or corrupt memory

**Solutions**:
1. Verify DMA buffer alignment (page-aligned)
2. Check physical address translation
3. Verify device can access buffer address
4. Check for buffer overflow conditions

## Best Practices

1. **Error Handling**: Always check return values from kernel APIs
2. **Memory Management**: Properly allocate and free device structures
3. **Synchronization**: Use locks for shared resources
4. **Documentation**: Document device operations and requirements
5. **Testing**: Write stress tests for your driver
6. **Logging**: Add meaningful debug messages
7. **Resource Cleanup**: Properly release resources on device removal

## Advanced Topics

### DMA and Memory Mapping

For high-performance drivers requiring DMA:

```c
// Allocate DMA-capable buffer
void* dma_buffer = allocate_dma_buffer(size);
u64 dma_phys_addr = virt_to_phys(dma_buffer);

// Configure device to use DMA
device_setup_dma(device, dma_phys_addr, size);
```

### Interrupt Sharing

Multiple devices can share an interrupt:

```c
// Register multiple handlers on same IRQ
register_irq_handler(IRQ_NUM, driver1_handler);
register_irq_handler(IRQ_NUM, driver2_handler);
```

### Hot Plugging

For dynamic device addition/removal:

```c
void device_hotplug(pci_device_t* pci_dev) {
    // Detect and initialize new device
}

void device_unplug(u32 major) {
    // Clean up and remove device
}
```

## References

- PCI Specification: https://pcisig.com/
- Device Tree Specification: https://www.devicetree.org/
- VirtIO Specification: https://docs.oasis-open.org/virtio/
- QEMU Device Documentation: https://www.qemu.org/documentation/

## Support

For questions or issues:
1. Check the debugging guide in `docs/DEBUGGING.md`
2. Review existing driver implementations
3. Run stress tests to validate your driver
4. Use GDB to debug driver issues
