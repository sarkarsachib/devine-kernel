#[cfg(not(test))]
extern crate alloc;

use alloc::{collections::VecDeque, string::String, vec::Vec};
use core::{slice, str};

use spin::Mutex;

use crate::memory::{VirtAddr, frame_allocator::allocate_frame, paging::Page};
use crate::process::{
    self,
    create_process,
    create_thread,
    scheduler,
    thread::{self, ThreadState, THREAD_TABLE},
    FileDescriptorTable,
    ProcessId,
    ThreadId,
    Context,
};
use crate::process::elf_loader::{self, TargetArch};
use crate::userspace;

pub const SYS_EXIT: usize = 0;
pub const SYS_FORK: usize = 1;
pub const SYS_EXEC: usize = 2;
pub const SYS_WAIT: usize = 3;
pub const SYS_GETPID: usize = 4;
pub const SYS_MMAP: usize = 5;
pub const SYS_MUNMAP: usize = 6;
pub const SYS_BRK: usize = 7;
pub const SYS_CLONE: usize = 8;
pub const SYS_WRITE: usize = 9;
pub const SYS_READ: usize = 10;

#[derive(Debug)]
pub enum SyscallError {
    InvalidSyscall,
    InvalidArgument,
    OutOfMemory,
    ProcessNotFound,
    PermissionDenied,
}

pub type SyscallResult = Result<usize, SyscallError>;

struct ZombieChild {
    parent: ProcessId,
    child: ProcessId,
    status: usize,
}

struct Waiter {
    parent: ProcessId,
    tid: ThreadId,
    target: Option<ProcessId>,
}

static USER_STDOUT: Mutex<Vec<u8>> = Mutex::new(Vec::new());
static USER_STDIN: Mutex<VecDeque<u8>> = Mutex::new(VecDeque::new());
static ZOMBIE_CHILDREN: Mutex<Vec<ZombieChild>> = Mutex::new(Vec::new());
static WAITERS: Mutex<Vec<Waiter>> = Mutex::new(Vec::new());

pub fn handle_syscall(
    syscall_number: usize,
    arg1: usize,
    arg2: usize,
    arg3: usize,
    arg4: usize,
    arg5: usize,
    arg6: usize,
) -> SyscallResult {
    match syscall_number {
        SYS_EXIT => sys_exit(arg1),
        SYS_FORK => sys_fork(),
        SYS_EXEC => sys_exec(arg1, arg2),
        SYS_WAIT => sys_wait(arg1),
        SYS_GETPID => sys_getpid(),
        SYS_MMAP => sys_mmap(arg1, arg2, arg3, arg4, arg5, arg6),
        SYS_MUNMAP => sys_munmap(arg1, arg2),
        SYS_BRK => sys_brk(arg1),
        SYS_CLONE => sys_clone(arg1, arg2, arg3),
        SYS_WRITE => sys_write(arg1, arg2, arg3),
        SYS_READ => sys_read(arg1, arg2, arg3),
        _ => Err(SyscallError::InvalidSyscall),
    }
}

fn sys_exit(exit_code: usize) -> SyscallResult {
    let (tid, thread) = current_thread_snapshot()?;
    finalize_thread(tid, thread, exit_code);
    Ok(exit_code)
}

fn sys_fork() -> SyscallResult {
    let (_, thread) = current_thread_snapshot()?;
    let parent_process = process::get_process(thread.process_id).ok_or(SyscallError::ProcessNotFound)?;

    let new_page_frame = allocate_frame().ok_or(SyscallError::OutOfMemory)?;
    let child_security = parent_process.security.clone();
    let child_fds = parent_process.file_descriptors.inherit();

    let child_pid = create_process(
        parent_process.name.clone(),
        new_page_frame,
        child_security,
        child_fds,
    )
    .ok_or(SyscallError::OutOfMemory)?;

    {
        let mut table = process::PROCESS_TABLE.lock();
        if let Some(child) = table.get_process_mut(child_pid) {
            child.parent = Some(parent_process.id);
            child.image = parent_process.image.clone();
        }
        if let Some(parent) = table.get_process_mut(parent_process.id) {
            parent.children.push(child_pid);
        }
    }

    let child_stack = allocate_frame().ok_or(SyscallError::OutOfMemory)?;
    let child_tid = create_thread(
        child_pid,
        thread.priority,
        VirtAddr::new(thread.context.rip),
        VirtAddr::new(child_stack.start_address.0),
        thread.user_stack,
    )
    .ok_or(SyscallError::OutOfMemory)?;

    scheduler::add_thread(child_tid);
    Ok(child_pid.0)
}

