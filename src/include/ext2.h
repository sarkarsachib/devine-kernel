#pragma once

#include "types.h"

// ext2 constants
#define EXT2_MAGIC 0xEF53
#define EXT2_BLOCK_SIZE 1024
#define EXT2_NAME_LEN 255
#define EXT2_ROOT_INO 2

// File type indicators
#define EXT2_FT_UNKNOWN  0
#define EXT2_FT_REG_FILE 1
#define EXT2_FT_DIR      2
#define EXT2_FT_CHRDEV   3
#define EXT2_FT_BLKDEV   4
#define EXT2_FT_FIFO     5
#define EXT2_FT_SOCK     6
#define EXT2_FT_SYMLINK  7

// Inode mode bits
#define EXT2_S_IFREG  0x8000
#define EXT2_S_IFDIR  0x4000
#define EXT2_S_IFLNK  0xA000

// Superblock structure (1024 bytes)
typedef struct ext2_superblock {
    u32 s_inodes_count;
    u32 s_blocks_count;
    u32 s_r_blocks_count;
    u32 s_free_blocks_count;
    u32 s_free_inodes_count;
    u32 s_first_data_block;
    u32 s_log_block_size;
    u32 s_log_frag_size;
    u32 s_blocks_per_group;
    u32 s_frags_per_group;
    u32 s_inodes_per_group;
    u32 s_mtime;
    u32 s_wtime;
    u16 s_mnt_count;
    u16 s_max_mnt_count;
    u16 s_magic;
    u16 s_state;
    u16 s_errors;
    u16 s_minor_rev_level;
    u32 s_lastcheck;
    u32 s_checkinterval;
    u32 s_creator_os;
    u32 s_rev_level;
    u16 s_def_resuid;
    u16 s_def_resgid;
    u32 s_first_ino;
    u16 s_inode_size;
    u16 s_block_group_nr;
    u32 s_feature_compat;
    u32 s_feature_incompat;
    u32 s_feature_ro_compat;
    u8  s_uuid[16];
    u8  s_volume_name[16];
    u8  s_last_mounted[64];
    u32 s_algo_bitmap;
    u8  s_prealloc_blocks;
    u8  s_prealloc_dir_blocks;
    u16 s_padding1;
    u8  s_journal_uuid[16];
    u32 s_journal_inum;
    u32 s_journal_dev;
    u32 s_last_orphan;
    u32 s_hash_seed[4];
    u8  s_def_hash_version;
    u8  s_reserved_char_pad;
    u16 s_reserved_word_pad;
    u32 s_default_mount_opts;
    u32 s_first_meta_bg;
    u8  s_reserved[760];
} __attribute__((packed)) ext2_superblock_t;

// Block group descriptor (32 bytes)
typedef struct ext2_block_group_desc {
    u32 bg_block_bitmap;
    u32 bg_inode_bitmap;
    u32 bg_inode_table;
    u16 bg_free_blocks_count;
    u16 bg_free_inodes_count;
    u16 bg_used_dirs_count;
    u16 bg_pad;
    u8  bg_reserved[12];
} __attribute__((packed)) ext2_block_group_desc_t;

// Inode structure (128 bytes)
typedef struct ext2_inode {
    u16 i_mode;
    u16 i_uid;
    u32 i_size;
    u32 i_atime;
    u32 i_ctime;
    u32 i_mtime;
    u32 i_dtime;
    u16 i_gid;
    u16 i_links_count;
    u32 i_blocks;
    u32 i_flags;
    u32 i_osd1;
    u32 i_block[15];
    u32 i_generation;
    u32 i_file_acl;
    u32 i_dir_acl;
    u32 i_faddr;
    u8  i_osd2[12];
} __attribute__((packed)) ext2_inode_t;

// Directory entry
typedef struct ext2_dir_entry {
    u32 inode;
    u16 rec_len;
    u8  name_len;
    u8  file_type;
    char name[EXT2_NAME_LEN];
} __attribute__((packed)) ext2_dir_entry_t;

// ext2 filesystem instance
typedef struct ext2_fs {
    void* block_device;
    ext2_superblock_t* superblock;
    ext2_block_group_desc_t* block_groups;
    u32 block_size;
    u32 num_block_groups;
    bool dirty;
} ext2_fs_t;

// ext2 operations
ext2_fs_t* ext2_mount(void* block_device);
i32 ext2_umount(ext2_fs_t* fs);
i32 ext2_read_inode(ext2_fs_t* fs, u32 ino, ext2_inode_t* inode);
i32 ext2_write_inode(ext2_fs_t* fs, u32 ino, ext2_inode_t* inode);
i32 ext2_read_block(ext2_fs_t* fs, u32 block, void* buffer);
i32 ext2_write_block(ext2_fs_t* fs, u32 block, const void* buffer);
i32 ext2_alloc_block(ext2_fs_t* fs, u32* block_num);
i32 ext2_free_block(ext2_fs_t* fs, u32 block_num);
i32 ext2_alloc_inode(ext2_fs_t* fs, u32* ino);
i32 ext2_free_inode(ext2_fs_t* fs, u32 ino);
i32 ext2_lookup(ext2_fs_t* fs, u32 parent_ino, const char* name, u32* ino);
i32 ext2_readdir(ext2_fs_t* fs, u32 ino, u64 index, ext2_dir_entry_t* entry);
i32 ext2_create(ext2_fs_t* fs, u32 parent_ino, const char* name, u16 mode, u32* ino);
i32 ext2_mkdir(ext2_fs_t* fs, u32 parent_ino, const char* name, u16 mode, u32* ino);
i32 ext2_unlink(ext2_fs_t* fs, u32 parent_ino, const char* name);
i32 ext2_read_file(ext2_fs_t* fs, ext2_inode_t* inode, u64 offset, u64 size, void* buffer);
i32 ext2_write_file(ext2_fs_t* fs, ext2_inode_t* inode, u64 offset, u64 size, const void* buffer);
i32 ext2_sync(ext2_fs_t* fs);
