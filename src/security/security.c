/* Security Framework - Privilege Rings and Capability System */

#include "../../include/types.h"

// Capability descriptor structure
typedef struct capability {
    u64 cap_id;
    u64 owner_pid;
    u64 target_pid;
    u64 permissions;
    u64 resource_type;  // File, IPC, Memory, etc.
    u64 resource_id;
    u64 create_time;
    u64 expire_time;
    struct capability* next;
} capability_t;

// Task security context
typedef struct security_context {
    u64 task_pid;
    u64 user_id;
    u64 group_id;
    u64 privilege_level;    // 0-3 (0 = most privileged)
    u64 umask;
    capability_t* capabilities;
    struct security_context* next;
} security_context_t;

// Security constants
#define PRIV_RING_0   0  // Kernel mode (most privileged)
#define PRIV_RING_1   1  // Device drivers
#define PRIV_RING_2   2  // System services
#define PRIV_RING_3   3  // User mode (least privileged)

// Global security structures
static security_context_t* security_contexts = NULL;
static capability_t* global_capabilities = NULL;
static u64 next_capability_id = 1;

// Security initialization
void security_init(void) {
    console_print("Initializing security framework... ");
    
    // Create kernel security context
    security_context_t* kernel_ctx = security_create_context(0, 0, PRIV_RING_0);
    kernel_ctx->umask = 022;
    console_print("Created kernel context (Ring 0)... ");
    
    console_print("OK\n");
}

// Create security context for a task
security_context_t* security_create_context(u64 pid, u64 uid, u64 privilege_level) {
    security_context_t* ctx = (security_context_t*)malloc(sizeof(security_context_t));
    if (!ctx) {
        return NULL;
    }
    
    ctx->task_pid = pid;
    ctx->user_id = uid;
    ctx->group_id = uid; // For now, same as user
    ctx->privilege_level = privilege_level;
    ctx->umask = 022;
    ctx->capabilities = NULL;
    ctx->next = security_contexts;
    
    security_contexts = ctx;
    return ctx;
}

// Create a capability
u64 security_create_capability(u64 owner_pid, u64 target_pid, u64 permissions,
                               u64 resource_type, u64 resource_id, u64 expire_time) {
    capability_t* cap = (capability_t*)malloc(sizeof(capability_t));
    if (!cap) {
        return 0;
    }
    
    cap->cap_id = next_capability_id++;
    cap->owner_pid = owner_pid;
    cap->target_pid = target_pid;
    cap->permissions = permissions;
    cap->resource_type = resource_type;
    cap->resource_id = resource_id;
    cap->create_time = system_time;
    cap->expire_time = expire_time;
    cap->next = global_capabilities;
    
    global_capabilities = cap;
    return cap->cap_id;
}

// Check if a task has required capability
bool security_check_capability(u64 task_pid, u64 target_permissions, u64 required_permissions) {
    security_context_t* ctx = security_find_context(task_pid);
    if (!ctx) {
        return false; // No security context
    }
    
    // Kernel tasks have all capabilities
    if (ctx->privilege_level == PRIV_RING_0) {
        return true;
    }
    
    // Check if task has required permissions
    u64 granted_permissions = target_permissions & 0777;
    return (granted_permissions & required_permissions) == required_permissions;
}

// Check privilege ring
bool security_check_privilege(u64 task_pid, u64 required_ring) {
    security_context_t* ctx = security_find_context(task_pid);
    if (!ctx) {
        return false;
    }
    
    return ctx->privilege_level <= required_ring;
}

// Revoke a capability
i32 security_revoke_capability(u64 cap_id) {
    capability_t* cap = global_capabilities;
    capability_t* prev = NULL;
    
    while (cap && cap->cap_id != cap_id) {
        prev = cap;
        cap = cap->next;
    }
    
    if (!cap) {
        return ERR_NOT_FOUND;
    }
    
    if (prev) {
        prev->next = cap->next;
    } else {
        global_capabilities = cap->next;
    }
    
    free(cap);
    return ERR_SUCCESS;
}

// Validate memory access
bool security_validate_memory_access(u64 task_pid, void* addr, u64 size, u8 access_type) {
    security_context_t* ctx = security_find_context(task_pid);
    if (!ctx) {
        return false;
    }
    
    // Kernel can access anything
    if (ctx->privilege_level == PRIV_RING_0) {
        return true;
    }
    
    u64 task_addr = (u64)addr;
    
    // Basic bounds checking
    // User space: 0x0000000000000000 - 0x00007FFFFFFFFFFF
    // Kernel space: 0xFFFF800000000000 - 0xFFFFFFFFFFFFFFFF
    if (task_addr >= 0x0000800000000000UL && task_addr < 0xFFFF800000000000UL) {
        // Access to kernel space from user mode
        return false;
    }
    
    // Check memory protection based on access type
    switch (access_type) {
        case 0: // Read
        case 1: // Write
        case 2: // Execute
            // For now, allow access to user space
            return true;
        default:
            return false;
    }
}

// Set umask for a task
void security_set_umask(u64 task_pid, u64 umask) {
    security_context_t* ctx = security_find_context(task_pid);
    if (ctx) {
        ctx->umask = umask & 0777;
    }
}

// Get umask for a task
u64 security_get_umask(u64 task_pid) {
    security_context_t* ctx = security_find_context(task_pid);
    if (ctx) {
        return ctx->umask;
    }
    return 022;
}

// Find security context
security_context_t* security_find_context(u64 task_pid) {
    security_context_t* ctx = security_contexts;
    while (ctx && ctx->task_pid != task_pid) {
        ctx = ctx->next;
    }
    return ctx;
}

// Clean up capabilities (remove expired ones)
void security_cleanup_capabilities(void) {
    capability_t* cap = global_capabilities;
    capability_t* prev = NULL;
    
    while (cap) {
        if (cap->expire_time && cap->expire_time < system_time) {
            // Remove expired capability
            capability_t* next = cap->next;
            
            if (prev) {
                prev->next = next;
            } else {
                global_capabilities = next;
            }
            
            free(cap);
            cap = next;
        } else {
            prev = cap;
            cap = cap->next;
        }
    }
}

// Get security statistics
void security_get_stats(u64* task_count, u64* capability_count) {
    security_context_t* ctx = security_contexts;
    *task_count = 0;
    while (ctx) {
        (*task_count)++;
        ctx = ctx->next;
    }
    
    capability_t* cap = global_capabilities;
    *capability_count = 0;
    while (cap) {
        (*capability_count)++;
        cap = cap->next;
    }
}