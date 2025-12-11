/* ext2 File Operations */

#include "../../include/types.h"
#include "../../include/ext2.h"
#include "../../include/block_cache.h"
#include "../../include/console.h"

extern void* malloc(u64 size);
extern void free(void* ptr);
extern void* memset(void* ptr, int value, u64 num);
extern void* memcpy(void* dest, const void* src, u64 num);
extern u64 system_time;

extern i32 ext2_get_block_num(ext2_fs_t* fs, ext2_inode_t* inode, u32 file_block, u32* block_num);
extern i32 ext2_set_block_num(ext2_fs_t* fs, ext2_inode_t* inode, u32 file_block, u32 block_num);

// Read from file
i32 ext2_read_file(ext2_fs_t* fs, ext2_inode_t* inode, u64 offset, u64 size, void* buffer) {
    if (offset >= inode->i_size) {
        return 0;
    }
    
    if (offset + size > inode->i_size) {
        size = inode->i_size - offset;
    }
    
    block_cache_t* cache = (block_cache_t*)fs->block_device;
    u8* out = (u8*)buffer;
    u64 bytes_read = 0;
    
    u8* block_buffer = (u8*)malloc(fs->block_size);
    if (!block_buffer) {
        return ERR_NO_MEMORY;
    }
    
    while (size > 0) {
        u32 file_block = offset / fs->block_size;
        u32 block_offset = offset % fs->block_size;
        u32 to_read = fs->block_size - block_offset;
        if (to_read > size) {
            to_read = size;
        }
        
        u32 block_num;
        i32 result = ext2_get_block_num(fs, inode, file_block, &block_num);
        if (result < 0) {
            free(block_buffer);
            return result;
        }
        
        if (block_num == 0) {
            memset(out, 0, to_read);
        } else {
            result = block_cache_read(cache, block_num, block_buffer);
            if (result < 0) {
                free(block_buffer);
                return result;
            }
            
            memcpy(out, block_buffer + block_offset, to_read);
        }
        
        out += to_read;
        offset += to_read;
        size -= to_read;
        bytes_read += to_read;
    }
    
    free(block_buffer);
    return bytes_read;
}

// Write to file
i32 ext2_write_file(ext2_fs_t* fs, ext2_inode_t* inode, u64 offset, u64 size, const void* buffer) {
    block_cache_t* cache = (block_cache_t*)fs->block_device;
    const u8* in = (const u8*)buffer;
    u64 bytes_written = 0;
    
    u8* block_buffer = (u8*)malloc(fs->block_size);
    if (!block_buffer) {
        return ERR_NO_MEMORY;
    }
    
    while (size > 0) {
        u32 file_block = offset / fs->block_size;
        u32 block_offset = offset % fs->block_size;
        u32 to_write = fs->block_size - block_offset;
        if (to_write > size) {
            to_write = size;
        }
        
        u32 block_num;
        i32 result = ext2_get_block_num(fs, inode, file_block, &block_num);
        if (result < 0) {
            free(block_buffer);
            return result;
        }
        
        if (block_num == 0) {
            result = ext2_alloc_block(fs, &block_num);
            if (result < 0) {
                free(block_buffer);
                return result;
            }
            
            result = ext2_set_block_num(fs, inode, file_block, block_num);
            if (result < 0) {
                ext2_free_block(fs, block_num);
                free(block_buffer);
                return result;
            }
            
            inode->i_blocks += (fs->block_size / 512);
        }
        
        if (block_offset != 0 || to_write != fs->block_size) {
            result = block_cache_read(cache, block_num, block_buffer);
            if (result < 0) {
                free(block_buffer);
                return result;
            }
        }
        
        memcpy(block_buffer + block_offset, in, to_write);
        
        result = block_cache_write(cache, block_num, block_buffer);
        if (result < 0) {
            free(block_buffer);
            return result;
        }
        
        in += to_write;
        offset += to_write;
        size -= to_write;
        bytes_written += to_write;
    }
    
    free(block_buffer);
    
    if (offset > inode->i_size) {
        inode->i_size = offset;
    }
    
    inode->i_mtime = system_time;
    fs->dirty = true;
    
    return bytes_written;
}

// Read entire block
i32 ext2_read_block(ext2_fs_t* fs, u32 block, void* buffer) {
    block_cache_t* cache = (block_cache_t*)fs->block_device;
    return block_cache_read(cache, block, buffer);
}

// Write entire block
i32 ext2_write_block(ext2_fs_t* fs, u32 block, const void* buffer) {
    block_cache_t* cache = (block_cache_t*)fs->block_device;
    fs->dirty = true;
    return block_cache_write(cache, block, buffer);
}
