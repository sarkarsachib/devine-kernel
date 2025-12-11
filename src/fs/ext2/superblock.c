/* ext2 Superblock and Block Group Management */

#include "../../include/types.h"
#include "../../include/ext2.h"
#include "../../include/block_cache.h"
#include "../../include/console.h"

extern void* malloc(u64 size);
extern void free(void* ptr);
extern void* memset(void* ptr, int value, u64 num);
extern void* memcpy(void* dest, const void* src, u64 num);

// Read superblock from device
i32 ext2_read_superblock(block_cache_t* cache, ext2_superblock_t* sb) {
    u8 buffer[1024];
    
    // Superblock is at offset 1024 (block 1 for 1024-byte blocks)
    i32 result = block_cache_read(cache, 1, buffer);
    if (result < 0) {
        return result;
    }
    
    memcpy(sb, buffer, sizeof(ext2_superblock_t));
    return ERR_SUCCESS;
}

// Write superblock to device
i32 ext2_write_superblock(block_cache_t* cache, ext2_superblock_t* sb) {
    u8 buffer[1024];
    
    memcpy(buffer, sb, sizeof(ext2_superblock_t));
    
    i32 result = block_cache_write(cache, 1, buffer);
    if (result < 0) {
        return result;
    }
    
    return ERR_SUCCESS;
}

// Validate superblock
bool ext2_validate_superblock(ext2_superblock_t* sb) {
    if (sb->s_magic != EXT2_MAGIC) {
        console_print("ext2: Invalid magic number\n");
        return false;
    }
    
    if (sb->s_inodes_count == 0 || sb->s_blocks_count == 0) {
        console_print("ext2: Invalid inode or block count\n");
        return false;
    }
    
    if (sb->s_blocks_per_group == 0 || sb->s_inodes_per_group == 0) {
        console_print("ext2: Invalid blocks/inodes per group\n");
        return false;
    }
    
    return true;
}

// Read block group descriptors
i32 ext2_read_block_groups(block_cache_t* cache, ext2_superblock_t* sb, 
                           ext2_block_group_desc_t** bg_out, u32* num_groups) {
    // Calculate number of block groups
    u32 num_bg = (sb->s_blocks_count + sb->s_blocks_per_group - 1) / sb->s_blocks_per_group;
    
    // Allocate memory for block group descriptors
    u32 bg_size = num_bg * sizeof(ext2_block_group_desc_t);
    ext2_block_group_desc_t* bg = (ext2_block_group_desc_t*)malloc(bg_size);
    if (!bg) {
        return ERR_NO_MEMORY;
    }
    
    // Block group descriptors start at block 2 (after superblock)
    u32 bg_block = 2;
    u32 descriptors_per_block = 1024 / sizeof(ext2_block_group_desc_t);
    
    u8 buffer[1024];
    for (u32 i = 0; i < num_bg; i += descriptors_per_block) {
        i32 result = block_cache_read(cache, bg_block++, buffer);
        if (result < 0) {
            free(bg);
            return result;
        }
        
        u32 count = (num_bg - i < descriptors_per_block) ? (num_bg - i) : descriptors_per_block;
        memcpy(&bg[i], buffer, count * sizeof(ext2_block_group_desc_t));
    }
    
    *bg_out = bg;
    *num_groups = num_bg;
    return ERR_SUCCESS;
}

// Write block group descriptors
i32 ext2_write_block_groups(block_cache_t* cache, ext2_block_group_desc_t* bg, u32 num_groups) {
    u32 bg_block = 2;
    u32 descriptors_per_block = 1024 / sizeof(ext2_block_group_desc_t);
    
    u8 buffer[1024];
    for (u32 i = 0; i < num_groups; i += descriptors_per_block) {
        u32 count = (num_groups - i < descriptors_per_block) ? (num_groups - i) : descriptors_per_block;
        memcpy(buffer, &bg[i], count * sizeof(ext2_block_group_desc_t));
        
        i32 result = block_cache_write(cache, bg_block++, buffer);
        if (result < 0) {
            return result;
        }
    }
    
    return ERR_SUCCESS;
}

// Get block group for a given block number
u32 ext2_get_block_group(ext2_superblock_t* sb, u32 block_num) {
    return (block_num - sb->s_first_data_block) / sb->s_blocks_per_group;
}

// Get block group for a given inode number
u32 ext2_get_inode_group(ext2_superblock_t* sb, u32 ino) {
    return (ino - 1) / sb->s_inodes_per_group;
}
