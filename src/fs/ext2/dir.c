/* ext2 Directory Operations */

#include "../../include/types.h"
#include "../../include/ext2.h"
#include "../../include/block_cache.h"
#include "../../include/console.h"

extern void* malloc(u64 size);
extern void free(void* ptr);
extern void* memset(void* ptr, int value, u64 num);
extern void* memcpy(void* dest, const void* src, u64 num);
extern i32 strcmp(const char* s1, const char* s2);
extern i32 strncmp(const char* s1, const char* s2, u64 n);
extern char* strncpy(char* dest, const char* src, u64 n);
extern u64 strlen(const char* str);
extern u64 system_time;

extern i32 ext2_get_block_num(ext2_fs_t* fs, ext2_inode_t* inode, u32 file_block, u32* block_num);
extern i32 ext2_set_block_num(ext2_fs_t* fs, ext2_inode_t* inode, u32 file_block, u32 block_num);

// Lookup entry in directory
i32 ext2_lookup(ext2_fs_t* fs, u32 parent_ino, const char* name, u32* ino) {
    ext2_inode_t inode;
    i32 result = ext2_read_inode(fs, parent_ino, &inode);
    if (result < 0) {
        return result;
    }
    
    if ((inode.i_mode & 0xF000) != EXT2_S_IFDIR) {
        return ERR_INVALID;
    }
    
    block_cache_t* cache = (block_cache_t*)fs->block_device;
    u8* block_buffer = (u8*)malloc(fs->block_size);
    if (!block_buffer) {
        return ERR_NO_MEMORY;
    }
    
    u32 num_blocks = (inode.i_size + fs->block_size - 1) / fs->block_size;
    
    for (u32 i = 0; i < num_blocks; i++) {
        u32 block_num;
        result = ext2_get_block_num(fs, &inode, i, &block_num);
        if (result < 0 || block_num == 0) {
            continue;
        }
        
        result = block_cache_read(cache, block_num, block_buffer);
        if (result < 0) {
            free(block_buffer);
            return result;
        }
        
        u32 offset = 0;
        while (offset < fs->block_size) {
            ext2_dir_entry_t* entry = (ext2_dir_entry_t*)(block_buffer + offset);
            
            if (entry->rec_len == 0) {
                break;
            }
            
            if (entry->inode != 0 && entry->name_len == strlen(name) &&
                strncmp(entry->name, name, entry->name_len) == 0) {
                *ino = entry->inode;
                free(block_buffer);
                return ERR_SUCCESS;
            }
            
            offset += entry->rec_len;
        }
    }
    
    free(block_buffer);
    return ERR_NOT_FOUND;
}

// Read directory entry at index
i32 ext2_readdir(ext2_fs_t* fs, u32 ino, u64 index, ext2_dir_entry_t* entry) {
    ext2_inode_t inode;
    i32 result = ext2_read_inode(fs, ino, &inode);
    if (result < 0) {
        return result;
    }
    
    if ((inode.i_mode & 0xF000) != EXT2_S_IFDIR) {
        return ERR_INVALID;
    }
    
    block_cache_t* cache = (block_cache_t*)fs->block_device;
    u8* block_buffer = (u8*)malloc(fs->block_size);
    if (!block_buffer) {
        return ERR_NO_MEMORY;
    }
    
    u32 num_blocks = (inode.i_size + fs->block_size - 1) / fs->block_size;
    u64 current_index = 0;
    
    for (u32 i = 0; i < num_blocks; i++) {
        u32 block_num;
        result = ext2_get_block_num(fs, &inode, i, &block_num);
        if (result < 0 || block_num == 0) {
            continue;
        }
        
        result = block_cache_read(cache, block_num, block_buffer);
        if (result < 0) {
            free(block_buffer);
            return result;
        }
        
        u32 offset = 0;
        while (offset < fs->block_size) {
            ext2_dir_entry_t* dir_entry = (ext2_dir_entry_t*)(block_buffer + offset);
            
            if (dir_entry->rec_len == 0) {
                break;
            }
            
            if (dir_entry->inode != 0) {
                if (current_index == index) {
                    memcpy(entry, dir_entry, sizeof(ext2_dir_entry_t));
                    free(block_buffer);
                    return ERR_SUCCESS;
                }
                current_index++;
            }
            
            offset += dir_entry->rec_len;
        }
    }
    
    free(block_buffer);
    return ERR_NOT_FOUND;
}

