/* Task Scheduler with Priority Support */

#include "../../include/types.h"

// Task control block
typedef struct task_control_block {
    u64 task_id;
    char name[MAX_STRING_LEN];
    u64 state;              // 0=ready, 1=running, 2=waiting, 3=blocked
    u64 priority;           // Task priority (0=highest, 255=lowest)
    u64 time_slice;         // Current time slice remaining
    u64 total_time;         // Total CPU time used
    u64 wake_time;          // Time to wake up (for sleeping tasks)
    
    // Task context
    u64 registers[16];      // General purpose registers
    u64 stack_pointer;
    u64 instruction_pointer;
    u64 flags;
    
    // Task memory
    void* stack_base;
    void* stack_top;
    u64 page_table;
    
    // Security context
    u64 user_id;
    u64 group_id;
    u64 privilege_level;
    
    // IPC state
    u64 message_queue;
    u64 waiting_for;
    
    // Link list
    struct task_control_block* next;
    struct task_control_block* prev;
} tcb_t;

// Scheduler state
static tcb_t* ready_queue = NULL;
static tcb_t* waiting_queue = NULL;
static tcb_t* current_task = NULL;
static tcb_t* idle_task = NULL;
static u64 next_task_id = 1;
static u64 system_ticks = 0;

// Scheduler constants
#define TIME_SLICE_TICKS 10
#define MAX_PRIORITY 255
#define MIN_PRIORITY 0

// Task states
#define TASK_READY    0
#define TASK_RUNNING  1
#define TASK_WAITING  2
#define TASK_BLOCKED  3
#define TASK_TERMINATED 4

// Initialize scheduler
void scheduler_init(void) {
    console_print("Creating idle task... ");
    idle_task = scheduler_create_task("idle", MIN_PRIORITY);
    idle_task->state = TASK_READY;
    console_print("OK\n");
    
    console_print("Creating init task... ");
    tcb_t* init_task = scheduler_create_task("init", MIN_PRIORITY);
    scheduler_add_to_queue(init_task);
    console_print("OK\n");
    
    current_task = idle_task;
}

// Create a new task
tcb_t* scheduler_create_task(const char* name, u64 priority) {
    tcb_t* task = (tcb_t*)malloc(sizeof(tcb_t));
    if (!task) {
        return NULL;
    }
    
    task->task_id = next_task_id++;
    strncpy(task->name, name, MAX_STRING_LEN - 1);
    task->name[MAX_STRING_LEN - 1] = '\0';
    task->state = TASK_READY;
    task->priority = priority;
    task->time_slice = TIME_SLICE_TICKS;
    task->total_time = 0;
    task->wake_time = 0;
    
    // Initialize registers
    for (u32 i = 0; i < 16; i++) {
        task->registers[i] = 0;
    }
    
    // Setup stack
    task->stack_base = malloc(STACK_SIZE);
    task->stack_top = (void*)((u64)task->stack_base + STACK_SIZE);
    task->stack_pointer = (u64)task->stack_top;
    task->instruction_pointer = 0;
    task->flags = 0x2; // Interrupts enabled
    
    // Setup security context
    task->user_id = 0;
    task->group_id = 0;
    task->privilege_level = 3; // User mode by default
    
    // Setup IPC state
    task->message_queue = 0;
    task->waiting_for = 0;
    
    task->next = NULL;
    task->prev = NULL;
    
    return task;
}

// Add task to ready queue
void scheduler_add_to_queue(tcb_t* task) {
    if (!task) return;
    
    task->state = TASK_READY;
    task->next = ready_queue;
    if (ready_queue) {
        ready_queue->prev = task;
    }
    ready_queue = task;
}

// Remove task from ready queue
void scheduler_remove_from_queue(tcb_t* task) {
    if (!task || !ready_queue) return;
    
    if (task->prev) {
        task->prev->next = task->next;
    } else {
        ready_queue = task->next;
    }
    
    if (task->next) {
        task->next->prev = task->prev;
    }
    
    task->prev = task->next = NULL;
}

// Scheduler tick handler
void scheduler_tick(void) {
    system_ticks++;
    
    // Update current task's time slice
    if (current_task) {
        current_task->time_slice--;
        current_task->total_time++;
        
        // Check if time slice expired
        if (current_task->time_slice == 0) {
            scheduler_yield();
        }
    }
}

// Schedule next task
void scheduler_schedule(void) {
    tcb_t* next_task = scheduler_select_next_task();
    if (next_task) {
        scheduler_switch_to(next_task);
    }
}

// Select next task to run
tcb_t* scheduler_select_next_task(void) {
    tcb_t* task = ready_queue;
    tcb_t* highest_priority = NULL;
    
    // Find highest priority ready task
    while (task) {
        if (!highest_priority || task->priority < highest_priority->priority) {
            highest_priority = task;
        }
        task = task->next;
    }
    
    // If no ready tasks, use idle task
    return highest_priority ? highest_priority : idle_task;
}

// Yield CPU
void scheduler_yield(void) {
    if (current_task && current_task->state == TASK_RUNNING) {
        current_task->state = TASK_READY;
        current_task->time_slice = TIME_SLICE_TICKS;
        
        if (current_task != idle_task) {
            scheduler_add_to_queue(current_task);
        }
    }
    
    scheduler_schedule();
}

