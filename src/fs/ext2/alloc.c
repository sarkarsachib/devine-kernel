/* ext2 Block and Inode Allocator */

#include "../../include/types.h"
#include "../../include/ext2.h"
#include "../../include/block_cache.h"
#include "../../include/console.h"

extern void* malloc(u64 size);
extern void free(void* ptr);
extern void* memset(void* ptr, int value, u64 num);

extern u32 ext2_get_block_group(ext2_superblock_t* sb, u32 block_num);
extern u32 ext2_get_inode_group(ext2_superblock_t* sb, u32 ino);

// Find first zero bit in bitmap
static i32 find_first_zero_bit(u8* bitmap, u32 size) {
    for (u32 i = 0; i < size; i++) {
        if (bitmap[i] != 0xFF) {
            for (u32 j = 0; j < 8; j++) {
                if (!(bitmap[i] & (1 << j))) {
                    return i * 8 + j;
                }
            }
        }
    }
    return -1;
}

// Set bit in bitmap
static void set_bit(u8* bitmap, u32 bit) {
    bitmap[bit / 8] |= (1 << (bit % 8));
}

// Clear bit in bitmap
static void clear_bit(u8* bitmap, u32 bit) {
    bitmap[bit / 8] &= ~(1 << (bit % 8));
}

// Test bit in bitmap
static bool test_bit(u8* bitmap, u32 bit) {
    return (bitmap[bit / 8] & (1 << (bit % 8))) != 0;
}

// Allocate a block
i32 ext2_alloc_block(ext2_fs_t* fs, u32* block_num) {
    block_cache_t* cache = (block_cache_t*)fs->block_device;
    
    // Try each block group
    for (u32 group = 0; group < fs->num_block_groups; group++) {
        if (fs->block_groups[group].bg_free_blocks_count == 0) {
            continue;
        }
        
        // Read block bitmap
        u32 bitmap_block = fs->block_groups[group].bg_block_bitmap;
        u8* bitmap = (u8*)malloc(fs->block_size);
        if (!bitmap) {
            return ERR_NO_MEMORY;
        }
        
        i32 result = block_cache_read(cache, bitmap_block, bitmap);
        if (result < 0) {
            free(bitmap);
            return result;
        }
        
        // Find free block
        i32 bit = find_first_zero_bit(bitmap, fs->block_size);
        if (bit < 0 || bit >= (i32)fs->superblock->s_blocks_per_group) {
            free(bitmap);
            continue;
        }
        
        // Allocate block
        set_bit(bitmap, bit);
        
        // Write bitmap back
        result = block_cache_write(cache, bitmap_block, bitmap);
        free(bitmap);
        if (result < 0) {
            return result;
        }
        
        // Update block group descriptor
        fs->block_groups[group].bg_free_blocks_count--;
        
        // Update superblock
        fs->superblock->s_free_blocks_count--;
        
        // Calculate absolute block number
        *block_num = fs->superblock->s_first_data_block + 
                     group * fs->superblock->s_blocks_per_group + bit;
        
        fs->dirty = true;
        return ERR_SUCCESS;
    }
    
    return ERR_NO_MEMORY;
}

// Free a block
i32 ext2_free_block(ext2_fs_t* fs, u32 block_num) {
    if (block_num < fs->superblock->s_first_data_block || 
        block_num >= fs->superblock->s_blocks_count) {
        return ERR_INVALID;
    }
    
    u32 group = ext2_get_block_group(fs->superblock, block_num);
    if (group >= fs->num_block_groups) {
        return ERR_INVALID;
    }
    
    u32 bit = (block_num - fs->superblock->s_first_data_block) % 
              fs->superblock->s_blocks_per_group;
    
    // Read block bitmap
    block_cache_t* cache = (block_cache_t*)fs->block_device;
    u32 bitmap_block = fs->block_groups[group].bg_block_bitmap;
    u8* bitmap = (u8*)malloc(fs->block_size);
    if (!bitmap) {
        return ERR_NO_MEMORY;
    }
    
    i32 result = block_cache_read(cache, bitmap_block, bitmap);
    if (result < 0) {
        free(bitmap);
        return result;
    }
    
    // Free block
    if (!test_bit(bitmap, bit)) {
        console_print("ext2: Warning: freeing already free block\n");
    }
    
    clear_bit(bitmap, bit);
    
    // Write bitmap back
    result = block_cache_write(cache, bitmap_block, bitmap);
    free(bitmap);
    if (result < 0) {
        return result;
    }
    
    // Update block group descriptor
    fs->block_groups[group].bg_free_blocks_count++;
    
    // Update superblock
    fs->superblock->s_free_blocks_count++;
    
    fs->dirty = true;
    return ERR_SUCCESS;
}

