/* Device Driver Interface */

#include "../include/types.h"
#include "../include/console.h"

// Device types
#define DEVICE_CHAR    1
#define DEVICE_BLOCK   2
#define DEVICE_NETWORK 3
#define DEVICE_FS      4

// Forward declarations for external functions
extern void* malloc(u64 size);
extern void free(void* ptr);
extern i32 strcmp(const char* s1, const char* s2);
extern i32 strncmp(const char* s1, const char* s2, u64 n);
extern char* strncpy(char* dest, const char* src, u64 n);
extern u64 system_time;

// Forward declarations for driver init functions
void virtio_blk_init(void* pci_dev);
void virtio_net_init(void* pci_dev);
void virtio_mmio_init(void* dt_dev);
void uart16550_init(void);
void pl011_init(void* dt_dev);

// Device operations structure
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
    i32 (*probe)(void* device);
    i32 (*remove)(void* device);
} device_ops_t;

// Device structure
typedef struct device {
    char name[MAX_STRING_LEN];
    u32 major;
    u32 minor;
    u32 type;
    u64 size;
    void* data;
    device_ops_t* ops;
    struct device* next;
} device_t;

// Global device list
static device_t* devices = NULL;
static u32 next_major = 1;

// Register a device
i32 device_register(const char* name, u32 type, device_ops_t* ops, void* data) {
    device_t* dev = (device_t*)malloc(sizeof(device_t));
    if (!dev) {
        return ERR_NO_MEMORY;
    }
    
    strncpy(dev->name, name, MAX_STRING_LEN - 1);
    dev->name[MAX_STRING_LEN - 1] = '\0';
    dev->major = next_major++;
    dev->minor = 0;
    dev->type = type;
    dev->size = 0;
    dev->data = data;
    dev->ops = ops;
    dev->next = devices;
    
    devices = dev;
    
    console_print("Registered device: ");
    console_print(name);
    console_print(" (major=");
    console_print_dec(dev->major);
    console_print(")\n");
    
    return dev->major;
}

// Find device by name
device_t* device_find(const char* name) {
    device_t* dev = devices;
    while (dev && strcmp(dev->name, name) != 0) {
        dev = dev->next;
    }
    return dev;
}

// Find device by major number
device_t* device_find_by_major(u32 major) {
    device_t* dev = devices;
    while (dev && dev->major != major) {
        dev = dev->next;
    }
    return dev;
}

// Initialize device subsystem
void device_init(void) {
    console_print("Initializing device subsystem...\n");
    devices = NULL;
    
    // Initialize specific device types
    console_print("  Initializing character devices...\n");
    console_print("  Initializing block devices...\n");
    console_print("  Initializing network devices...\n");
    console_print("Device subsystem initialized\n");
}

// VFS device node creation stub
void vfs_create_device_node(const char* path, u64 mode, u32 major, u32 minor) {
    console_print("Creating device node: ");
    console_print(path);
    console_print(" (major=");
    console_print_dec(major);
    console_print(", minor=");
    console_print_dec(minor);
    console_print(", mode=0");
    console_print_hex(mode);
    console_print(")\n");
}

// Additional device utility functions
device_t* device_find_by_name(const char* name) {
    device_t* dev = devices;
    while (dev && strcmp(dev->name, name) != 0) {
        dev = dev->next;
    }
    return dev;
}

i32 device_unregister(const char* name) {
    device_t* dev = devices;
    device_t* prev = NULL;
    
    while (dev) {
        if (strcmp(dev->name, name) == 0) {
            if (prev) {
                prev->next = dev->next;
            } else {
                devices = dev->next;
            }
            
            console_print("Unregistered device: ");
            console_print(name);
            console_print("\n");
            
            free(dev);
            return 0;
        }
        prev = dev;
        dev = dev->next;
    }
    
    return ERR_NOT_FOUND;
}