// Switch to another task
void scheduler_switch_to(tcb_t* task) {
    if (current_task == task) {
        return; // Already running
    }
    
    tcb_t* old_task = current_task;
    current_task = task;
    
    // Update task state
    if (old_task && old_task->state == TASK_RUNNING) {
        old_task->state = TASK_READY;
    }
    
    task->state = TASK_RUNNING;
    task->time_slice = TIME_SLICE_TICKS;
    
    // Switch context
    if (old_task) {
        scheduler_save_context(old_task);
    }
    
    scheduler_restore_context(task);
}

// Save current task context
void scheduler_save_context(tcb_t* task) {
    // Save general purpose registers
    asm volatile("movq %%rax, %0" : "=m"(task->registers[0]));
    asm volatile("movq %%rbx, %0" : "=m"(task->registers[1]));
    asm volatile("movq %%rcx, %0" : "=m"(task->registers[2]));
    asm volatile("movq %%rdx, %0" : "=m"(task->registers[3]));
    asm volatile("movq %%rsi, %0" : "=m"(task->registers[4]));
    asm volatile("movq %%rdi, %0" : "=m"(task->registers[5]));
    asm volatile("movq %%rbp, %0" : "=m"(task->registers[6]));
    asm volatile("movq %%r8, %0"  : "=m"(task->registers[7]));
    asm volatile("movq %%r9, %0"  : "=m"(task->registers[8]));
    asm volatile("movq %%r10, %0" : "=m"(task->registers[9]));
    asm volatile("movq %%r11, %0" : "=m"(task->registers[10]));
    asm volatile("movq %%r12, %0" : "=m"(task->registers[11]));
    asm volatile("movq %%r13, %0" : "=m"(task->registers[12]));
    asm volatile("movq %%r14, %0" : "=m"(task->registers[13]));
    asm volatile("movq %%r15, %0" : "=m"(task->registers[14]));
    
    // Save stack pointer and instruction pointer
    asm volatile("movq %%rsp, %0" : "=m"(task->stack_pointer));
    asm volatile("pushfq");
    asm volatile("popq %0" : "=m"(task->flags));
    // Note: IP is saved by interrupt/exception mechanism
}

// Restore task context
void scheduler_restore_context(tcb_t* task) {
    // Restore general purpose registers
    asm volatile("movq %0, %%rax" : : "m"(task->registers[0]));
    asm volatile("movq %0, %%rbx" : : "m"(task->registers[1]));
    asm volatile("movq %0, %%rcx" : : "m"(task->registers[2]));
    asm volatile("movq %0, %%rdx" : : "m"(task->registers[3]));
    asm volatile("movq %0, %%rsi" : : "m"(task->registers[4]));
    asm volatile("movq %0, %%rdi" : : "m"(task->registers[5]));
    asm volatile("movq %0, %%rbp" : : "m"(task->registers[6]));
    asm volatile("movq %0, %%r8"  : : "m"(task->registers[7]));
    asm volatile("movq %0, %%r9"  : : "m"(task->registers[8]));
    asm volatile("movq %0, %%r10" : : "m"(task->registers[9]));
    asm volatile("movq %0, %%r11" : : "m"(task->registers[10]));
    asm volatile("movq %0, %%r12" : : "m"(task->registers[11]));
    asm volatile("movq %0, %%r13" : : "m"(task->registers[12]));
    asm volatile("movq %0, %%r14" : : "m"(task->registers[13]));
    asm volatile("movq %0, %%r15" : : "m"(task->registers[14]));
    
    // Restore stack pointer
    asm volatile("movq %0, %%rsp" : : "m"(task->stack_pointer));
    asm volatile("pushq %0" : : "m"(task->flags));
    asm volatile("popfq");
}

// Make task sleep
void scheduler_sleep_task(u64 task_id) {
    tcb_t* task = scheduler_find_task(task_id);
    if (task && task->state == TASK_RUNNING) {
        task->state = TASK_WAITING;
        task->wake_time = system_ticks + 100; // Sleep for 100 ticks
    }
}

// Wake up a task
void scheduler_wake_task(u64 task_id) {
    tcb_t* task = scheduler_find_task(task_id);
    if (task && task->state == TASK_WAITING) {
        task->state = TASK_READY;
        task->wake_time = 0;
        scheduler_add_to_queue(task);
    }
}

// Get current task ID
u64 get_current_pid(void) {
    return current_task ? current_task->task_id : 0;
}

// Get current task
tcb_t* get_current_task(void) {
    return current_task;
}

// Get task by ID
tcb_t* scheduler_find_task(u64 task_id) {
    tcb_t* task = ready_queue;
    while (task && task->task_id != task_id) {
        task = task->next;
    }
    return task;
}

// Get task priority
u64 get_task_priority(u64 task_id) {
    tcb_t* task = scheduler_find_task(task_id);
    return task ? task->priority : MAX_PRIORITY;
}

// Start the scheduler
void scheduler_start(void) {
    console_print("Starting scheduler...\n");
    scheduler_schedule();
}

// Check for tasks to wake up
void scheduler_check_wakeups(void) {
    tcb_t* task = waiting_queue;
    while (task) {
        if (task->wake_time && task->wake_time <= system_ticks) {
            scheduler_wake_task(task->task_id);
        }
        task = task->next;
    }
}