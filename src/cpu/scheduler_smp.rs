/// SMP-aware Scheduler Implementation
/// 
/// Supports per-CPU run queues and load balancing

use crate::lib::spinlock::Spinlock;
use crate::cpu::percpu::{CpuInfo, MAX_CPUS};

/// Per-CPU scheduler run queue
pub struct PerCpuRunQueue {
    pub cpu_id: u32,
    pub tasks: [Option<TaskRef>; MAX_TASKS_PER_QUEUE],
    pub head: usize,
    pub tail: usize,
    pub count: usize,
}

pub const MAX_TASKS_PER_QUEUE: usize = 64;

#[derive(Debug, Clone, Copy)]
pub struct TaskRef {
    pub task_id: u64,
    pub priority: u8,
}

pub struct MultiCpuScheduler {
    run_queues: [Spinlock<PerCpuRunQueue>; MAX_CPUS],
    current_tasks: [Option<u64>; MAX_CPUS],
}

impl PerCpuRunQueue {
    pub fn new(cpu_id: u32) -> Self {
        PerCpuRunQueue {
            cpu_id,
            tasks: [None; MAX_TASKS_PER_QUEUE],
            head: 0,
            tail: 0,
            count: 0,
        }
    }

    pub fn enqueue(&mut self, task: TaskRef) -> bool {
        if self.count >= MAX_TASKS_PER_QUEUE {
            return false;  // Queue full
        }

        self.tasks[self.tail] = Some(task);
        self.tail = (self.tail + 1) % MAX_TASKS_PER_QUEUE;
        self.count += 1;
        true
    }

    pub fn dequeue(&mut self) -> Option<TaskRef> {
        if self.count == 0 {
            return None;
        }

        let task = self.tasks[self.head];
        self.tasks[self.head] = None;
        self.head = (self.head + 1) % MAX_TASKS_PER_QUEUE;
        self.count -= 1;
        task
    }

    pub fn peek(&self) -> Option<TaskRef> {
        self.tasks[self.head]
    }

    pub fn is_empty(&self) -> bool {
        self.count == 0
    }
}

impl MultiCpuScheduler {
    pub fn new() -> Self {
        let run_queues: [Spinlock<PerCpuRunQueue>; MAX_CPUS] = 
            core::array::from_fn(|i| Spinlock::new(PerCpuRunQueue::new(i as u32)));
        
        const NONE_TASK: Option<u64> = None;
        MultiCpuScheduler {
            run_queues,
            current_tasks: [NONE_TASK; MAX_CPUS],
        }
    }

    pub fn enqueue_task(&self, cpu_id: u32, task: TaskRef) -> bool {
        if (cpu_id as usize) < MAX_CPUS {
            let mut queue = self.run_queues[cpu_id as usize].lock();
            queue.enqueue(task)
        } else {
            false
        }
    }

    pub fn dequeue_task(&self, cpu_id: u32) -> Option<TaskRef> {
        if (cpu_id as usize) < MAX_CPUS {
            let mut queue = self.run_queues[cpu_id as usize].lock();
            queue.dequeue()
        } else {
            None
        }
    }

    pub fn get_next_task(&self, cpu_id: u32) -> Option<TaskRef> {
        if (cpu_id as usize) < MAX_CPUS {
            let queue = self.run_queues[cpu_id as usize].lock();
            queue.peek()
        } else {
            None
        }
    }

    pub fn set_current_task(&mut self, cpu_id: u32, task_id: u64) {
        if (cpu_id as usize) < MAX_CPUS {
            self.current_tasks[cpu_id as usize] = Some(task_id);
        }
    }

    pub fn get_current_task(&self, cpu_id: u32) -> Option<u64> {
        if (cpu_id as usize) < MAX_CPUS {
            self.current_tasks[cpu_id as usize]
        } else {
            None
        }
    }

    /// Load balancing: steal tasks from busier CPUs
    pub fn load_balance(&self, idle_cpu_id: u32) -> Option<TaskRef> {
        // Simple round-robin work stealing
        // Try to find a CPU with tasks and steal from it
        
        for i in 0..MAX_CPUS {
            if i as u32 == idle_cpu_id {
                continue;
            }

            let mut queue = self.run_queues[i].lock();
            if !queue.is_empty() {
                // Try to steal a task
                if let Some(task) = queue.dequeue() {
                    return Some(task);
                }
            }
        }
        
        None
    }

    pub fn rebalance_all(&self) {
        // Try to balance tasks across CPUs
        // This is a simplified version - more sophisticated algorithms exist
        
        let mut total_tasks = 0;
        let mut active_cpus = 0;

        // Count total tasks and active CPUs
        for i in 0..MAX_CPUS {
            let queue = self.run_queues[i].lock();
            if !queue.is_empty() {
                active_cpus += 1;
                total_tasks += queue.count;
            }
        }

        if active_cpus == 0 || total_tasks == 0 {
            return;
        }

        // Target tasks per CPU
        let target = (total_tasks + active_cpus - 1) / active_cpus;

        // Move tasks around to balance
        for i in 0..MAX_CPUS {
            let mut queue = self.run_queues[i].lock();
            if queue.count > target {
                // Too many tasks on this CPU
                // Would need to move excess to other CPUs
                // This is simplified - proper implementation would be more complex
            }
        }
    }
}

pub static mut MULTI_CPU_SCHEDULER: Option<MultiCpuScheduler> = None;

pub fn init_scheduler() {
    unsafe {
        MULTI_CPU_SCHEDULER = Some(MultiCpuScheduler::new());
    }
}

pub fn get_scheduler() -> Option<&'static MultiCpuScheduler> {
    unsafe {
        MULTI_CPU_SCHEDULER.as_ref()
    }
}
