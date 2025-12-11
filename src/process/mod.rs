pub mod scheduler;
pub mod context;
pub mod thread;
pub mod elf_loader;

use spin::Mutex;
use crate::memory::{VirtAddr, frame_allocator::Frame};
use crate::security::{self, SecurityContext};

#[cfg(not(test))]
use alloc::vec::Vec;
#[cfg(not(test))]
use alloc::string::String;

#[cfg(test)]
extern crate std;
#[cfg(test)]
use std::vec::Vec;
#[cfg(test)]
use std::string::String;

pub use thread::{Thread, ThreadState, THREAD_TABLE, create_thread};
pub use context::Context;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct ProcessId(pub usize);

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct ThreadId(pub usize);

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Priority {
    Idle = 0,
    Low = 1,
    Normal = 2,
    High = 3,
    Realtime = 4,
}

impl Priority {
    pub fn as_usize(&self) -> usize {
        *self as usize
    }
}

#[derive(Debug, Clone)]
pub struct FileDescriptorEntry {
    pub fd: u32,
    pub flags: u32,
    pub inheritable: bool,
}

#[derive(Debug, Clone)]
pub struct FileDescriptorTable {
    entries: Vec<FileDescriptorEntry>,
    next_fd: u32,
}

impl FileDescriptorTable {
    pub fn new() -> Self {
        Self::default()
    }

    pub fn inherit(&self) -> Self {
        let entries = self
            .entries
            .iter()
            .filter(|entry| entry.inheritable)
            .cloned()
            .collect();
        Self {
            entries,
            next_fd: self.next_fd,
        }
    }

    pub fn iter(&self) -> impl Iterator<Item = &FileDescriptorEntry> {
        self.entries.iter()
    }

    pub fn insert(&mut self, fd: u32, flags: u32, inheritable: bool) {
        self.next_fd = self.next_fd.max(fd + 1);
        if let Some(existing) = self.entries.iter_mut().find(|entry| entry.fd == fd) {
            existing.flags = flags;
            existing.inheritable = inheritable;
        } else {
            self.entries.push(FileDescriptorEntry { fd, flags, inheritable });
        }
    }
}

impl Default for FileDescriptorTable {
    fn default() -> Self {
        let mut table = FileDescriptorTable {
            entries: Vec::new(),
            next_fd: 0,
        };
        table.insert(0, 0, true);
        table.insert(1, 0, true);
        table.insert(2, 0, true);
        table
    }
}

#[derive(Debug, Clone)]
pub struct AddressSpace {
    pub page_table_frame: Frame,
    pub heap_start: VirtAddr,
    pub heap_end: VirtAddr,
    pub stack_start: VirtAddr,
    pub stack_end: VirtAddr,
}

impl AddressSpace {
    pub fn new(page_table_frame: Frame) -> Self {
        Self {
            page_table_frame,
            heap_start: VirtAddr::new(0x0000_1000_0000_0000),
            heap_end: VirtAddr::new(0x0000_1000_1000_0000),
            stack_start: VirtAddr::new(0x0000_7FFF_FFFF_0000),
            stack_end: VirtAddr::new(0x0000_8000_0000_0000),
        }
    }

    pub fn clone_for_fork(&self) -> Option<Self> {
        let frame = crate::memory::frame_allocator::allocate_frame()?;
        Some(Self {
            page_table_frame: frame,
            heap_start: self.heap_start,
            heap_end: self.heap_end,
            stack_start: self.stack_start,
            stack_end: self.stack_end,
        })
    }
}

#[derive(Debug, Clone)]
pub struct Process {
    pub id: ProcessId,
    pub name: String,
    pub address_space: AddressSpace,
    pub threads: Vec<ThreadId>,
    pub parent: Option<ProcessId>,
    pub children: Vec<ProcessId>,
    pub file_descriptors: FileDescriptorTable,
    pub security: SecurityContext,
    pub image: Option<elf_loader::LoadedImage>,
}

impl Process {
    pub fn new(
        id: ProcessId,
        name: String,
        address_space: AddressSpace,
        security: SecurityContext,
        file_descriptors: FileDescriptorTable,
    ) -> Self {
        Self {
            id,
            name,
            address_space,
            threads: Vec::new(),
            parent: None,
            children: Vec::new(),
            file_descriptors,
            security,
            image: None,
        }
    }

    pub fn add_thread(&mut self, thread_id: ThreadId) {
        self.threads.push(thread_id);
    }

    pub fn remove_thread(&mut self, thread_id: ThreadId) {
        self.threads.retain(|&id| id != thread_id);
    }
}

pub struct ProcessTable {
    processes: Vec<Option<Process>>,
    next_pid: usize,
}

impl ProcessTable {
    pub const fn new() -> Self {
        Self {
            processes: Vec::new(),
            next_pid: 1,
        }
    }

    pub fn allocate_pid(&mut self) -> ProcessId {
        let pid = ProcessId(self.next_pid);
        self.next_pid += 1;
        pid
    }

    pub fn add_process(&mut self, process: Process) {
        let id = process.id.0;
        while self.processes.len() <= id {
            self.processes.push(None);
        }
        self.processes[id] = Some(process);
    }

    pub fn get_process(&self, pid: ProcessId) -> Option<&Process> {
        self.processes.get(pid.0)?.as_ref()
    }

    pub fn get_process_mut(&mut self, pid: ProcessId) -> Option<&mut Process> {
        self.processes.get_mut(pid.0)?.as_mut()
    }

    pub fn remove_process(&mut self, pid: ProcessId) -> Option<Process> {
        if pid.0 < self.processes.len() {
            if let Some(process) = self.processes[pid.0].take() {
                security::remove_context(pid.0);
                Some(process)
            } else {
                None
            }
        } else {
            None
        }
    }
}

pub static PROCESS_TABLE: Mutex<ProcessTable> = Mutex::new(ProcessTable::new());

pub fn create_process(
    name: String,
    page_table_frame: Frame,
    security: SecurityContext,
    file_descriptors: FileDescriptorTable,
) -> Option<ProcessId> {
    let mut table = PROCESS_TABLE.lock();
    let pid = table.allocate_pid();
    let address_space = AddressSpace::new(page_table_frame);
    let process = Process::new(pid, name, address_space, security.clone(), file_descriptors);
    security::register_process(pid.0, security);
    table.add_process(process);
    Some(pid)
}

pub fn get_process(pid: ProcessId) -> Option<Process> {
    let table = PROCESS_TABLE.lock();
    table.get_process(pid).cloned()
}

pub fn set_process_capabilities(pid: ProcessId, capabilities: security::CapMask) -> bool {
    let mut table = PROCESS_TABLE.lock();
    let process = match table.get_process_mut(pid) {
        Some(process) => process,
        None => return false,
    };

    process.security.capabilities = capabilities;
    security::set_capabilities(pid.0, capabilities);
    true
}

pub fn grant_process_capabilities(pid: ProcessId, mask: security::CapMask) -> bool {
    let mut table = PROCESS_TABLE.lock();
    let process = match table.get_process_mut(pid) {
        Some(process) => process,
        None => return false,
    };

    process.security.capabilities |= mask;
    security::grant_capabilities(pid.0, mask);
    true
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_process_creation() {
        let mut table = ProcessTable::new();
        let pid = table.allocate_pid();
        assert_eq!(pid.0, 1);

        let pid2 = table.allocate_pid();
        assert_eq!(pid2.0, 2);
    }

    #[test]
    fn test_priority() {
        assert!(Priority::High.as_usize() > Priority::Normal.as_usize());
        assert!(Priority::Realtime.as_usize() > Priority::High.as_usize());
    }
}
