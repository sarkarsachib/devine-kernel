/* Simple mkfs.ext2 - Creates a minimal ext2 filesystem image */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

#define EXT2_MAGIC 0xEF53
#define EXT2_ROOT_INO 2
#define EXT2_S_IFREG 0x8000
#define EXT2_S_IFDIR 0x4000
#define EXT2_FT_DIR 2

typedef struct {
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

typedef struct {
    u32 bg_block_bitmap;
    u32 bg_inode_bitmap;
    u32 bg_inode_table;
    u16 bg_free_blocks_count;
    u16 bg_free_inodes_count;
    u16 bg_used_dirs_count;
    u16 bg_pad;
    u8  bg_reserved[12];
} __attribute__((packed)) ext2_block_group_desc_t;

typedef struct {
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

typedef struct {
    u32 inode;
    u16 rec_len;
    u8  name_len;
    u8  file_type;
    char name[255];
} __attribute__((packed)) ext2_dir_entry_t;

void create_ext2_image(const char* filename, u32 size_mb) {
    printf("Creating ext2 image: %s (%u MB)\n", filename, size_mb);
    
    u32 block_size = 1024;
    u32 total_blocks = (size_mb * 1024 * 1024) / block_size;
    u32 inodes_per_group = 128;
    u32 blocks_per_group = 8192;
    u32 total_inodes = total_blocks / 8;
    if (total_inodes < 32) total_inodes = 32;
    
    u32 num_groups = (total_blocks + blocks_per_group - 1) / blocks_per_group;
    if (num_groups == 0) num_groups = 1;
    
    FILE* fp = fopen(filename, "wb");
    if (!fp) {
        perror("Failed to create image");
        exit(1);
    }
    
    u8* zero_block = calloc(1, block_size);
    for (u32 i = 0; i < total_blocks; i++) {
        fwrite(zero_block, 1, block_size, fp);
    }
    free(zero_block);
    
    ext2_superblock_t sb = {0};
    sb.s_inodes_count = inodes_per_group * num_groups;
    sb.s_blocks_count = total_blocks;
    sb.s_r_blocks_count = 0;
    sb.s_free_blocks_count = total_blocks - 10;
    sb.s_free_inodes_count = sb.s_inodes_count - 11;
    sb.s_first_data_block = 1;
    sb.s_log_block_size = 0;
    sb.s_log_frag_size = 0;
    sb.s_blocks_per_group = blocks_per_group;
    sb.s_frags_per_group = blocks_per_group;
    sb.s_inodes_per_group = inodes_per_group;
    sb.s_mtime = time(NULL);
    sb.s_wtime = time(NULL);
    sb.s_mnt_count = 0;
    sb.s_max_mnt_count = 20;
    sb.s_magic = EXT2_MAGIC;
    sb.s_state = 1;
    sb.s_errors = 1;
    sb.s_minor_rev_level = 0;
    sb.s_lastcheck = time(NULL);
    sb.s_checkinterval = 0;
    sb.s_creator_os = 0;
    sb.s_rev_level = 0;
    sb.s_def_resuid = 0;
    sb.s_def_resgid = 0;
    sb.s_first_ino = 11;
    sb.s_inode_size = 128;
    sb.s_block_group_nr = 0;
    
    fseek(fp, 1024, SEEK_SET);
    fwrite(&sb, sizeof(ext2_superblock_t), 1, fp);
    
    ext2_block_group_desc_t bg = {0};
    bg.bg_block_bitmap = 3;
    bg.bg_inode_bitmap = 4;
    bg.bg_inode_table = 5;
    bg.bg_free_blocks_count = total_blocks - 10;
    bg.bg_free_inodes_count = inodes_per_group - 11;
    bg.bg_used_dirs_count = 1;
    
    fseek(fp, 2048, SEEK_SET);
    fwrite(&bg, sizeof(ext2_block_group_desc_t), 1, fp);
    
    u8* block_bitmap = calloc(1, block_size);
    for (u32 i = 0; i < 10; i++) {
        block_bitmap[i / 8] |= (1 << (i % 8));
    }
    fseek(fp, 3 * block_size, SEEK_SET);
    fwrite(block_bitmap, 1, block_size, fp);
    free(block_bitmap);
    
    u8* inode_bitmap = calloc(1, block_size);
    for (u32 i = 0; i < 11; i++) {
        inode_bitmap[i / 8] |= (1 << (i % 8));
    }
    fseek(fp, 4 * block_size, SEEK_SET);
    fwrite(inode_bitmap, 1, block_size, fp);
    free(inode_bitmap);
    
    ext2_inode_t root_inode = {0};
    root_inode.i_mode = EXT2_S_IFDIR | 0755;
    root_inode.i_uid = 0;
    root_inode.i_gid = 0;
    root_inode.i_size = block_size;
    root_inode.i_atime = time(NULL);
    root_inode.i_ctime = time(NULL);
    root_inode.i_mtime = time(NULL);
    root_inode.i_links_count = 2;
    root_inode.i_blocks = 2;
    root_inode.i_block[0] = 10;
    
    fseek(fp, 5 * block_size + (EXT2_ROOT_INO - 1) * 128, SEEK_SET);
    fwrite(&root_inode, sizeof(ext2_inode_t), 1, fp);
    
    u8* root_dir = calloc(1, block_size);
    ext2_dir_entry_t* dot = (ext2_dir_entry_t*)root_dir;
    dot->inode = EXT2_ROOT_INO;
    dot->rec_len = 12;
    dot->name_len = 1;
    dot->file_type = EXT2_FT_DIR;
    dot->name[0] = '.';
    
    ext2_dir_entry_t* dotdot = (ext2_dir_entry_t*)(root_dir + 12);
    dotdot->inode = EXT2_ROOT_INO;
    dotdot->rec_len = block_size - 12;
    dotdot->name_len = 2;
    dotdot->file_type = EXT2_FT_DIR;
    dotdot->name[0] = '.';
    dotdot->name[1] = '.';
    
    fseek(fp, 10 * block_size, SEEK_SET);
    fwrite(root_dir, 1, block_size, fp);
    free(root_dir);
    
    fclose(fp);
    
    printf("ext2 image created successfully\n");
    printf("  Total blocks: %u\n", total_blocks);
    printf("  Total inodes: %u\n", sb.s_inodes_count);
    printf("  Block groups: %u\n", num_groups);
    printf("  Block size: %u\n", block_size);
}

int main(int argc, char** argv) {
    if (argc != 3) {
        printf("Usage: %s <image_file> <size_mb>\n", argv[0]);
        return 1;
    }
    
    const char* filename = argv[1];
    u32 size_mb = atoi(argv[2]);
    
    if (size_mb < 1 || size_mb > 1024) {
        printf("Error: Size must be between 1 and 1024 MB\n");
        return 1;
    }
    
    create_ext2_image(filename, size_mb);
    
    return 0;
}
