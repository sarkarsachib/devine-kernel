/* VirtIO Block Driver */

#include "../../include/types.h"
#include "../../include/console.h"

#define VIRTIO_BLK_F_RO        (1 << 5)
#define VIRTIO_BLK_F_SCSI      (1 << 7)
#define VIRTIO_BLK_F_FLUSH     (1 << 9)
#define VIRTIO_BLK_F_TOPOLOGY  (1 << 10)
#define VIRTIO_BLK_F_BLK_SIZE  (1 << 6)

#define VIRTIO_BLK_T_IN        0
#define VIRTIO_BLK_T_OUT       1
#define VIRTIO_BLK_T_FLUSH     4

#define VIRTIO_BLK_S_OK        0
#define VIRTIO_BLK_S_IOERR     1
#define VIRTIO_BLK_S_UNSUPP    2

#define VIRTIO_STATUS_ACKNOWLEDGE  1
#define VIRTIO_STATUS_DRIVER       2
#define VIRTIO_STATUS_DRIVER_OK    4
#define VIRTIO_STATUS_FEATURES_OK  8
#define VIRTIO_STATUS_FAILED       128

typedef struct virtio_blk_req {
    u32 type;
    u32 reserved;
    u64 sector;
    u8 data[512];
    u8 status;
} __attribute__((packed)) virtio_blk_req_t;

typedef struct virtio_blk_device {
    u64 base_addr;
    u64 capacity;
    u32 block_size;
    bool readonly;
    void* pci_dev;
} virtio_blk_device_t;

static virtio_blk_device_t* virtio_blk_dev = NULL;

static inline void virtio_write32(u64 addr, u32 value) {
    *((volatile u32*)addr) = value;
}

static inline u32 virtio_read32(u64 addr) {
    return *((volatile u32*)addr);
}

static inline void virtio_write64(u64 addr, u64 value) {
    *((volatile u64*)addr) = value;
}

static inline u64 virtio_read64(u64 addr) {
    return *((volatile u64*)addr);
}

static i32 virtio_blk_read_block(void* device, u64 block_num, void* buffer) {
    virtio_blk_device_t* dev = (virtio_blk_device_t*)device;
    
    if (!dev || !buffer) {
        return ERR_INVALID;
    }
    
    if (block_num >= dev->capacity) {
        return ERR_INVALID;
    }
    
    virtio_blk_req_t req;
    req.type = VIRTIO_BLK_T_IN;
    req.reserved = 0;
    req.sector = block_num;
    req.status = 0xFF;
    
    for (u32 i = 0; i < 512; i++) {
        req.data[i] = 0;
    }
    
    for (u32 i = 0; i < 512; i++) {
        ((u8*)buffer)[i] = req.data[i];
    }
    
    return 512;
}

static i32 virtio_blk_write_block(void* device, u64 block_num, const void* buffer) {
    virtio_blk_device_t* dev = (virtio_blk_device_t*)device;
    
    if (!dev || !buffer) {
        return ERR_INVALID;
    }
    
    if (dev->readonly) {
        return ERR_PERMISSION;
    }
    
    if (block_num >= dev->capacity) {
        return ERR_INVALID;
    }
    
    virtio_blk_req_t req;
    req.type = VIRTIO_BLK_T_OUT;
    req.reserved = 0;
    req.sector = block_num;
    req.status = 0xFF;
    
    for (u32 i = 0; i < 512; i++) {
        req.data[i] = ((const u8*)buffer)[i];
    }
    
    return 512;
}

static i32 virtio_blk_ioctl(void* device, u32 cmd, void* arg) {
    return ERR_INVALID;
}

static device_ops_t virtio_blk_ops = {
    .read = NULL,
    .write = NULL,
    .ioctl = NULL,
    .read_block = virtio_blk_read_block,
    .write_block = virtio_blk_write_block,
    .ioctl_block = virtio_blk_ioctl,
    .open = NULL,
    .close = NULL,
    .probe = NULL,
    .remove = NULL
};

void virtio_blk_init(void* pci_dev) {
    console_print("Initializing VirtIO block device...\n");
    
    virtio_blk_dev = (virtio_blk_device_t*)malloc(sizeof(virtio_blk_device_t));
    if (!virtio_blk_dev) {
        console_print("  Failed to allocate device structure\n");
        return;
    }
    
    virtio_blk_dev->pci_dev = pci_dev;
    virtio_blk_dev->base_addr = 0;
    virtio_blk_dev->capacity = 1024 * 1024;
    virtio_blk_dev->block_size = 512;
    virtio_blk_dev->readonly = false;
    
    i32 major = device_register("virtio-blk", DEVICE_BLOCK, &virtio_blk_ops, virtio_blk_dev);
    if (major < 0) {
        console_print("  Failed to register block device\n");
        free(virtio_blk_dev);
        virtio_blk_dev = NULL;
        return;
    }
    
    console_print("  VirtIO block device registered (major=");
    console_print_dec(major);
    console_print(", capacity=");
    console_print_dec(virtio_blk_dev->capacity);
    console_print(" blocks)\n");
    
    vfs_create_device_node("/dev/vda", S_IFBLK | 0660, major, 0);
}

void virtio_mmio_init(void* dt_dev) {
    console_print("Initializing VirtIO MMIO device...\n");
    
    virtio_blk_dev = (virtio_blk_device_t*)malloc(sizeof(virtio_blk_device_t));
    if (!virtio_blk_dev) {
        console_print("  Failed to allocate device structure\n");
        return;
    }
    
    virtio_blk_dev->pci_dev = NULL;
    virtio_blk_dev->base_addr = 0;
    virtio_blk_dev->capacity = 1024 * 1024;
    virtio_blk_dev->block_size = 512;
    virtio_blk_dev->readonly = false;
    
    i32 major = device_register("virtio-blk", DEVICE_BLOCK, &virtio_blk_ops, virtio_blk_dev);
    if (major < 0) {
        console_print("  Failed to register block device\n");
        free(virtio_blk_dev);
        virtio_blk_dev = NULL;
        return;
    }
    
    console_print("  VirtIO MMIO device registered\n");
}
