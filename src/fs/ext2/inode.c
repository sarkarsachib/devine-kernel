/* ext2 Inode Operations */

#include "../../include/types.h"
#include "../../include/ext2.h"
#include "../../include/block_cache.h"
#include "../../include/console.h"

extern void* malloc(u64 size);
extern void free(void* ptr);
extern void* memset(void* ptr, int value, u64 num);
extern void* memcpy(void* dest, const void* src, u64 num);

extern u32 ext2_get_inode_group(ext2_superblock_t* sb, u32 ino);

// Read inode from disk
i32 ext2_read_inode(ext2_fs_t* fs, u32 ino, ext2_inode_t* inode) {
    if (ino == 0 || ino > fs->superblock->s_inodes_count) {
        return ERR_INVALID;
    }
    
    // Get block group containing this inode
    u32 group = ext2_get_inode_group(fs->superblock, ino);
    if (group >= fs->num_block_groups) {
        return ERR_INVALID;
    }
    
    // Get inode table block
    u32 inode_table = fs->block_groups[group].bg_inode_table;
    u32 inode_size = fs->superblock->s_inode_size ? fs->superblock->s_inode_size : 128;
    u32 inodes_per_block = fs->block_size / inode_size;
    u32 index = (ino - 1) % fs->superblock->s_inodes_per_group;
    u32 block = inode_table + (index / inodes_per_block);
    u32 offset = (index % inodes_per_block) * inode_size;
    
    // Read block containing inode
    u8* buffer = (u8*)malloc(fs->block_size);
    if (!buffer) {
        return ERR_NO_MEMORY;
    }
    
    block_cache_t* cache = (block_cache_t*)fs->block_device;
    i32 result = block_cache_read(cache, block, buffer);
    if (result < 0) {
        free(buffer);
        return result;
    }
    
    memcpy(inode, buffer + offset, sizeof(ext2_inode_t));
    free(buffer);
    
    return ERR_SUCCESS;
}

// Write inode to disk
i32 ext2_write_inode(ext2_fs_t* fs, u32 ino, ext2_inode_t* inode) {
    if (ino == 0 || ino > fs->superblock->s_inodes_count) {
        return ERR_INVALID;
    }
    
    // Get block group containing this inode
    u32 group = ext2_get_inode_group(fs->superblock, ino);
    if (group >= fs->num_block_groups) {
        return ERR_INVALID;
    }
    
    // Get inode table block
    u32 inode_table = fs->block_groups[group].bg_inode_table;
    u32 inode_size = fs->superblock->s_inode_size ? fs->superblock->s_inode_size : 128;
    u32 inodes_per_block = fs->block_size / inode_size;
    u32 index = (ino - 1) % fs->superblock->s_inodes_per_group;
    u32 block = inode_table + (index / inodes_per_block);
    u32 offset = (index % inodes_per_block) * inode_size;
    
    // Read block containing inode
    u8* buffer = (u8*)malloc(fs->block_size);
    if (!buffer) {
        return ERR_NO_MEMORY;
    }
    
    block_cache_t* cache = (block_cache_t*)fs->block_device;
    i32 result = block_cache_read(cache, block, buffer);
    if (result < 0) {
        free(buffer);
        return result;
    }
    
    // Update inode in buffer
    memcpy(buffer + offset, inode, sizeof(ext2_inode_t));
    
    // Write block back
    result = block_cache_write(cache, block, buffer);
    free(buffer);
    
    if (result < 0) {
        return result;
    }
    
    fs->dirty = true;
    return ERR_SUCCESS;
}

