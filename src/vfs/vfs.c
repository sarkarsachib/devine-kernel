/* Virtual File System (VFS) Implementation */

#include "../include/types.h"
#include "../include/console.h"

// Forward declarations
extern void* malloc(u64 size);
extern void free(void* ptr);
extern i32 strcmp(const char* s1, const char* s2);
extern i32 strncmp(const char* s1, const char* s2, u64 n);
extern char* strncpy(char* dest, const char* src, u64 n);
extern char* strtok(char* str, const char* delim);
extern char* strrchr(const char* str, int c);
extern u64 system_time;

// Stub implementations for security functions
static inline u64 get_current_pid(void) {
    return 0;
}

static inline bool security_check_capability(u64 pid, u64 permissions, u64 cap) {
    return true;
}

// VFS node structure
typedef struct vfs_node {
    char name[MAX_STRING_LEN];
    u64 inode;
    u64 mode;
    u64 size;
    u64 permissions;
    u64 owner_uid;
    u64 owner_gid;
    u64 create_time;
    u64 access_time;
    u64 modify_time;
    u64 link_count;
    
    // VFS operations
    struct vfs_ops* ops;
    
    // File system specific data
    void* fs_data;
    
    // Mount point
    struct vfs_node* mount_point;
    
    // Link list
    struct vfs_node* next;
    struct vfs_node* prev;
    struct vfs_node* parent;
    struct vfs_node* children;
} vfs_node_t;

// VFS operations structure
typedef struct vfs_ops {
    // File operations
    i32 (*open)(vfs_node_t* node, u32 flags);
    i32 (*close)(vfs_node_t* node);
    i32 (*read)(vfs_node_t* node, u64 offset, u64 size, void* buffer);
    i32 (*write)(vfs_node_t* node, u64 offset, u64 size, const void* buffer);
    i32 (*ioctl)(vfs_node_t* node, u32 cmd, void* arg);
    
    // Directory operations
    i32 (*mkdir)(vfs_node_t* parent, const char* name, u64 permissions);
    i32 (*rmdir)(vfs_node_t* node);
    i32 (*create)(vfs_node_t* parent, const char* name, u64 permissions);
    i32 (*unlink)(vfs_node_t* node);
    
    // File system operations
    i32 (*mount)(vfs_node_t* mount_point, const char* device, const char* fstype);
    i32 (*umount)(vfs_node_t* mount_point);
    i32 (*sync)(vfs_node_t* node);
    
    // Lookup operations
    vfs_node_t* (*lookup)(vfs_node_t* parent, const char* name);
    i32 (*readdir)(vfs_node_t* dir, u64 index, vfs_node_t* result);
} vfs_ops_t;

// Mount point structure
typedef struct mount_point {
    char device[MAX_STRING_LEN];
    char fstype[MAX_STRING_LEN];
    vfs_node_t* mount_point;
    vfs_node_t* root;
    void* fs_data;
    struct mount_point* next;
} mount_point_t;

// File descriptor structure
typedef struct {
    vfs_node_t* node;
    u64 offset;
    u32 flags;
    u32 reference_count;
} file_descriptor_t;

// Global VFS structures
static vfs_node_t* vfs_root = NULL;
static mount_point_t* mount_points = NULL;
static file_descriptor_t file_descriptors[256];
static u32 next_fd = 3; // 0=stdin, 1=stdout, 2=stderr

// Standard file descriptors
#define FD_STDIN  0
#define FD_STDOUT 1
#define FD_STDERR 2

// VFS flags
#define O_READ     0x00000001
#define O_WRITE    0x00000002
#define O_CREATE   0x00000004
#define O_TRUNC    0x00000008
#define O_APPEND   0x00000010
#define O_EXCL     0x00000020

// File types
#define S_IFREG    0x1000
#define S_IFDIR    0x2000
#define S_IFLNK    0x3000
#define S_IFCHR    0x4000
#define S_IFBLK    0x5000
#define S_IFIFO    0x6000
#define S_IFSOCK   0x7000

// Initialize VFS
void vfs_init(void) {
    console_print("Creating root directory... ");
    vfs_root = vfs_create_node("/", S_IFDIR | 0755, 0, 0);
    console_print("OK\n");
    
    console_print("Setting up standard directories... ");
    vfs_mkdir(vfs_root, "dev", 0755);
    vfs_mkdir(vfs_root, "proc", 0755);
    vfs_mkdir(vfs_root, "tmp", 0755);
    vfs_mkdir(vfs_root, "usr", 0755);
    vfs_mkdir(vfs_root, "bin", 0755);
    vfs_mkdir(vfs_root, "lib", 0755);
    console_print("OK\n");
    
    console_print("Initializing file descriptor table... ");
    for (u32 i = 0; i < 256; i++) {
        file_descriptors[i].node = NULL;
        file_descriptors[i].offset = 0;
        file_descriptors[i].flags = 0;
        file_descriptors[i].reference_count = 0;
    }
    console_print("OK\n");
}

