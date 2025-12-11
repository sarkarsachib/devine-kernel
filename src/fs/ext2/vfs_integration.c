/* ext2 VFS Integration */

#include "../../include/types.h"
#include "../../include/ext2.h"
#include "../../include/console.h"

extern void* malloc(u64 size);
extern void free(void* ptr);
extern char* strncpy(char* dest, const char* src, u64 n);
extern i32 strcmp(const char* s1, const char* s2);

// Forward declarations from vfs.c
typedef struct vfs_node vfs_node_t;
typedef struct vfs_ops vfs_ops_t;

extern vfs_node_t* vfs_create_node(const char* name, u64 mode, u64 uid, u64 gid);

// ext2 inode to VFS node mapping
typedef struct ext2_vfs_data {
    ext2_fs_t* fs;
    u32 ino;
} ext2_vfs_data_t;

// Convert ext2 mode to VFS mode
static u64 ext2_mode_to_vfs(u16 ext2_mode) {
    u64 vfs_mode = ext2_mode & 0xFFF;
    
    if ((ext2_mode & 0xF000) == EXT2_S_IFREG) {
        vfs_mode |= S_IFREG;
    } else if ((ext2_mode & 0xF000) == EXT2_S_IFDIR) {
        vfs_mode |= S_IFDIR;
    } else if ((ext2_mode & 0xF000) == EXT2_S_IFLNK) {
        vfs_mode |= S_IFLNK;
    }
    
    return vfs_mode;
}

// Convert VFS mode to ext2 mode
static u16 vfs_mode_to_ext2(u64 vfs_mode) {
    u16 ext2_mode = vfs_mode & 0xFFF;
    
    if ((vfs_mode & 0xF000) == S_IFREG) {
        ext2_mode |= EXT2_S_IFREG;
    } else if ((vfs_mode & 0xF000) == S_IFDIR) {
        ext2_mode |= EXT2_S_IFDIR;
    } else if ((vfs_mode & 0xF000) == S_IFLNK) {
        ext2_mode |= EXT2_S_IFLNK;
    }
    
    return ext2_mode;
}

// VFS open callback
static i32 ext2_vfs_open(vfs_node_t* node, u32 flags) {
    (void)node;
    (void)flags;
    return ERR_SUCCESS;
}

// VFS close callback
static i32 ext2_vfs_close(vfs_node_t* node) {
    (void)node;
    return ERR_SUCCESS;
}

// VFS read callback
static i32 ext2_vfs_read(vfs_node_t* node, u64 offset, u64 size, void* buffer) {
    if (!node || !node->fs_data) {
        return ERR_INVALID;
    }
    
    ext2_vfs_data_t* data = (ext2_vfs_data_t*)node->fs_data;
    ext2_inode_t inode;
    
    i32 result = ext2_read_inode(data->fs, data->ino, &inode);
    if (result < 0) {
        return result;
    }
    
    return ext2_read_file(data->fs, &inode, offset, size, buffer);
}

// VFS write callback
static i32 ext2_vfs_write(vfs_node_t* node, u64 offset, u64 size, const void* buffer) {
    if (!node || !node->fs_data) {
        return ERR_INVALID;
    }
    
    ext2_vfs_data_t* data = (ext2_vfs_data_t*)node->fs_data;
    ext2_inode_t inode;
    
    i32 result = ext2_read_inode(data->fs, data->ino, &inode);
    if (result < 0) {
        return result;
    }
    
    result = ext2_write_file(data->fs, &inode, offset, size, buffer);
    if (result > 0) {
        ext2_write_inode(data->fs, data->ino, &inode);
        node->size = inode.i_size;
    }
    
    return result;
}

// VFS lookup callback
static vfs_node_t* ext2_vfs_lookup(vfs_node_t* parent, const char* name) {
    if (!parent || !parent->fs_data || !name) {
        return NULL;
    }
    
    ext2_vfs_data_t* parent_data = (ext2_vfs_data_t*)parent->fs_data;
    u32 ino;
    
    i32 result = ext2_lookup(parent_data->fs, parent_data->ino, name, &ino);
    if (result < 0) {
        return NULL;
    }
    
    ext2_inode_t inode;
    result = ext2_read_inode(parent_data->fs, ino, &inode);
    if (result < 0) {
        return NULL;
    }
    
    vfs_node_t* node = vfs_create_node(name, ext2_mode_to_vfs(inode.i_mode), inode.i_uid, inode.i_gid);
    if (!node) {
        return NULL;
    }
    
    ext2_vfs_data_t* node_data = (ext2_vfs_data_t*)malloc(sizeof(ext2_vfs_data_t));
    if (!node_data) {
        free(node);
        return NULL;
    }
    
    node_data->fs = parent_data->fs;
    node_data->ino = ino;
    node->fs_data = node_data;
    node->size = inode.i_size;
    node->inode = ino;
    
    return node;
}