// Get block number for a file block (handles indirect blocks)
i32 ext2_get_block_num(ext2_fs_t* fs, ext2_inode_t* inode, u32 file_block, u32* block_num) {
    u32 addrs_per_block = fs->block_size / 4;
    
    // Direct blocks
    if (file_block < 12) {
        *block_num = inode->i_block[file_block];
        return ERR_SUCCESS;
    }
    
    file_block -= 12;
    
    // Single indirect
    if (file_block < addrs_per_block) {
        if (inode->i_block[12] == 0) {
            *block_num = 0;
            return ERR_SUCCESS;
        }
        
        u32* indirect = (u32*)malloc(fs->block_size);
        if (!indirect) {
            return ERR_NO_MEMORY;
        }
        
        block_cache_t* cache = (block_cache_t*)fs->block_device;
        i32 result = block_cache_read(cache, inode->i_block[12], indirect);
        if (result < 0) {
            free(indirect);
            return result;
        }
        
        *block_num = indirect[file_block];
        free(indirect);
        return ERR_SUCCESS;
    }
    
    file_block -= addrs_per_block;
    
    // Double indirect
    if (file_block < addrs_per_block * addrs_per_block) {
        if (inode->i_block[13] == 0) {
            *block_num = 0;
            return ERR_SUCCESS;
        }
        
        u32 indirect1_idx = file_block / addrs_per_block;
        u32 indirect2_idx = file_block % addrs_per_block;
        
        u32* indirect1 = (u32*)malloc(fs->block_size);
        if (!indirect1) {
            return ERR_NO_MEMORY;
        }
        
        block_cache_t* cache = (block_cache_t*)fs->block_device;
        i32 result = block_cache_read(cache, inode->i_block[13], indirect1);
        if (result < 0) {
            free(indirect1);
            return result;
        }
        
        u32 indirect2_block = indirect1[indirect1_idx];
        free(indirect1);
        
        if (indirect2_block == 0) {
            *block_num = 0;
            return ERR_SUCCESS;
        }
        
        u32* indirect2 = (u32*)malloc(fs->block_size);
        if (!indirect2) {
            return ERR_NO_MEMORY;
        }
        
        result = block_cache_read(cache, indirect2_block, indirect2);
        if (result < 0) {
            free(indirect2);
            return result;
        }
        
        *block_num = indirect2[indirect2_idx];
        free(indirect2);
        return ERR_SUCCESS;
    }
    
    // Triple indirect not implemented
    return ERR_INVALID;
}

// Set block number for a file block (handles indirect blocks)
i32 ext2_set_block_num(ext2_fs_t* fs, ext2_inode_t* inode, u32 file_block, u32 block_num) {
    u32 addrs_per_block = fs->block_size / 4;
    
    // Direct blocks
    if (file_block < 12) {
        inode->i_block[file_block] = block_num;
        return ERR_SUCCESS;
    }
    
    file_block -= 12;
    
    // Single indirect
    if (file_block < addrs_per_block) {
        if (inode->i_block[12] == 0) {
            // Allocate indirect block
            u32 indirect_block;
            i32 result = ext2_alloc_block(fs, &indirect_block);
            if (result < 0) {
                return result;
            }
            inode->i_block[12] = indirect_block;
            
            // Zero the indirect block
            u8* zero_block = (u8*)malloc(fs->block_size);
            if (!zero_block) {
                return ERR_NO_MEMORY;
            }
            memset(zero_block, 0, fs->block_size);
            
            block_cache_t* cache = (block_cache_t*)fs->block_device;
            result = block_cache_write(cache, indirect_block, zero_block);
            free(zero_block);
            if (result < 0) {
                return result;
            }
        }
        
        u32* indirect = (u32*)malloc(fs->block_size);
        if (!indirect) {
            return ERR_NO_MEMORY;
        }
        
        block_cache_t* cache = (block_cache_t*)fs->block_device;
        i32 result = block_cache_read(cache, inode->i_block[12], indirect);
        if (result < 0) {
            free(indirect);
            return result;
        }
        
        indirect[file_block] = block_num;
        result = block_cache_write(cache, inode->i_block[12], indirect);
        free(indirect);
        return result;
    }
    
    // Double and triple indirect not fully implemented for brevity
    return ERR_INVALID;
}