// Create a VFS node
vfs_node_t* vfs_create_node(const char* name, u64 mode, u64 uid, u64 gid) {
    vfs_node_t* node = (vfs_node_t*)malloc(sizeof(vfs_node_t));
    if (!node) {
        return NULL;
    }
    
    strncpy(node->name, name, MAX_STRING_LEN - 1);
    node->name[MAX_STRING_LEN - 1] = '\0';
    node->inode = (u64)node;  // Use address as inode
    node->mode = mode;
    node->size = 0;
    node->permissions = mode & 0777;
    node->owner_uid = uid;
    node->owner_gid = gid;
    node->create_time = system_time;
    node->access_time = system_time;
    node->modify_time = system_time;
    node->link_count = 1;
    node->ops = NULL;
    node->fs_data = NULL;
    node->mount_point = NULL;
    node->parent = NULL;
    node->children = NULL;
    node->next = NULL;
    node->prev = NULL;
    
    return node;
}

// Open a file
i32 vfs_open(const char* path, u32 flags) {
    vfs_node_t* node = vfs_lookup(path);
    
    if (!node && !(flags & O_CREATE)) {
        return ERR_NOT_FOUND;
    }
    
    if (!node && (flags & O_CREATE)) {
        // Create new file
        vfs_node_t* parent = vfs_get_parent(path);
        const char* name = vfs_get_basename(path);
        node = vfs_create_node(name, S_IFREG | 0666, 0, 0);
        if (node) {
            node->parent = parent;
            vfs_add_child(parent, node);
        }
    }
    
    if (!node) {
        return ERR_INVALID;
    }
    
    // Check permissions
    if (flags & O_WRITE && !security_check_capability(get_current_pid(), node->permissions, CAP_WRITE)) {
        return ERR_PERMISSION;
    }
    
    // Allocate file descriptor
    u32 fd = vfs_alloc_fd();
    if (fd == 0xFFFFFFFF) {
        return ERR_BUSY;
    }
    
    file_descriptors[fd].node = node;
    file_descriptors[fd].offset = 0;
    file_descriptors[fd].flags = flags;
    file_descriptors[fd].reference_count = 1;
    
    // Call open operation if defined
    if (node->ops && node->ops->open) {
        i32 result = node->ops->open(node, flags);
        if (result < 0) {
            vfs_free_fd(fd);
            return result;
        }
    }
    
    return fd;
}

// Close a file
i32 vfs_close(u32 fd) {
    if (fd >= 256 || !file_descriptors[fd].node) {
        return ERR_INVALID;
    }
    
    file_descriptors[fd].reference_count--;
    if (file_descriptors[fd].reference_count == 0) {
        vfs_node_t* node = file_descriptors[fd].node;
        
        // Call close operation if defined
        if (node->ops && node->ops->close) {
            node->ops->close(node);
        }
        
        file_descriptors[fd].node = NULL;
        file_descriptors[fd].flags = 0;
        file_descriptors[fd].offset = 0;
    }
    
    return ERR_SUCCESS;
}

// Read from a file
i32 vfs_read(u32 fd, u64 size, void* buffer) {
    if (fd >= 256 || !file_descriptors[fd].node) {
        return ERR_INVALID;
    }
    
    vfs_node_t* node = file_descriptors[fd].node;
    u64 offset = file_descriptors[fd].offset;
    
    // Check permissions
    if (!security_check_capability(get_current_pid(), node->permissions, CAP_READ)) {
        return ERR_PERMISSION;
    }
    
    i32 bytes_read = 0;
    if (node->ops && node->ops->read) {
        bytes_read = node->ops->read(node, offset, size, buffer);
        if (bytes_read > 0) {
            file_descriptors[fd].offset += bytes_read;
        }
    }
    
    return bytes_read;
}