// VFS readdir callback
static i32 ext2_vfs_readdir(vfs_node_t* dir, u64 index, vfs_node_t* result) {
    if (!dir || !dir->fs_data || !result) {
        return ERR_INVALID;
    }
    
    ext2_vfs_data_t* dir_data = (ext2_vfs_data_t*)dir->fs_data;
    ext2_dir_entry_t entry;
    
    i32 res = ext2_readdir(dir_data->fs, dir_data->ino, index, &entry);
    if (res < 0) {
        return res;
    }
    
    ext2_inode_t inode;
    res = ext2_read_inode(dir_data->fs, entry.inode, &inode);
    if (res < 0) {
        return res;
    }
    
    strncpy(result->name, entry.name, entry.name_len);
    result->name[entry.name_len] = '\0';
    result->mode = ext2_mode_to_vfs(inode.i_mode);
    result->size = inode.i_size;
    result->inode = entry.inode;
    result->owner_uid = inode.i_uid;
    result->owner_gid = inode.i_gid;
    
    return ERR_SUCCESS;
}

// VFS mkdir callback
static i32 ext2_vfs_mkdir(vfs_node_t* parent, const char* name, u64 permissions) {
    if (!parent || !parent->fs_data || !name) {
        return ERR_INVALID;
    }
    
    ext2_vfs_data_t* parent_data = (ext2_vfs_data_t*)parent->fs_data;
    u32 ino;
    
    return ext2_mkdir(parent_data->fs, parent_data->ino, name, permissions, &ino);
}

// VFS create callback
static i32 ext2_vfs_create(vfs_node_t* parent, const char* name, u64 permissions) {
    if (!parent || !parent->fs_data || !name) {
        return ERR_INVALID;
    }
    
    ext2_vfs_data_t* parent_data = (ext2_vfs_data_t*)parent->fs_data;
    u32 ino;
    
    return ext2_create(parent_data->fs, parent_data->ino, name, permissions, &ino);
}

// VFS unlink callback
static i32 ext2_vfs_unlink(vfs_node_t* node) {
    if (!node || !node->fs_data || !node->parent || !node->parent->fs_data) {
        return ERR_INVALID;
    }
    
    ext2_vfs_data_t* parent_data = (ext2_vfs_data_t*)node->parent->fs_data;
    
    return ext2_unlink(parent_data->fs, parent_data->ino, node->name);
}

// VFS sync callback
static i32 ext2_vfs_sync(vfs_node_t* node) {
    if (!node || !node->fs_data) {
        return ERR_INVALID;
    }
    
    ext2_vfs_data_t* data = (ext2_vfs_data_t*)node->fs_data;
    
    return ext2_sync(data->fs);
}

// VFS mount callback
static i32 ext2_vfs_mount(vfs_node_t* mount_point, const char* device, const char* fstype) {
    (void)device;
    (void)fstype;
    
    if (!mount_point) {
        return ERR_INVALID;
    }
    
    console_print("ext2: VFS mount operation not fully implemented\n");
    
    return ERR_SUCCESS;
}

// ext2 VFS operations
static vfs_ops_t ext2_vfs_ops = {
    .open = ext2_vfs_open,
    .close = ext2_vfs_close,
    .read = ext2_vfs_read,
    .write = ext2_vfs_write,
    .ioctl = NULL,
    .mkdir = ext2_vfs_mkdir,
    .rmdir = NULL,
    .create = ext2_vfs_create,
    .unlink = ext2_vfs_unlink,
    .mount = ext2_vfs_mount,
    .umount = NULL,
    .sync = ext2_vfs_sync,
    .lookup = ext2_vfs_lookup,
    .readdir = ext2_vfs_readdir,
};

// Create VFS root node for mounted ext2 filesystem
vfs_node_t* ext2_create_vfs_root(ext2_fs_t* fs) {
    if (!fs) {
        return NULL;
    }
    
    ext2_inode_t root_inode;
    i32 result = ext2_read_inode(fs, EXT2_ROOT_INO, &root_inode);
    if (result < 0) {
        console_print("ext2: Failed to read root inode\n");
        return NULL;
    }
    
    vfs_node_t* root = vfs_create_node("/", ext2_mode_to_vfs(root_inode.i_mode), 
                                       root_inode.i_uid, root_inode.i_gid);
    if (!root) {
        console_print("ext2: Failed to create VFS root node\n");
        return NULL;
    }
    
    ext2_vfs_data_t* root_data = (ext2_vfs_data_t*)malloc(sizeof(ext2_vfs_data_t));
    if (!root_data) {
        free(root);
        return NULL;
    }
    
    root_data->fs = fs;
    root_data->ino = EXT2_ROOT_INO;
    
    root->fs_data = root_data;
    root->ops = &ext2_vfs_ops;
    root->size = root_inode.i_size;
    root->inode = EXT2_ROOT_INO;
    
    console_print("ext2: VFS root node created\n");
    
    return root;
}
