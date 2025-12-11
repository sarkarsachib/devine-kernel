/* Simple RAM Disk Block Device Driver */

#include "../../include/types.h"

// RAM disk device structure
typedef struct ramdisk {
    u64 size;
    u8* data;
    u64 block_size;
} ramdisk_t;

// RAM disk operations
static i32 ramdisk_read_block(void* device, u64 block_num, void* buffer) {
    ramdisk_t* disk = (ramdisk_t*)device;
    if (!disk || !buffer) {
        return ERR_INVALID;
    }
    
    u64 offset = block_num * disk->block_size;
    if (offset >= disk->size) {
        return ERR_INVALID;
    }
    
    memcpy(buffer, disk->data + offset, disk->block_size);
    return disk->block_size;
}

static i32 ramdisk_write_block(void* device, u64 block_num, const void* buffer) {
    ramdisk_t* disk = (ramdisk_t*)device;
    if (!disk || !buffer) {
        return ERR_INVALID;
    }
    
    u64 offset = block_num * disk->block_size;
    if (offset >= disk->size) {
        return ERR_INVALID;
    }
    
    memcpy(disk->data + offset, buffer, disk->block_size);
    return disk->block_size;
}

static i32 ramdisk_ioctl(void* device, u32 cmd, void* arg) {
    ramdisk_t* disk = (ramdisk_t*)device;
    if (!disk) {
        return ERR_INVALID;
    }
    
    switch (cmd) {
        case 0: // Get size
            if (arg) {
                *(u64*)arg = disk->size;
                return ERR_SUCCESS;
            }
            break;
        case 1: // Get block size
            if (arg) {
                *(u64*)arg = disk->block_size;
                return ERR_SUCCESS;
            }
            break;
        default:
            return ERR_INVALID;
    }
    
    return ERR_INVALID;
}

static i32 ramdisk_open(void* device, u32 flags) {
    console_print("RAM disk opened\n");
    return ERR_SUCCESS;
}

static i32 ramdisk_close(void* device) {
    console_print("RAM disk closed\n");
    return ERR_SUCCESS;
}

static i32 ramdisk_probe(void* device) {
    console_print("RAM disk probed\n");
    return ERR_SUCCESS;
}

static i32 ramdisk_remove(void* device) {
    ramdisk_t* disk = (ramdisk_t*)device;
    if (disk) {
        if (disk->data) {
            free(disk->data);
        }
        free(disk);
    }
    console_print("RAM disk removed\n");
    return ERR_SUCCESS;
}

// Device operations structure
static device_ops_t ramdisk_ops = {
    .read_block = ramdisk_read_block,
    .write_block = ramdisk_write_block,
    .ioctl_block = ramdisk_ioctl,
    .open = ramdisk_open,
    .close = ramdisk_close,
    .probe = ramdisk_probe,
    .remove = ramdisk_remove,
};

// Global ramdisk instance for loading ext2 image
static ramdisk_t* g_ramdisk = NULL;

// Initialize RAM disk
void ramdisk_init(void) {
    ramdisk_t* disk = (ramdisk_t*)malloc(sizeof(ramdisk_t));
    if (!disk) {
        console_print("Failed to allocate RAM disk\n");
        return;
    }
    
    disk->size = 16 * 1024 * 1024; // 16MB to match ext2 image size
    disk->block_size = 512;
    disk->data = (u8*)malloc(disk->size);
    
    if (!disk->data) {
        console_print("Failed to allocate RAM disk memory\n");
        free(disk);
        return;
    }
    
    // Clear the disk
    memset(disk->data, 0, disk->size);
    
    g_ramdisk = disk;
    
    // Register the device
    i32 major = device_register("ramdisk", DEVICE_BLOCK, &ramdisk_ops, disk);
    console_print("RAM disk initialized (major=");
    console_print_dec(major);
    console_print(")\n");
}

// Load ext2 image into ramdisk
void ramdisk_load_ext2_image(void* image_data, u64 image_size) {
    if (!g_ramdisk || !image_data || image_size == 0) {
        console_print("ramdisk_load_ext2_image: invalid parameters\n");
        return;
    }
    
    if (image_size > g_ramdisk->size) {
        console_print("ramdisk_load_ext2_image: image too large\n");
        return;
    }
    
    console_print("Loading ext2 image into ramdisk (");
    console_print_dec(image_size);
    console_print(" bytes)... ");
    
    memcpy(g_ramdisk->data, image_data, image_size);
    
    console_print("OK\n");
}