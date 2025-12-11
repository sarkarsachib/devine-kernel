/* Block Device Wrapper for Block Cache */

#include "../include/types.h"
#include "../include/block_cache.h"
#include "../include/console.h"

extern void* malloc(u64 size);
extern void free(void* ptr);

// Forward declaration of device_ops_t from device.c
typedef struct device_ops {
    i32 (*read_block)(void* device, u64 block_num, void* buffer);
    i32 (*write_block)(void* device, u64 block_num, const void* buffer);
    i32 (*ioctl_block)(void* device, u32 cmd, void* arg);
    i32 (*open)(void* device, u32 flags);
    i32 (*close)(void* device);
} device_ops_t;

typedef struct device {
    char name[256];
    u32 major;
    u32 minor;
    u32 type;
    u64 size;
    void* data;
    device_ops_t* ops;
    struct device* next;
} device_t;

// Wrapper for device operations
static i32 block_device_wrapper_read(void* device, u64 block_num, void* buffer) {
    device_t* dev = (device_t*)device;
    if (!dev || !dev->ops || !dev->ops->read_block) {
        return ERR_INVALID;
    }
    return dev->ops->read_block(dev->data, block_num, buffer);
}

static i32 block_device_wrapper_write(void* device, u64 block_num, const void* buffer) {
    device_t* dev = (device_t*)device;
    if (!dev || !dev->ops || !dev->ops->write_block) {
        return ERR_INVALID;
    }
    return dev->ops->write_block(dev->data, block_num, buffer);
}

static i32 block_device_wrapper_get_block_size(void* device) {
    device_t* dev = (device_t*)device;
    if (!dev || !dev->ops || !dev->ops->ioctl_block) {
        return 512;
    }
    
    u64 block_size = 512;
    dev->ops->ioctl_block(dev->data, 1, &block_size);
    return block_size;
}

static i32 block_device_wrapper_get_num_blocks(void* device) {
    device_t* dev = (device_t*)device;
    if (!dev || !dev->ops || !dev->ops->ioctl_block) {
        return 0;
    }
    
    u64 size = 0;
    dev->ops->ioctl_block(dev->data, 0, &size);
    
    u64 block_size = block_device_wrapper_get_block_size(device);
    return size / block_size;
}

static block_device_ops_t wrapper_ops = {
    .read_block = block_device_wrapper_read,
    .write_block = block_device_wrapper_write,
    .get_block_size = block_device_wrapper_get_block_size,
    .get_num_blocks = block_device_wrapper_get_num_blocks,
};

// Create block device wrapper
block_device_t* block_device_create(device_t* device) {
    if (!device) {
        return NULL;
    }
    
    block_device_t* bd = (block_device_t*)malloc(sizeof(block_device_t));
    if (!bd) {
        return NULL;
    }
    
    bd->device_data = device;
    bd->ops = &wrapper_ops;
    
    return bd;
}

// Destroy block device wrapper
void block_device_destroy(block_device_t* bd) {
    if (bd) {
        free(bd);
    }
}
