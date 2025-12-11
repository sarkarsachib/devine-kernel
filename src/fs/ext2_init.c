/* ext2 Filesystem Initialization */

#include "../include/types.h"
#include "../include/ext2.h"
#include "../include/block_cache.h"
#include "../include/console.h"

extern void* malloc(u64 size);
extern void free(void* ptr);

// Forward declarations
typedef struct device device_t;
typedef struct vfs_node vfs_node_t;

extern device_t* device_find(const char* name);
extern block_device_t* block_device_create(device_t* device);
extern void block_device_destroy(block_device_t* bd);
extern ext2_fs_t* ext2_mount(void* block_device);
extern i32 ext2_umount(ext2_fs_t* fs);
extern vfs_node_t* ext2_create_vfs_root(ext2_fs_t* fs);
extern void ramdisk_init(void);
extern void ramdisk_load_ext2_image(void* image_data, u64 image_size);

// Global ext2 filesystem instance
static ext2_fs_t* g_ext2_fs = NULL;
static block_cache_t* g_block_cache = NULL;
static block_device_t* g_block_device = NULL;
static vfs_node_t* g_ext2_root = NULL;

// Initialize ext2 filesystem on ramdisk
void ext2_init(void) {
    console_print("\n=== ext2 Filesystem Initialization ===\n");
    
    // Find ramdisk device
    console_print("Looking for ramdisk device... ");
    device_t* ramdisk = device_find("ramdisk");
    if (!ramdisk) {
        console_print("FAIL\n");
        console_print("  ramdisk device not found\n");
        return;
    }
    console_print("OK\n");
    
    // Create block device wrapper
    console_print("Creating block device wrapper... ");
    g_block_device = block_device_create(ramdisk);
    if (!g_block_device) {
        console_print("FAIL\n");
        return;
    }
    console_print("OK\n");
    
    // Create block cache
    console_print("Creating block cache... ");
    g_block_cache = block_cache_create(g_block_device, 1024);
    if (!g_block_cache) {
        console_print("FAIL\n");
        block_device_destroy(g_block_device);
        return;
    }
    console_print("OK\n");
    
    // Mount ext2 filesystem
    console_print("Mounting ext2 filesystem...\n");
    g_ext2_fs = ext2_mount(g_block_cache);
    if (!g_ext2_fs) {
        console_print("FAIL: Could not mount ext2 filesystem\n");
        console_print("  This is expected if no ext2 image is loaded\n");
        block_cache_destroy(g_block_cache);
        block_device_destroy(g_block_device);
        return;
    }
    
    // Create VFS root node
    console_print("Creating VFS root node... ");
    g_ext2_root = ext2_create_vfs_root(g_ext2_fs);
    if (!g_ext2_root) {
        console_print("FAIL\n");
        ext2_umount(g_ext2_fs);
        block_cache_destroy(g_block_cache);
        block_device_destroy(g_block_device);
        return;
    }
    console_print("OK\n");
    
    // Display cache statistics
    u64 hits, misses;
    block_cache_stats(g_block_cache, &hits, &misses);
    console_print("Block cache: hits=");
    console_print_dec(hits);
    console_print(", misses=");
    console_print_dec(misses);
    console_print("\n");
    
    console_print("=== ext2 Filesystem Ready ===\n");
}

// Cleanup ext2 filesystem
void ext2_cleanup(void) {
    if (g_ext2_fs) {
        ext2_umount(g_ext2_fs);
    }
    if (g_block_cache) {
        block_cache_destroy(g_block_cache);
    }
    if (g_block_device) {
        block_device_destroy(g_block_device);
    }
}

// Get ext2 filesystem instance
ext2_fs_t* ext2_get_instance(void) {
    return g_ext2_fs;
}

// Get ext2 VFS root
vfs_node_t* ext2_get_vfs_root(void) {
    return g_ext2_root;
}
