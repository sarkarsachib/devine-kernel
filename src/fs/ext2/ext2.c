/* ext2 Filesystem Main Implementation */

#include "../../include/types.h"
#include "../../include/ext2.h"
#include "../../include/block_cache.h"
#include "../../include/console.h"

extern void* malloc(u64 size);
extern void free(void* ptr);
extern void* memset(void* ptr, int value, u64 num);
extern void* memcpy(void* dest, const void* src, u64 num);

extern i32 ext2_read_superblock(block_cache_t* cache, ext2_superblock_t* sb);
extern i32 ext2_write_superblock(block_cache_t* cache, ext2_superblock_t* sb);
extern bool ext2_validate_superblock(ext2_superblock_t* sb);
extern i32 ext2_read_block_groups(block_cache_t* cache, ext2_superblock_t* sb, 
                                  ext2_block_group_desc_t** bg_out, u32* num_groups);
extern i32 ext2_write_block_groups(block_cache_t* cache, ext2_block_group_desc_t* bg, u32 num_groups);

// Mount ext2 filesystem
ext2_fs_t* ext2_mount(void* block_device) {
    console_print("ext2: Mounting filesystem...\n");
    
    if (!block_device) {
        console_print("ext2: Invalid block device\n");
        return NULL;
    }
    
    ext2_fs_t* fs = (ext2_fs_t*)malloc(sizeof(ext2_fs_t));
    if (!fs) {
        console_print("ext2: Failed to allocate filesystem structure\n");
        return NULL;
    }
    
    memset(fs, 0, sizeof(ext2_fs_t));
    fs->block_device = block_device;
    fs->dirty = false;
    
    fs->superblock = (ext2_superblock_t*)malloc(sizeof(ext2_superblock_t));
    if (!fs->superblock) {
        console_print("ext2: Failed to allocate superblock\n");
        free(fs);
        return NULL;
    }
    
    block_cache_t* cache = (block_cache_t*)block_device;
    i32 result = ext2_read_superblock(cache, fs->superblock);
    if (result < 0) {
        console_print("ext2: Failed to read superblock\n");
        free(fs->superblock);
        free(fs);
        return NULL;
    }
    
    if (!ext2_validate_superblock(fs->superblock)) {
        console_print("ext2: Invalid superblock\n");
        free(fs->superblock);
        free(fs);
        return NULL;
    }
    
    fs->block_size = 1024 << fs->superblock->s_log_block_size;
    console_print("ext2: Block size: ");
    console_print_dec(fs->block_size);
    console_print("\n");
    
    result = ext2_read_block_groups(cache, fs->superblock, &fs->block_groups, &fs->num_block_groups);
    if (result < 0) {
        console_print("ext2: Failed to read block groups\n");
        free(fs->superblock);
        free(fs);
        return NULL;
    }
    
    console_print("ext2: Number of block groups: ");
    console_print_dec(fs->num_block_groups);
    console_print("\n");
    
    console_print("ext2: Total blocks: ");
    console_print_dec(fs->superblock->s_blocks_count);
    console_print("\n");
    
    console_print("ext2: Total inodes: ");
    console_print_dec(fs->superblock->s_inodes_count);
    console_print("\n");
    
    console_print("ext2: Filesystem mounted successfully\n");
    
    return fs;
}

// Unmount ext2 filesystem
i32 ext2_umount(ext2_fs_t* fs) {
    if (!fs) {
        return ERR_INVALID;
    }
    
    console_print("ext2: Unmounting filesystem...\n");
    
    i32 result = ext2_sync(fs);
    if (result < 0) {
        console_print("ext2: Warning: sync failed during unmount\n");
    }
    
    if (fs->block_groups) {
        free(fs->block_groups);
    }
    
    if (fs->superblock) {
        free(fs->superblock);
    }
    
    free(fs);
    
    console_print("ext2: Filesystem unmounted\n");
    
    return ERR_SUCCESS;
}

// Sync filesystem to disk
i32 ext2_sync(ext2_fs_t* fs) {
    if (!fs || !fs->dirty) {
        return ERR_SUCCESS;
    }
    
    console_print("ext2: Syncing filesystem...\n");
    
    block_cache_t* cache = (block_cache_t*)fs->block_device;
    
    i32 result = ext2_write_superblock(cache, fs->superblock);
    if (result < 0) {
        console_print("ext2: Failed to write superblock\n");
        return result;
    }
    
    result = ext2_write_block_groups(cache, fs->block_groups, fs->num_block_groups);
    if (result < 0) {
        console_print("ext2: Failed to write block groups\n");
        return result;
    }
    
    result = block_cache_flush(cache);
    if (result < 0) {
        console_print("ext2: Failed to flush cache\n");
        return result;
    }
    
    fs->dirty = false;
    
    console_print("ext2: Filesystem synced\n");
    
    return ERR_SUCCESS;
}
