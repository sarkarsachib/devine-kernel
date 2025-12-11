/* ext2 Filesystem Demo and Tests */

#include "../include/types.h"
#include "../include/ext2.h"
#include "../include/block_cache.h"
#include "../include/console.h"

extern ext2_fs_t* ext2_get_instance(void);

// Demo: Create a file and write to it
void ext2_demo_create_file(void) {
    console_print("\n=== ext2 Demo: Create File ===\n");
    
    ext2_fs_t* fs = ext2_get_instance();
    if (!fs) {
        console_print("  ext2 filesystem not mounted\n");
        return;
    }
    
    // Create a file in root directory
    u32 file_ino;
    i32 result = ext2_create(fs, EXT2_ROOT_INO, "test.txt", 0644, &file_ino);
    if (result < 0) {
        console_print("  Failed to create file: ");
        console_print_dec(result);
        console_print("\n");
        return;
    }
    
    console_print("  File created with inode: ");
    console_print_dec(file_ino);
    console_print("\n");
    
    // Write some data to the file
    ext2_inode_t inode;
    result = ext2_read_inode(fs, file_ino, &inode);
    if (result < 0) {
        console_print("  Failed to read inode\n");
        return;
    }
    
    const char* data = "Hello from ext2 filesystem!\n";
    u64 data_len = 0;
    while (data[data_len]) data_len++;
    
    result = ext2_write_file(fs, &inode, 0, data_len, data);
    if (result < 0) {
        console_print("  Failed to write to file\n");
        return;
    }
    
    console_print("  Wrote ");
    console_print_dec(result);
    console_print(" bytes to file\n");
    
    // Update inode
    ext2_write_inode(fs, file_ino, &inode);
    
    console_print("  File size: ");
    console_print_dec(inode.i_size);
    console_print(" bytes\n");
}

// Demo: Read a file
void ext2_demo_read_file(void) {
    console_print("\n=== ext2 Demo: Read File ===\n");
    
    ext2_fs_t* fs = ext2_get_instance();
    if (!fs) {
        console_print("  ext2 filesystem not mounted\n");
        return;
    }
    
    // Lookup the file
    u32 file_ino;
    i32 result = ext2_lookup(fs, EXT2_ROOT_INO, "test.txt", &file_ino);
    if (result < 0) {
        console_print("  File not found\n");
        return;
    }
    
    console_print("  Found file with inode: ");
    console_print_dec(file_ino);
    console_print("\n");
    
    // Read the inode
    ext2_inode_t inode;
    result = ext2_read_inode(fs, file_ino, &inode);
    if (result < 0) {
        console_print("  Failed to read inode\n");
        return;
    }
    
    console_print("  File size: ");
    console_print_dec(inode.i_size);
    console_print(" bytes\n");
    
    // Read the file content
    char buffer[256];
    result = ext2_read_file(fs, &inode, 0, inode.i_size, buffer);
    if (result < 0) {
        console_print("  Failed to read file\n");
        return;
    }
    
    buffer[result] = '\0';
    console_print("  File content: ");
    console_print(buffer);
}

// Demo: List directory
void ext2_demo_list_dir(void) {
    console_print("\n=== ext2 Demo: List Root Directory ===\n");
    
    ext2_fs_t* fs = ext2_get_instance();
    if (!fs) {
        console_print("  ext2 filesystem not mounted\n");
        return;
    }
    
    ext2_dir_entry_t entry;
    u64 index = 0;
    
    while (1) {
        i32 result = ext2_readdir(fs, EXT2_ROOT_INO, index, &entry);
        if (result < 0) {
            break;
        }
        
        console_print("  ");
        console_print_dec(entry.inode);
        console_print(": ");
        
        for (u32 i = 0; i < entry.name_len && i < 255; i++) {
            char c = entry.name[i];
            if (c >= 32 && c <= 126) {
                console_putc(c);
            }
        }
        
        console_print(" (type=");
        console_print_dec(entry.file_type);
        console_print(")\n");
        
        index++;
    }
    
    console_print("  Total entries: ");
    console_print_dec(index);
    console_print("\n");
}

// Demo: Create directory
void ext2_demo_create_dir(void) {
    console_print("\n=== ext2 Demo: Create Directory ===\n");
    
    ext2_fs_t* fs = ext2_get_instance();
    if (!fs) {
        console_print("  ext2 filesystem not mounted\n");
        return;
    }
    
    u32 dir_ino;
    i32 result = ext2_mkdir(fs, EXT2_ROOT_INO, "mydir", 0755, &dir_ino);
    if (result < 0) {
        console_print("  Failed to create directory: ");
        console_print_dec(result);
        console_print("\n");
        return;
    }
    
    console_print("  Directory created with inode: ");
    console_print_dec(dir_ino);
    console_print("\n");
}

// Demo: Display cache statistics
void ext2_demo_cache_stats(void) {
    console_print("\n=== ext2 Demo: Cache Statistics ===\n");
    
    ext2_fs_t* fs = ext2_get_instance();
    if (!fs) {
        console_print("  ext2 filesystem not mounted\n");
        return;
    }
    
    block_cache_t* cache = (block_cache_t*)fs->block_device;
    u64 hits, misses;
    block_cache_stats(cache, &hits, &misses);
    
    console_print("  Cache hits: ");
    console_print_dec(hits);
    console_print("\n");
    
    console_print("  Cache misses: ");
    console_print_dec(misses);
    console_print("\n");
    
    if (hits + misses > 0) {
        u64 hit_rate = (hits * 100) / (hits + misses);
        console_print("  Hit rate: ");
        console_print_dec(hit_rate);
        console_print("%\n");
    }
}

// Run all demos
void ext2_run_demos(void) {
    console_print("\n========================================\n");
    console_print("=== ext2 Filesystem Demos ===\n");
    console_print("========================================\n");
    
    ext2_demo_list_dir();
    ext2_demo_create_file();
    ext2_demo_read_file();
    ext2_demo_create_dir();
    ext2_demo_list_dir();
    ext2_demo_cache_stats();
    
    // Sync filesystem
    ext2_fs_t* fs = ext2_get_instance();
    if (fs) {
        console_print("\nSyncing filesystem...\n");
        ext2_sync(fs);
    }
    
    console_print("\n========================================\n");
    console_print("=== ext2 Demos Complete ===\n");
    console_print("========================================\n\n");
}