// Add directory entry
static i32 ext2_add_dir_entry(ext2_fs_t* fs, u32 parent_ino, const char* name, u32 ino, u8 file_type) {
    ext2_inode_t parent_inode;
    i32 result = ext2_read_inode(fs, parent_ino, &parent_inode);
    if (result < 0) {
        return result;
    }
    
    block_cache_t* cache = (block_cache_t*)fs->block_device;
    u32 name_len = strlen(name);
    u32 required_len = 8 + name_len;
    required_len = (required_len + 3) & ~3;
    
    u8* block_buffer = (u8*)malloc(fs->block_size);
    if (!block_buffer) {
        return ERR_NO_MEMORY;
    }
    
    u32 num_blocks = (parent_inode.i_size + fs->block_size - 1) / fs->block_size;
    if (num_blocks == 0) {
        num_blocks = 1;
    }
    
    for (u32 i = 0; i < num_blocks; i++) {
        u32 block_num;
        result = ext2_get_block_num(fs, &parent_inode, i, &block_num);
        
        if (block_num == 0) {
            result = ext2_alloc_block(fs, &block_num);
            if (result < 0) {
                free(block_buffer);
                return result;
            }
            
            result = ext2_set_block_num(fs, &parent_inode, i, block_num);
            if (result < 0) {
                free(block_buffer);
                return result;
            }
            
            memset(block_buffer, 0, fs->block_size);
        } else {
            result = block_cache_read(cache, block_num, block_buffer);
            if (result < 0) {
                free(block_buffer);
                return result;
            }
        }
        
        u32 offset = 0;
        
        while (offset < fs->block_size) {
            ext2_dir_entry_t* entry = (ext2_dir_entry_t*)(block_buffer + offset);
            
            if (entry->rec_len == 0) {
                entry->inode = ino;
                entry->rec_len = fs->block_size - offset;
                entry->name_len = name_len;
                entry->file_type = file_type;
                strncpy(entry->name, name, name_len);
                entry->name[name_len] = '\0';
                
                result = block_cache_write(cache, block_num, block_buffer);
                free(block_buffer);
                
                if (i * fs->block_size + offset + required_len > parent_inode.i_size) {
                    parent_inode.i_size = i * fs->block_size + offset + required_len;
                    ext2_write_inode(fs, parent_ino, &parent_inode);
                }
                
                return result;
            }
            
            u32 actual_len = 8 + entry->name_len;
            actual_len = (actual_len + 3) & ~3;
            u32 free_space = entry->rec_len - actual_len;
            
            if (entry->inode != 0 && free_space >= required_len) {
                ext2_dir_entry_t* new_entry = (ext2_dir_entry_t*)(block_buffer + offset + actual_len);
                new_entry->inode = ino;
                new_entry->rec_len = entry->rec_len - actual_len;
                new_entry->name_len = name_len;
                new_entry->file_type = file_type;
                strncpy(new_entry->name, name, name_len);
                new_entry->name[name_len] = '\0';
                
                entry->rec_len = actual_len;
                
                result = block_cache_write(cache, block_num, block_buffer);
                free(block_buffer);
                return result;
            }
            
            offset += entry->rec_len;
        }
    }
    
    free(block_buffer);
    return ERR_NO_MEMORY;
}

// Create file in directory
i32 ext2_create(ext2_fs_t* fs, u32 parent_ino, const char* name, u16 mode, u32* ino) {
    i32 result = ext2_alloc_inode(fs, ino);
    if (result < 0) {
        return result;
    }
    
    ext2_inode_t inode;
    memset(&inode, 0, sizeof(ext2_inode_t));
    inode.i_mode = EXT2_S_IFREG | (mode & 0xFFF);
    inode.i_uid = 0;
    inode.i_gid = 0;
    inode.i_size = 0;
    inode.i_atime = system_time;
    inode.i_ctime = system_time;
    inode.i_mtime = system_time;
    inode.i_links_count = 1;
    inode.i_blocks = 0;
    
    result = ext2_write_inode(fs, *ino, &inode);
    if (result < 0) {
        ext2_free_inode(fs, *ino);
        return result;
    }
    
    result = ext2_add_dir_entry(fs, parent_ino, name, *ino, EXT2_FT_REG_FILE);
    if (result < 0) {
        ext2_free_inode(fs, *ino);
        return result;
    }
    
    return ERR_SUCCESS;
}

