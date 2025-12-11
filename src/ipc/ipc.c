/* Inter-Process Communication (IPC) Primitives */

#include "../../include/types.h"
#include "../../include/security.h"
#include "../../include/console.h"

// IPC message structure
typedef struct ipc_message {
    u64 sender_pid;
    u64 receiver_pid;
    u64 message_type;
    u64 message_size;
    u64 timestamp;
    u8 data[0];  // Flexible array for message payload
} ipc_message_t;

// Message queue structure
typedef struct message_queue {
    u64 queue_id;
    u64 owner_pid;
    u64 permissions;
    ipc_message_t* messages;
    u32 message_count;
    u32 max_messages;
    u32 max_message_size;
    struct message_queue* next;
    struct message_queue* prev;
} message_queue_t;

// Shared memory channel structure
typedef struct shm_channel {
    u64 channel_id;
    u64 owner_pid;
    u64 permissions;
    void* shared_memory;
    u64 size;
    u32 ref_count;
    struct shm_channel* next;
    struct shm_channel* prev;
} shm_channel_t;

// Signal structure
typedef struct signal {
    u64 signal_number;
    u64 sender_pid;
    u64 receiver_pid;
    u64 timestamp;
    struct signal* next;
} signal_t;

// Priority inheritance structure
typedef struct {
    u64 task_pid;
    u64 base_priority;
    u64 current_priority;
    u64 resource_held;
    u64 waiting_tasks[64];  // Tasks waiting on this task's resources
    u32 waiting_count;
} priority_inheritance_t;

// Global IPC structures
static message_queue_t* message_queues = NULL;
static shm_channel_t* shm_channels = NULL;
static signal_t* signals = NULL;
static priority_inheritance_t pi_table[MAX_CPUS];

// IPC capabilities
#define IPC_READ    0x00000001
#define IPC_WRITE   0x00000002
#define IPC_CREATE  0x00000004
#define IPC_ADMIN   0x00000008

// Signal numbers
#define SIGTERM     1
#define SIGKILL     2
#define SIGINT      3
#define SIGUSR1     10
#define SIGUSR2     11

// Initialize IPC system
void ipc_init(void) {
    console_print("Setting up message queues... ");
    message_queues = NULL;
    console_print("OK\n");
    
    console_print("Setting up shared memory channels... ");
    shm_channels = NULL;
    console_print("OK\n");
    
    console_print("Setting up signals... ");
    signals = NULL;
    console_print("OK\n");
    
    console_print("Initializing priority inheritance... ");
    for (u32 i = 0; i < MAX_CPUS; i++) {
        pi_table[i].task_pid = 0;
        pi_table[i].base_priority = 0;
        pi_table[i].current_priority = 0;
        pi_table[i].resource_held = 0;
        pi_table[i].waiting_count = 0;
    }
    console_print("OK\n");
}

// Create a message queue
u64 ipc_create_message_queue(u64 permissions) {
    message_queue_t* queue = (message_queue_t*)mm_alloc_page();
    if (!queue) {
        return ERR_NO_MEMORY;
    }
    
    queue->queue_id = (u64)queue;  // Use address as ID
    queue->owner_pid = get_current_pid();
    queue->permissions = permissions;
    queue->message_count = 0;
    queue->max_messages = 256;
    queue->max_message_size = 1024;
    
    // Allocate message buffer
    queue->messages = (ipc_message_t*)malloc(queue->max_messages * queue->max_message_size);
    if (!queue->messages) {
        mm_free_page(queue);
        return ERR_NO_MEMORY;
    }
    
    // Add to global list
    queue->next = message_queues;
    if (message_queues) {
        message_queues->prev = queue;
    }
    message_queues = queue;
    
    return queue->queue_id;
}

// Send a message
i32 ipc_send_message(u64 queue_id, u64 receiver_pid, u64 message_type, 
                    const void* data, u64 size) {
    message_queue_t* queue = ipc_find_queue(queue_id);
    if (!queue) {
        return ERR_NOT_FOUND;
    }
    
    // Check permissions
    if (!security_check_capability(get_current_pid(), queue->permissions, IPC_WRITE)) {
        return ERR_PERMISSION;
    }
    
    // Check queue capacity
    if (queue->message_count >= queue->max_messages) {
        return ERR_BUSY;
    }
    
    // Check message size
    if (size > queue->max_message_size) {
        return ERR_INVALID;
    }
    
    // Create message
    ipc_message_t* msg = (ipc_message_t*)((u8*)queue->messages + 
                                         (queue->message_count * queue->max_message_size));
    msg->sender_pid = get_current_pid();
    msg->receiver_pid = receiver_pid;
    msg->message_type = message_type;
    msg->message_size = size;
    msg->timestamp = system_time;
    
    // Copy data
    memcpy(msg->data, data, size);
    
    queue->message_count++;
    
    // Wake up receiver if waiting
    scheduler_wake_task(receiver_pid);
    
    return ERR_SUCCESS;
}

