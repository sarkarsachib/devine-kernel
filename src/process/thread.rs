use super::{ProcessId, ThreadId, Priority, Context};
use crate::memory::VirtAddr;
use spin::Mutex;

#[cfg(not(test))]
use alloc::vec::Vec;

#[cfg(test)]
extern crate std;
#[cfg(test)]
use std::vec::Vec;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum ThreadState {
    Ready,
    Running,
    Blocked,
    Sleeping,
    Terminated,
}

#[derive(Debug, Clone)]
pub struct Thread {
    pub id: ThreadId,
    pub process_id: ProcessId,
    pub state: ThreadState,
    pub priority: Priority,
    pub context: Context,
    pub kernel_stack: VirtAddr,
    pub user_stack: Option<VirtAddr>,
    pub time_slice: usize,
    pub total_cpu_time: u64,
}

impl Thread {
    pub fn new(
        id: ThreadId,
        process_id: ProcessId,
        priority: Priority,
        entry_point: VirtAddr,
        kernel_stack: VirtAddr,
        user_stack: Option<VirtAddr>,
    ) -> Self {
        let context = if let Some(user_sp) = user_stack {
            Context::new_user(entry_point, user_sp)
        } else {
            Context::new_kernel(entry_point, kernel_stack)
        };

        Self {
            id,
            process_id,
            state: ThreadState::Ready,
            priority,
            context,
            kernel_stack,
            user_stack,
            time_slice: Self::calculate_time_slice(priority),
            total_cpu_time: 0,
        }
    }

    fn calculate_time_slice(priority: Priority) -> usize {
        match priority {
            Priority::Idle => 1,
            Priority::Low => 5,
            Priority::Normal => 10,
            Priority::High => 20,
            Priority::Realtime => 50,
        }
    }

    pub fn is_runnable(&self) -> bool {
        self.state == ThreadState::Ready || self.state == ThreadState::Running
    }

    pub fn set_state(&mut self, state: ThreadState) {
        self.state = state;
    }

    pub fn increment_cpu_time(&mut self, ticks: u64) {
        self.total_cpu_time += ticks;
    }
}

pub struct ThreadTable {
    threads: Vec<Option<Thread>>,
    next_tid: usize,
}

impl ThreadTable {
    pub const fn new() -> Self {
        Self {
            threads: Vec::new(),
            next_tid: 1,
        }
    }

    pub fn allocate_tid(&mut self) -> ThreadId {
        let tid = ThreadId(self.next_tid);
        self.next_tid += 1;
        tid
    }

    pub fn add_thread(&mut self, thread: Thread) {
        let id = thread.id.0;
        while self.threads.len() <= id {
            self.threads.push(None);
        }
        self.threads[id] = Some(thread);
    }

    pub fn get_thread(&self, tid: ThreadId) -> Option<&Thread> {
        self.threads.get(tid.0)?.as_ref()
    }

    pub fn get_thread_mut(&mut self, tid: ThreadId) -> Option<&mut Thread> {
        self.threads.get_mut(tid.0)?.as_mut()
    }

    pub fn remove_thread(&mut self, tid: ThreadId) -> Option<Thread> {
        if tid.0 < self.threads.len() {
            self.threads[tid.0].take()
        } else {
            None
        }
    }

    pub fn all_threads(&self) -> impl Iterator<Item = &Thread> {
        self.threads.iter().filter_map(|t| t.as_ref())
    }

    pub fn all_threads_mut(&mut self) -> impl Iterator<Item = &mut Thread> {
        self.threads.iter_mut().filter_map(|t| t.as_mut())
    }
}

pub static THREAD_TABLE: Mutex<ThreadTable> = Mutex::new(ThreadTable::new());

pub fn create_thread(
    process_id: ProcessId,
    priority: Priority,
    entry_point: VirtAddr,
    kernel_stack: VirtAddr,
    user_stack: Option<VirtAddr>,
) -> Option<ThreadId> {
    let mut table = THREAD_TABLE.lock();
    let tid = table.allocate_tid();
    let thread = Thread::new(tid, process_id, priority, entry_point, kernel_stack, user_stack);
    table.add_thread(thread);
    
    if let Some(process) = super::PROCESS_TABLE.lock().get_process_mut(process_id) {
        process.add_thread(tid);
    }
    
    Some(tid)
}

pub fn get_thread(tid: ThreadId) -> Option<Thread> {
    let table = THREAD_TABLE.lock();
    table.get_thread(tid).cloned()
}

pub fn set_thread_state(tid: ThreadId, state: ThreadState) {
    if let Some(thread) = THREAD_TABLE.lock().get_thread_mut(tid) {
        thread.set_state(state);
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_thread_creation() {
        let mut table = ThreadTable::new();
        let tid = table.allocate_tid();
        assert_eq!(tid.0, 1);

        let thread = Thread::new(
            tid,
            ProcessId(1),
            Priority::Normal,
            VirtAddr::new(0x1000),
            VirtAddr::new(0x2000),
            Some(VirtAddr::new(0x3000)),
        );

        assert_eq!(thread.state, ThreadState::Ready);
        assert!(thread.is_runnable());
    }

    #[test]
    fn test_time_slice() {
        let thread = Thread::new(
            ThreadId(1),
            ProcessId(1),
            Priority::High,
            VirtAddr::new(0x1000),
            VirtAddr::new(0x2000),
            None,
        );
        assert!(thread.time_slice > 10);
    }
}