// Create directory
i32 ext2_mkdir(ext2_fs_t* fs, u32 parent_ino, const char* name, u16 mode, u32* ino) {
    i32 result = ext2_alloc_inode(fs, ino);
    if (result < 0) {
        return result;
    }
    
    ext2_inode_t inode;
    memset(&inode, 0, sizeof(ext2_inode_t));
    inode.i_mode = EXT2_S_IFDIR | (mode & 0xFFF);
    inode.i_uid = 0;
    inode.i_gid = 0;
    inode.i_size = fs->block_size;
    inode.i_atime = system_time;
    inode.i_ctime = system_time;
    inode.i_mtime = system_time;
    inode.i_links_count = 2;
    inode.i_blocks = (fs->block_size / 512);
    
    u32 block_num;
    result = ext2_alloc_block(fs, &block_num);
    if (result < 0) {
        ext2_free_inode(fs, *ino);
        return result;
    }
    
    inode.i_block[0] = block_num;
    
    u8* block_buffer = (u8*)malloc(fs->block_size);
    if (!block_buffer) {
        ext2_free_block(fs, block_num);
        ext2_free_inode(fs, *ino);
        return ERR_NO_MEMORY;
    }
    
    memset(block_buffer, 0, fs->block_size);
    
    ext2_dir_entry_t* dot = (ext2_dir_entry_t*)block_buffer;
    dot->inode = *ino;
    dot->rec_len = 12;
    dot->name_len = 1;
    dot->file_type = EXT2_FT_DIR;
    dot->name[0] = '.';
    
    ext2_dir_entry_t* dotdot = (ext2_dir_entry_t*)(block_buffer + 12);
    dotdot->inode = parent_ino;
    dotdot->rec_len = fs->block_size - 12;
    dotdot->name_len = 2;
    dotdot->file_type = EXT2_FT_DIR;
    dotdot->name[0] = '.';
    dotdot->name[1] = '.';
    
    block_cache_t* cache = (block_cache_t*)fs->block_device;
    result = block_cache_write(cache, block_num, block_buffer);
    free(block_buffer);
    
    if (result < 0) {
        ext2_free_block(fs, block_num);
        ext2_free_inode(fs, *ino);
        return result;
    }
    
    result = ext2_write_inode(fs, *ino, &inode);
    if (result < 0) {
        ext2_free_block(fs, block_num);
        ext2_free_inode(fs, *ino);
        return result;
    }
    
    result = ext2_add_dir_entry(fs, parent_ino, name, *ino, EXT2_FT_DIR);
    if (result < 0) {
        ext2_free_block(fs, block_num);
        ext2_free_inode(fs, *ino);
        return result;
    }
    
    ext2_inode_t parent_inode;
    if (ext2_read_inode(fs, parent_ino, &parent_inode) == ERR_SUCCESS) {
        parent_inode.i_links_count++;
        ext2_write_inode(fs, parent_ino, &parent_inode);
    }
    
    return ERR_SUCCESS;
}

// Unlink file from directory
i32 ext2_unlink(ext2_fs_t* fs, u32 parent_ino, const char* name) {
    u32 ino;
    i32 result = ext2_lookup(fs, parent_ino, name, &ino);
    if (result < 0) {
        return result;
    }
    
    ext2_inode_t inode;
    result = ext2_read_inode(fs, ino, &inode);
    if (result < 0) {
        return result;
    }
    
    inode.i_links_count--;
    if (inode.i_links_count == 0) {
        for (u32 i = 0; i < 15; i++) {
            if (inode.i_block[i] != 0) {
                ext2_free_block(fs, inode.i_block[i]);
            }
        }
        ext2_free_inode(fs, ino);
    } else {
        ext2_write_inode(fs, ino, &inode);
    }
    
    return ERR_SUCCESS;
}