// Receive a message
i32 ipc_receive_message(u64 queue_id, u64* sender_pid, u64* message_type,
                       void* buffer, u64* size) {
    message_queue_t* queue = ipc_find_queue(queue_id);
    if (!queue) {
        return ERR_NOT_FOUND;
    }
    
    // Check permissions
    if (!security_check_capability(get_current_pid(), queue->permissions, IPC_READ)) {
        return ERR_PERMISSION;
    }
    
    // Wait for message if none available
    while (queue->message_count == 0) {
        scheduler_sleep_task(get_current_pid());
    }
    
    // Get first message
    ipc_message_t* msg = (ipc_message_t*)queue->messages;
    
    *sender_pid = msg->sender_pid;
    *message_type = msg->message_type;
    *size = msg->message_size;
    
    // Copy data
    if (buffer && *size > 0) {
        memcpy(buffer, msg->data, *size);
    }
    
    // Remove message from queue (simple implementation - just move others)
    queue->message_count--;
    for (u32 i = 0; i < queue->message_count; i++) {
        memcpy(&queue->messages[i], &queue->messages[i + 1], sizeof(ipc_message_t));
    }
    
    return ERR_SUCCESS;
}

// Create shared memory channel
u64 ipc_create_shm_channel(u64 size, u64 permissions) {
    shm_channel_t* channel = (shm_channel_t*)mm_alloc_page();
    if (!channel) {
        return ERR_NO_MEMORY;
    }
    
    channel->channel_id = (u64)channel;  // Use address as ID
    channel->owner_pid = get_current_pid();
    channel->permissions = permissions;
    channel->size = ALIGN_UP(size, PAGE_SIZE);
    channel->ref_count = 0;
    
    // Allocate shared memory
    channel->shared_memory = mm_alloc_page();
    if (!channel->shared_memory) {
        mm_free_page(channel);
        return ERR_NO_MEMORY;
    }
    
    // Add to global list
    channel->next = shm_channels;
    if (shm_channels) {
        shm_channels->prev = channel;
    }
    shm_channels = channel;
    
    return channel->channel_id;
}

// Attach to shared memory channel
i32 ipc_attach_shm_channel(u64 channel_id, void** addr) {
    shm_channel_t* channel = ipc_find_shm_channel(channel_id);
    if (!channel) {
        return ERR_NOT_FOUND;
    }
    
    // Check permissions
    if (!security_check_capability(get_current_pid(), channel->permissions, IPC_READ)) {
        return ERR_PERMISSION;
    }
    
    *addr = channel->shared_memory;
    channel->ref_count++;
    
    return ERR_SUCCESS;
}

// Send a signal
i32 ipc_send_signal(u64 receiver_pid, u64 signal_number) {
    signal_t* sig = (signal_t*)malloc(sizeof(signal_t));
    if (!sig) {
        return ERR_NO_MEMORY;
    }
    
    sig->signal_number = signal_number;
    sig->sender_pid = get_current_pid();
    sig->receiver_pid = receiver_pid;
    sig->timestamp = system_time;
    
    // Add to signal list
    sig->next = signals;
    signals = sig;
    
    // Wake up receiver
    scheduler_wake_task(receiver_pid);
    
    return ERR_SUCCESS;
}

// Handle priority inheritance
void ipc_handle_priority_inheritance(u64 task_pid, u64 resource_id, u32 operation) {
    u32 cpu_id = get_cpu_id();
    
    if (operation == 0) {  // Acquire resource
        pi_table[cpu_id].task_pid = task_pid;
        pi_table[cpu_id].resource_held = resource_id;
        pi_table[cpu_id].current_priority = get_task_priority(task_pid);
    } else {  // Release resource
        if (pi_table[cpu_id].resource_held == resource_id) {
            pi_table[cpu_id].resource_held = 0;
            pi_table[cpu_id].current_priority = pi_table[cpu_id].base_priority;
        }
    }
}

// Utility functions
message_queue_t* ipc_find_queue(u64 queue_id) {
    message_queue_t* queue = message_queues;
    while (queue && queue->queue_id != queue_id) {
        queue = queue->next;
    }
    return queue;
}

shm_channel_t* ipc_find_shm_channel(u64 channel_id) {
    shm_channel_t* channel = shm_channels;
    while (channel && channel->channel_id != channel_id) {
        channel = channel->next;
    }
    return channel;
}