// Write to a file
i32 vfs_write(u32 fd, u64 size, const void* buffer) {
    if (fd >= 256 || !file_descriptors[fd].node) {
        return ERR_INVALID;
    }
    
    vfs_node_t* node = file_descriptors[fd].node;
    u64 offset = file_descriptors[fd].offset;
    
    // Check permissions
    if (!security_check_capability(get_current_pid(), node->permissions, CAP_WRITE)) {
        return ERR_PERMISSION;
    }
    
    i32 bytes_written = 0;
    if (node->ops && node->ops->write) {
        bytes_written = node->ops->write(node, offset, size, buffer);
        if (bytes_written > 0) {
            file_descriptors[fd].offset += bytes_written;
            node->size += bytes_written;
            node->modify_time = system_time;
        }
    }
    
    return bytes_written;
}

// Lookup a path
vfs_node_t* vfs_lookup(const char* path) {
    if (path[0] != '/') {
        return NULL; // Only absolute paths supported for now
    }
    
    vfs_node_t* current = vfs_root;
    char path_copy[MAX_STRING_LEN];
    strncpy(path_copy, path, MAX_STRING_LEN - 1);
    
    char* token = strtok(path_copy + 1, "/");
    while (token && current) {
        current = vfs_find_child(current, token);
        token = strtok(NULL, "/");
    }
    
    return current;
}

// Create a directory
i32 vfs_mkdir(vfs_node_t* parent, const char* name, u64 permissions) {
    if (!parent || !name) {
        return ERR_INVALID;
    }
    
    // Check permissions
    if (!security_check_capability(get_current_pid(), parent->permissions, CAP_WRITE)) {
        return ERR_PERMISSION;
    }
    
    vfs_node_t* dir = vfs_create_node(name, S_IFDIR | permissions, 0, 0);
    if (!dir) {
        return ERR_NO_MEMORY;
    }
    
    dir->parent = parent;
    vfs_add_child(parent, dir);
    
    return ERR_SUCCESS;
}

// Add child to parent
void vfs_add_child(vfs_node_t* parent, vfs_node_t* child) {
    if (!parent || !child) return;
    
    child->next = parent->children;
    if (parent->children) {
        parent->children->prev = child;
    }
    parent->children = child;
    parent->link_count++;
}

// Find child by name
vfs_node_t* vfs_find_child(vfs_node_t* parent, const char* name) {
    if (!parent || !name) return NULL;
    
    vfs_node_t* child = parent->children;
    while (child && strcmp(child->name, name) != 0) {
        child = child->next;
    }
    
    return child;
}

// Get parent directory
vfs_node_t* vfs_get_parent(const char* path) {
    char path_copy[MAX_STRING_LEN];
    strncpy(path_copy, path, MAX_STRING_LEN - 1);
    
    char* last_slash = strrchr(path_copy, '/');
    if (!last_slash || last_slash == path_copy) {
        return vfs_root; // Root directory
    }
    
    *last_slash = '\0';
    return vfs_lookup(path_copy);
}

// Get basename from path
const char* vfs_get_basename(const char* path) {
    const char* last_slash = strrchr(path, '/');
    return last_slash ? last_slash + 1 : path;
}

// Forward declaration for ext2
extern vfs_node_t* ext2_get_vfs_root(void);

// Mount a file system
i32 vfs_mount(const char* device, const char* mount_point, const char* fstype) {
    vfs_node_t* mount_node = vfs_lookup(mount_point);
    if (!mount_node) {
        return ERR_NOT_FOUND;
    }
    
    mount_point_t* mp = (mount_point_t*)malloc(sizeof(mount_point_t));
    if (!mp) {
        return ERR_NO_MEMORY;
    }
    
    strncpy(mp->device, device, MAX_STRING_LEN - 1);
    strncpy(mp->fstype, fstype, MAX_STRING_LEN - 1);
    mp->mount_point = mount_node;
    
    // Get ext2 root if mounting ext2
    if (strcmp(fstype, "ext2") == 0) {
        mp->root = ext2_get_vfs_root();
        if (mp->root) {
            mount_node->mount_point = mp->root;
        } else {
            mp->root = mount_node;
        }
    } else {
        mp->root = mount_node;
    }
    
    mp->fs_data = NULL;
    
    // Add to mount list
    mp->next = mount_points;
    mount_points = mp;
    
    return ERR_SUCCESS;
}

// Allocate file descriptor
u32 vfs_alloc_fd(void) {
    for (u32 i = 0; i < 256; i++) {
        if (file_descriptors[i].node == NULL) {
            return i;
        }
    }
    return 0xFFFFFFFF; // No free descriptors
}

// Free file descriptor
void vfs_free_fd(u32 fd) {
    if (fd < 256) {
        file_descriptors[fd].node = NULL;
        file_descriptors[fd].offset = 0;
        file_descriptors[fd].flags = 0;
        file_descriptors[fd].reference_count = 0;
    }
}