fn sys_exec(path_ptr: usize, argv_ptr: usize) -> SyscallResult {
    let (tid, thread) = current_thread_snapshot()?;
    let arch = current_arch();

    let path = if path_ptr == 0 {
        String::from("/sbin/init")
    } else {
        read_user_cstring(path_ptr)?
    };

    let mut argv = read_user_string_array(argv_ptr)?;
    if argv.is_empty() {
        argv.push(path.clone());
    }
    let argv_refs = argv.iter().map(|arg| arg.as_str()).collect::<Vec<_>>();

    let image = userspace::lookup(&path, arch).ok_or(SyscallError::ProcessNotFound)?;

    let loaded = {
        let mut table = process::PROCESS_TABLE.lock();
        let process = table
            .get_process_mut(thread.process_id)
            .ok_or(SyscallError::ProcessNotFound)?;
        let loaded = elf_loader::load_executable(
            image,
            arch,
            &process.address_space,
            &argv_refs,
            &[],
        )
        .map_err(|_| SyscallError::InvalidArgument)?;
        process.name = path;
        process.image = Some(loaded.clone());
        loaded
    };

    let mut threads = THREAD_TABLE.lock();
    if let Some(current) = threads.get_thread_mut(tid) {
        current.context = Context::new_user(loaded.entry_point, loaded.stack.user_sp);
        current.user_stack = Some(loaded.stack.user_sp);
    }

    Ok(0)
}

fn sys_wait(pid: usize) -> SyscallResult {
    let (tid, thread) = current_thread_snapshot()?;
    if let Some(zombie) = take_zombie(thread.process_id, pid) {
        return Ok(zombie.child.0);
    }

    register_waiter(thread.process_id, tid, pid);
    scheduler::block_current_thread();
    Ok(0)
}

fn sys_getpid() -> SyscallResult {
    let (_, thread) = current_thread_snapshot()?;
    Ok(thread.process_id.0)
}

fn sys_mmap(
    addr: usize,
    length: usize,
    _prot: usize,
    _flags: usize,
    _fd: usize,
    _offset: usize,
) -> SyscallResult {
    if length == 0 {
        return Err(SyscallError::InvalidArgument);
    }

    let start_page = Page::containing_address(VirtAddr::new(addr as u64));
    let num_pages = (length + 4095) / 4096;

    for i in 0..num_pages {
        let _page = Page::containing_address(VirtAddr::new(start_page.start_address.0 + (i * 4096) as u64));
        let _frame = allocate_frame().ok_or(SyscallError::OutOfMemory)?;
    }

    Ok(addr)
}

fn sys_munmap(_addr: usize, _length: usize) -> SyscallResult {
    Ok(0)
}

fn sys_brk(addr: usize) -> SyscallResult {
    Ok(addr)
}

fn sys_clone(_flags: usize, stack: usize, _parent_tid: usize) -> SyscallResult {
    let (_, thread) = current_thread_snapshot()?;

    let new_stack = if stack == 0 {
        thread.kernel_stack
    } else {
        VirtAddr::new(stack as u64)
    };

    let new_tid = create_thread(
        thread.process_id,
        thread.priority,
        VirtAddr::new(thread.context.rip),
        new_stack,
        thread.user_stack,
    )
    .ok_or(SyscallError::OutOfMemory)?;

    scheduler::add_thread(new_tid);
    Ok(new_tid.0)
}

fn sys_write(fd: usize, buf: usize, len: usize) -> SyscallResult {
    if len == 0 {
        return Ok(0);
    }

    let data = unsafe { slice::from_raw_parts(buf as *const u8, len) };
    match fd {
        1 | 2 => {
            USER_STDOUT.lock().extend_from_slice(data);
            Ok(len)
        }
        0 => Err(SyscallError::PermissionDenied),
        _ => Err(SyscallError::InvalidArgument),
    }
}

fn sys_read(fd: usize, buf: usize, len: usize) -> SyscallResult {
    if fd != 0 {
        return Err(SyscallError::InvalidArgument);
    }

    if len == 0 {
        return Ok(0);
    }

    let mut stdin = USER_STDIN.lock();
    let mut read = 0;
    while read < len {
        match stdin.pop_front() {
            Some(byte) => {
                unsafe { ((buf as *mut u8).add(read)).write(byte); }
                read += 1;
            }
            None => break,
        }
    }
    Ok(read)
}

pub fn feed_stdin(bytes: &[u8]) {
    USER_STDIN.lock().extend(bytes);
}

pub fn take_stdout() -> Vec<u8> {
    let mut buffer = USER_STDOUT.lock();
    let mut drained = Vec::new();
    core::mem::swap(&mut *buffer, &mut drained);
    drained
}

