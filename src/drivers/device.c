/* Device Driver Interface */

#include "../../include/types.h"

// Device types
#define DEVICE_CHAR    1
#define DEVICE_BLOCK   2
#define DEVICE_NETWORK 3
#define DEVICE_FS      4

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