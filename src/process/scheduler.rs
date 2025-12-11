use super::{ThreadId, Priority, ThreadState, THREAD_TABLE};
use spin::Mutex;

#[cfg(not(test))]
use alloc::collections::VecDeque;

#[cfg(test)]
extern crate std;
#[cfg(test)]
use std::collections::VecDeque;

pub struct RunQueue {
    queues: [VecDeque<ThreadId>; 5],
    current_thread: Option<ThreadId>,
    idle_thread: Option<ThreadId>,
}

impl RunQueue {
    pub const fn new() -> Self {
        Self {
            queues: [
                VecDeque::new(),
                VecDeque::new(),
                VecDeque::new(),
                VecDeque::new(),
                VecDeque::new(),
            ],
            current_thread: None,
            idle_thread: None,
        }
    }

    pub fn enqueue(&mut self, tid: ThreadId, priority: Priority) {
        let queue_idx = priority.as_usize();
        if !self.queues[queue_idx].contains(&tid) {
            self.queues[queue_idx].push_back(tid);
        }
    }

    pub fn dequeue(&mut self, priority: Priority) -> Option<ThreadId> {
        let queue_idx = priority.as_usize();
        self.queues[queue_idx].pop_front()
    }

    pub fn pick_next(&mut self) -> Option<ThreadId> {
        for i in (0..5).rev() {
            if let Some(tid) = self.queues[i].pop_front() {
                return Some(tid);
            }
        }
        
        self.idle_thread
    }

    pub fn set_current(&mut self, tid: Option<ThreadId>) {
        self.current_thread = tid;
    }

    pub fn current(&self) -> Option<ThreadId> {
        self.current_thread
    }

    pub fn set_idle_thread(&mut self, tid: ThreadId) {
        self.idle_thread = Some(tid);
    }

    pub fn remove(&mut self, tid: ThreadId) {
        for queue in &mut self.queues {
            queue.retain(|&t| t != tid);
        }
    }
}

pub struct Scheduler {
    run_queue: RunQueue,
    time_slice_remaining: usize,
    total_ticks: u64,
}

impl Scheduler {
    pub const fn new() -> Self {
        Self {
            run_queue: RunQueue::new(),
            time_slice_remaining: 0,
            total_ticks: 0,
        }
    }

    pub fn add_thread(&mut self, tid: ThreadId) {
        let thread_table = THREAD_TABLE.lock();
        if let Some(thread) = thread_table.get_thread(tid) {
            if thread.is_runnable() {
                self.run_queue.enqueue(tid, thread.priority);
            }
        }
    }

    pub fn remove_thread(&mut self, tid: ThreadId) {
        self.run_queue.remove(tid);
    }

    pub fn set_idle_thread(&mut self, tid: ThreadId) {
        self.run_queue.set_idle_thread(tid);
    }

    pub fn schedule(&mut self) -> Option<ThreadId> {
        let current = self.run_queue.current();
        
        if self.time_slice_remaining > 0 {
            if let Some(current_tid) = current {
                let thread_table = THREAD_TABLE.lock();
                if let Some(thread) = thread_table.get_thread(current_tid) {
                    if thread.state == ThreadState::Running && thread.is_runnable() {
                        return Some(current_tid);
                    }
                }
            }
        }

        if let Some(current_tid) = current {
            let mut thread_table = THREAD_TABLE.lock();
            if let Some(thread) = thread_table.get_thread_mut(current_tid) {
                if thread.state == ThreadState::Running {
                    thread.set_state(ThreadState::Ready);
                    self.run_queue.enqueue(current_tid, thread.priority);
                }
            }
        }

        let next_tid = self.run_queue.pick_next()?;
        
        let mut thread_table = THREAD_TABLE.lock();
        if let Some(thread) = thread_table.get_thread_mut(next_tid) {
            thread.set_state(ThreadState::Running);
            self.time_slice_remaining = thread.time_slice;
        }

        self.run_queue.set_current(Some(next_tid));
        Some(next_tid)
    }

    pub fn tick(&mut self) {
        self.total_ticks += 1;
        
        if self.time_slice_remaining > 0 {
            self.time_slice_remaining -= 1;
        }

        if let Some(current_tid) = self.run_queue.current() {
            let mut thread_table = THREAD_TABLE.lock();
            if let Some(thread) = thread_table.get_thread_mut(current_tid) {
                thread.increment_cpu_time(1);
            }
        }
    }

    pub fn yield_current(&mut self) {
        self.time_slice_remaining = 0;
    }

    pub fn block_current(&mut self) {
        if let Some(current_tid) = self.run_queue.current() {
            let mut thread_table = THREAD_TABLE.lock();
            if let Some(thread) = thread_table.get_thread_mut(current_tid) {
                thread.set_state(ThreadState::Blocked);
            }
            self.run_queue.set_current(None);
        }
    }

    pub fn unblock_thread(&mut self, tid: ThreadId) {
        let mut thread_table = THREAD_TABLE.lock();
        if let Some(thread) = thread_table.get_thread_mut(tid) {
            if thread.state == ThreadState::Blocked {
                thread.set_state(ThreadState::Ready);
                self.run_queue.enqueue(tid, thread.priority);
            }
        }
    }

    pub fn current_thread(&self) -> Option<ThreadId> {
        self.run_queue.current()
    }

    pub fn total_ticks(&self) -> u64 {
        self.total_ticks
    }
}

pub static SCHEDULER: Mutex<Scheduler> = Mutex::new(Scheduler::new());

pub fn init_scheduler() {
    
}

pub fn add_thread(tid: ThreadId) {
    SCHEDULER.lock().add_thread(tid);
}

pub fn remove_thread(tid: ThreadId) {
    SCHEDULER.lock().remove_thread(tid);
}

pub fn schedule() -> Option<ThreadId> {
    SCHEDULER.lock().schedule()
}

pub fn tick() {
    SCHEDULER.lock().tick();
}

pub fn yield_cpu() {
    SCHEDULER.lock().yield_current();
}

pub fn block_current_thread() {
    SCHEDULER.lock().block_current();
}

pub fn unblock_thread(tid: ThreadId) {
    SCHEDULER.lock().unblock_thread(tid);
}

pub fn current_thread() -> Option<ThreadId> {
    SCHEDULER.lock().current_thread()
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::process::{Priority, ProcessId};
    use crate::memory::VirtAddr;

    #[test]
    fn test_run_queue() {
        let mut queue = RunQueue::new();
        queue.enqueue(ThreadId(1), Priority::Normal);
        queue.enqueue(ThreadId(2), Priority::High);
        queue.enqueue(ThreadId(3), Priority::Low);

        let next = queue.pick_next();
        assert_eq!(next, Some(ThreadId(2)));

        let next = queue.pick_next();
        assert_eq!(next, Some(ThreadId(1)));
    }

    #[test]
    fn test_scheduler_tick() {
        let mut scheduler = Scheduler::new();
        let initial_ticks = scheduler.total_ticks();
        scheduler.tick();
        assert_eq!(scheduler.total_ticks(), initial_ticks + 1);
    }
}