// Allocate an inode
i32 ext2_alloc_inode(ext2_fs_t* fs, u32* ino) {
    block_cache_t* cache = (block_cache_t*)fs->block_device;
    
    // Try each block group
    for (u32 group = 0; group < fs->num_block_groups; group++) {
        if (fs->block_groups[group].bg_free_inodes_count == 0) {
            continue;
        }
        
        // Read inode bitmap
        u32 bitmap_block = fs->block_groups[group].bg_inode_bitmap;
        u8* bitmap = (u8*)malloc(fs->block_size);
        if (!bitmap) {
            return ERR_NO_MEMORY;
        }
        
        i32 result = block_cache_read(cache, bitmap_block, bitmap);
        if (result < 0) {
            free(bitmap);
            return result;
        }
        
        // Find free inode
        i32 bit = find_first_zero_bit(bitmap, fs->block_size);
        if (bit < 0 || bit >= (i32)fs->superblock->s_inodes_per_group) {
            free(bitmap);
            continue;
        }
        
        // Allocate inode
        set_bit(bitmap, bit);
        
        // Write bitmap back
        result = block_cache_write(cache, bitmap_block, bitmap);
        free(bitmap);
        if (result < 0) {
            return result;
        }
        
        // Update block group descriptor
        fs->block_groups[group].bg_free_inodes_count--;
        
        // Update superblock
        fs->superblock->s_free_inodes_count--;
        
        // Calculate absolute inode number
        *ino = group * fs->superblock->s_inodes_per_group + bit + 1;
        
        fs->dirty = true;
        return ERR_SUCCESS;
    }
    
    return ERR_NO_MEMORY;
}

// Free an inode
i32 ext2_free_inode(ext2_fs_t* fs, u32 ino) {
    if (ino == 0 || ino > fs->superblock->s_inodes_count) {
        return ERR_INVALID;
    }
    
    u32 group = ext2_get_inode_group(fs->superblock, ino);
    if (group >= fs->num_block_groups) {
        return ERR_INVALID;
    }
    
    u32 bit = (ino - 1) % fs->superblock->s_inodes_per_group;
    
    // Read inode bitmap
    block_cache_t* cache = (block_cache_t*)fs->block_device;
    u32 bitmap_block = fs->block_groups[group].bg_inode_bitmap;
    u8* bitmap = (u8*)malloc(fs->block_size);
    if (!bitmap) {
        return ERR_NO_MEMORY;
    }
    
    i32 result = block_cache_read(cache, bitmap_block, bitmap);
    if (result < 0) {
        free(bitmap);
        return result;
    }
    
    // Free inode
    if (!test_bit(bitmap, bit)) {
        console_print("ext2: Warning: freeing already free inode\n");
    }
    
    clear_bit(bitmap, bit);
    
    // Write bitmap back
    result = block_cache_write(cache, bitmap_block, bitmap);
    free(bitmap);
    if (result < 0) {
        return result;
    }
    
    // Update block group descriptor
    fs->block_groups[group].bg_free_inodes_count++;
    
    // Update superblock
    fs->superblock->s_free_inodes_count++;
    
    fs->dirty = true;
    return ERR_SUCCESS;
}