fn current_thread_snapshot() -> Result<(ThreadId, thread::Thread), SyscallError> {
    let tid = scheduler::current_thread().ok_or(SyscallError::ProcessNotFound)?;
    let thread = thread::get_thread(tid).ok_or(SyscallError::ProcessNotFound)?;
    Ok((tid, thread))
}

fn current_arch() -> TargetArch {
    #[cfg(target_arch = "x86_64")]
    {
        TargetArch::X86_64
    }

    #[cfg(target_arch = "aarch64")]
    {
        TargetArch::AArch64
    }
}

fn read_user_cstring(ptr: usize) -> Result<String, SyscallError> {
    if ptr == 0 {
        return Err(SyscallError::InvalidArgument);
    }

    let mut bytes = Vec::new();
    let mut offset = 0usize;
    loop {
        let byte = unsafe { ((ptr as *const u8).add(offset)).read() };
        if byte == 0 {
            break;
        }
        bytes.push(byte);
        offset += 1;
        if offset > 4096 {
            return Err(SyscallError::InvalidArgument);
        }
    }

    str::from_utf8(&bytes)
        .map(|s| s.to_string())
        .map_err(|_| SyscallError::InvalidArgument)
}

fn read_user_string_array(ptr: usize) -> Result<Vec<String>, SyscallError> {
    if ptr == 0 {
        return Ok(Vec::new());
    }

    let mut result = Vec::new();
    let mut index = 0usize;
    loop {
        let entry_ptr = unsafe { *((ptr as *const usize).add(index)) };
        if entry_ptr == 0 {
            break;
        }
        result.push(read_user_cstring(entry_ptr)?);
        index += 1;
        if index > 128 {
            break;
        }
    }
    Ok(result)
}

fn register_waiter(parent: ProcessId, tid: ThreadId, pid: usize) {
    let mut waiters = WAITERS.lock();
    waiters.retain(|waiter| waiter.tid != tid);
    let target = if pid == 0 { None } else { Some(ProcessId(pid)) };
    waiters.push(Waiter { parent, tid, target });
}

fn take_zombie(parent: ProcessId, pid: usize) -> Option<ZombieChild> {
    let mut zombies = ZOMBIE_CHILDREN.lock();
    if pid == 0 {
        if let Some(index) = zombies.iter().position(|z| z.parent == parent) {
            return Some(zombies.remove(index));
        }
    } else {
        let target = ProcessId(pid);
        if let Some(index) = zombies.iter().position(|z| z.parent == parent && z.child == target) {
            return Some(zombies.remove(index));
        }
    }
    None
}

fn record_child_exit(parent: Option<ProcessId>, child: ProcessId, status: usize) {
    if let Some(parent_pid) = parent {
        {
            let mut table = process::PROCESS_TABLE.lock();
            if let Some(parent_process) = table.get_process_mut(parent_pid) {
                parent_process.children.retain(|pid| *pid != child);
            }
        }

        {
            let mut zombies = ZOMBIE_CHILDREN.lock();
            zombies.push(ZombieChild {
                parent: parent_pid,
                child,
                status,
            });
        }

        let waiter = {
            let mut waiters = WAITERS.lock();
            if let Some(index) = waiters
                .iter()
                .position(|w| w.parent == parent_pid && (w.target.is_none() || w.target == Some(child)))
            {
                Some(waiters.remove(index))
            } else {
                None
            }
        };

        if let Some(waiter) = waiter {
            scheduler::unblock_thread(waiter.tid);
        }
    }
}

fn finalize_thread(tid: ThreadId, thread: thread::Thread, exit_code: usize) {
    thread::set_thread_state(tid, ThreadState::Terminated);
    scheduler::remove_thread(tid);
    THREAD_TABLE.lock().remove_thread(tid);

    let mut table = process::PROCESS_TABLE.lock();
    if let Some(process) = table.get_process_mut(thread.process_id) {
        process.remove_thread(tid);
        if process.threads.is_empty() {
            let parent = process.parent;
            let _ = table.remove_process(thread.process_id);
            drop(table);
            record_child_exit(parent, thread.process_id, exit_code);
            return;
        }
    }
}

pub extern "C" fn syscall_handler(
    syscall_number: usize,
    arg1: usize,
    arg2: usize,
    arg3: usize,
    arg4: usize,
    arg5: usize,
    arg6: usize,
) -> usize {
    match handle_syscall(syscall_number, arg1, arg2, arg3, arg4, arg5, arg6) {
        Ok(result) => result,
        Err(_) => usize::MAX,
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_syscall_numbers() {
        assert_eq!(SYS_EXIT, 0);
        assert_eq!(SYS_FORK, 1);
        assert_eq!(SYS_GETPID, 4);
        assert_eq!(SYS_WRITE, 9);
    }
}
