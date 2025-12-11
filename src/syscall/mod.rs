use crate::process::{create_process, create_thread};
use crate::memory::{VirtAddr, frame_allocator::allocate_frame, paging::Page};

pub const SYS_EXIT: usize = 0;
pub const SYS_FORK: usize = 1;
pub const SYS_EXEC: usize = 2;
pub const SYS_WAIT: usize = 3;
pub const SYS_GETPID: usize = 4;
pub const SYS_MMAP: usize = 5;
pub const SYS_MUNMAP: usize = 6;
pub const SYS_BRK: usize = 7;
pub const SYS_CLONE: usize = 8;

#[derive(Debug)]
pub enum SyscallError {
    InvalidSyscall,
    InvalidArgument,
    OutOfMemory,
    ProcessNotFound,
    PermissionDenied,
}

pub type SyscallResult = Result<usize, SyscallError>;

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
        _ => Err(SyscallError::InvalidSyscall),
    }
}

fn sys_exit(exit_code: usize) -> SyscallResult {
    if let Some(current_tid) = crate::process::scheduler::current_thread() {
        crate::process::scheduler::remove_thread(current_tid);
        crate::process::thread::set_thread_state(current_tid, crate::process::ThreadState::Terminated);
    }
    Ok(exit_code)
}

fn sys_fork() -> SyscallResult {
    let current_tid = crate::process::scheduler::current_thread()
        .ok_or(SyscallError::ProcessNotFound)?;
    
    let current_thread = crate::process::thread::get_thread(current_tid)
        .ok_or(SyscallError::ProcessNotFound)?;
    
    let parent_process = crate::process::get_process(current_thread.process_id)
        .ok_or(SyscallError::ProcessNotFound)?;
    
    let new_page_frame = allocate_frame()
        .ok_or(SyscallError::OutOfMemory)?;
    
    let child_pid = create_process(
        parent_process.name.clone(),
        new_page_frame,
    ).ok_or(SyscallError::OutOfMemory)?;
    
    let child_stack = allocate_frame()
        .ok_or(SyscallError::OutOfMemory)?;
    
    let child_tid = create_thread(
        child_pid,
        current_thread.priority,
        VirtAddr::new(current_thread.context.rip),
        VirtAddr::new(child_stack.start_address.0),
        current_thread.user_stack,
    ).ok_or(SyscallError::OutOfMemory)?;
    
    crate::process::scheduler::add_thread(child_tid);
    
    Ok(child_pid.0)
}

fn sys_exec(_path: usize, _argv: usize) -> SyscallResult {
    Err(SyscallError::InvalidArgument)
}

fn sys_wait(_pid: usize) -> SyscallResult {
    crate::process::scheduler::block_current_thread();
    Ok(0)
}

fn sys_getpid() -> SyscallResult {
    let current_tid = crate::process::scheduler::current_thread()
        .ok_or(SyscallError::ProcessNotFound)?;
    
    let current_thread = crate::process::thread::get_thread(current_tid)
        .ok_or(SyscallError::ProcessNotFound)?;
    
    Ok(current_thread.process_id.0)
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
        let _frame = allocate_frame()
            .ok_or(SyscallError::OutOfMemory)?;
        
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
    let current_tid = crate::process::scheduler::current_thread()
        .ok_or(SyscallError::ProcessNotFound)?;
    
    let current_thread = crate::process::thread::get_thread(current_tid)
        .ok_or(SyscallError::ProcessNotFound)?;
    
    let new_stack = if stack == 0 {
        current_thread.kernel_stack
    } else {
        VirtAddr::new(stack as u64)
    };
    
    let new_tid = create_thread(
        current_thread.process_id,
        current_thread.priority,
        VirtAddr::new(current_thread.context.rip),
        new_stack,
        current_thread.user_stack,
    ).ok_or(SyscallError::OutOfMemory)?;
    
    crate::process::scheduler::add_thread(new_tid);
    
    Ok(new_tid.0)
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
    }
}
