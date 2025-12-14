#[cfg(not(test))]
extern crate alloc;

use alloc::{collections::VecDeque, string::{String, ToString}, vec::Vec};
use core::{slice, str};

use spin::Mutex;

use crate::memory::{frame_allocator::allocate_frame, paging::Page, VirtAddr};
use crate::process::{
    self,
    create_process,
    create_thread,
    scheduler,
    thread::{self, ThreadState, THREAD_TABLE},
    Context,
    ProcessId,
    ThreadId,
};
use crate::process::elf_loader::{self, TargetArch};
use crate::security::{
    CapMask, PrivilegeLevel, CAP_CONSOLE_IO, CAP_PROC_MANAGE, CAP_VM_MANAGE,
};
use crate::process::loader;
use crate::userspace;

pub mod entry;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[repr(i32)]
pub enum Errno {
    EPERM = 1,
    ESRCH = 3,
    ENOMEM = 12,
    EINVAL = 22,
    ENOSYS = 38,
}

pub type SyscallResult = Result<usize, Errno>;
pub type SyscallError = Errno;

#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct SyscallArgs {
    pub a1: usize,
    pub a2: usize,
    pub a3: usize,
    pub a4: usize,
    pub a5: usize,
    pub a6: usize,
}

impl SyscallArgs {
    pub const fn new(a1: usize, a2: usize, a3: usize, a4: usize, a5: usize, a6: usize) -> Self {
        Self { a1, a2, a3, a4, a5, a6 }
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum SyscallArgKind {
    None,
    Usize,
    Fd,
    Ptr,
    Len,
    Flags,
    Offset,
    CStringPtr,
    CStringArrayPtr,
}

pub type SyscallHandlerFn = fn(SyscallArgs) -> SyscallResult;

#[derive(Clone, Copy)]
pub struct SyscallDescriptor {
    pub number: usize,
    pub name: &'static str,
    pub handler: SyscallHandlerFn,
    pub max_caller_ring: PrivilegeLevel,
    pub required_capabilities: CapMask,
    pub args: [SyscallArgKind; 6],
}

const fn arg_spec(
    a1: SyscallArgKind,
    a2: SyscallArgKind,
    a3: SyscallArgKind,
    a4: SyscallArgKind,
    a5: SyscallArgKind,
    a6: SyscallArgKind,
) -> [SyscallArgKind; 6] {
    [a1, a2, a3, a4, a5, a6]
}

fn sys_unimplemented(_: SyscallArgs) -> SyscallResult {
    Err(Errno::ENOSYS)
}

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
pub const SYS_OPEN: usize = 11;
pub const SYS_CLOSE: usize = 12;
pub const SYS_PIPE: usize = 13;
pub const SYS_SIGNAL: usize = 14;

pub const SYS_EXECVE: usize = SYS_EXEC;
pub const SYS_WAITPID: usize = SYS_WAIT;
pub const SYS_IOCTL: usize = 13;
pub const SYS_YIELD: usize = 14;
pub const SYS_NANOSLEEP: usize = 15;
pub const SYS_IPC_SEND: usize = 16;
pub const SYS_IPC_RECV: usize = 17;
pub const SYS_SHM_CREATE: usize = 18;
pub const SYS_SHM_MAP: usize = 19;
pub const SYS_SYSFS_READ: usize = 20;
pub const SYS_SYSFS_WRITE: usize = 21;
pub const SYS_DEBUG_LOG: usize = 22;

pub const SYSCALL_MAX: usize = 32;

pub static SYSCALL_TABLE: [SyscallDescriptor; SYSCALL_MAX] = [
    SyscallDescriptor {
        number: SYS_EXIT,
        name: "exit",
        handler: sys_exit,
        max_caller_ring: PrivilegeLevel::Ring3,
        required_capabilities: CAP_NONE,
        args: arg_spec(SyscallArgKind::Usize, SyscallArgKind::None, SyscallArgKind::None, SyscallArgKind::None, SyscallArgKind::None, SyscallArgKind::None),
    },
    SyscallDescriptor {
        number: SYS_FORK,
        name: "fork",
        handler: sys_fork,
        max_caller_ring: PrivilegeLevel::Ring3,
        required_capabilities: CAP_PROC_MANAGE,
        args: arg_spec(SyscallArgKind::None, SyscallArgKind::None, SyscallArgKind::None, SyscallArgKind::None, SyscallArgKind::None, SyscallArgKind::None),
    },
    SyscallDescriptor {
        number: SYS_EXEC,
        name: "exec",
        handler: sys_exec,
        max_caller_ring: PrivilegeLevel::Ring3,
        required_capabilities: CAP_PROC_MANAGE,
        args: arg_spec(SyscallArgKind::CStringPtr, SyscallArgKind::CStringArrayPtr, SyscallArgKind::None, SyscallArgKind::None, SyscallArgKind::None, SyscallArgKind::None),
    },
    SyscallDescriptor {
        number: SYS_WAIT,
        name: "wait",
        handler: sys_wait,
        max_caller_ring: PrivilegeLevel::Ring3,
        required_capabilities: CAP_PROC_MANAGE,
        args: arg_spec(SyscallArgKind::Usize, SyscallArgKind::None, SyscallArgKind::None, SyscallArgKind::None, SyscallArgKind::None, SyscallArgKind::None),
    },
    SyscallDescriptor {
        number: SYS_GETPID,
        name: "getpid",
        handler: sys_getpid,
        max_caller_ring: PrivilegeLevel::Ring3,
        required_capabilities: CAP_NONE,
        args: arg_spec(SyscallArgKind::None, SyscallArgKind::None, SyscallArgKind::None, SyscallArgKind::None, SyscallArgKind::None, SyscallArgKind::None),
    },
    SyscallDescriptor {
        number: SYS_MMAP,
        name: "mmap",
        handler: sys_mmap,
        max_caller_ring: PrivilegeLevel::Ring3,
        required_capabilities: CAP_VM_MANAGE,
        args: arg_spec(SyscallArgKind::Ptr, SyscallArgKind::Len, SyscallArgKind::Flags, SyscallArgKind::Flags, SyscallArgKind::Fd, SyscallArgKind::Offset),
    },
    SyscallDescriptor {
        number: SYS_MUNMAP,
        name: "munmap",
        handler: sys_munmap,
        max_caller_ring: PrivilegeLevel::Ring3,
        required_capabilities: CAP_VM_MANAGE,
        args: arg_spec(SyscallArgKind::Ptr, SyscallArgKind::Len, SyscallArgKind::None, SyscallArgKind::None, SyscallArgKind::None, SyscallArgKind::None),
    },
    SyscallDescriptor {
        number: SYS_BRK,
        name: "brk",
        handler: sys_brk,
        max_caller_ring: PrivilegeLevel::Ring3,
        required_capabilities: CAP_VM_MANAGE,
        args: arg_spec(SyscallArgKind::Ptr, SyscallArgKind::None, SyscallArgKind::None, SyscallArgKind::None, SyscallArgKind::None, SyscallArgKind::None),
    },
    SyscallDescriptor {
        number: SYS_CLONE,
        name: "clone",
        handler: sys_clone,
        max_caller_ring: PrivilegeLevel::Ring3,
        required_capabilities: CAP_PROC_MANAGE,
        args: arg_spec(SyscallArgKind::Flags, SyscallArgKind::Ptr, SyscallArgKind::Ptr, SyscallArgKind::None, SyscallArgKind::None, SyscallArgKind::None),
    },
    SyscallDescriptor {
        number: SYS_WRITE,
        name: "write",
        handler: sys_write,
        max_caller_ring: PrivilegeLevel::Ring3,
        required_capabilities: CAP_CONSOLE_IO,
        args: arg_spec(SyscallArgKind::Fd, SyscallArgKind::Ptr, SyscallArgKind::Len, SyscallArgKind::None, SyscallArgKind::None, SyscallArgKind::None),
    },
    SyscallDescriptor {
        number: SYS_READ,
        name: "read",
        handler: sys_read,
        max_caller_ring: PrivilegeLevel::Ring3,
        required_capabilities: CAP_CONSOLE_IO,
        args: arg_spec(SyscallArgKind::Fd, SyscallArgKind::Ptr, SyscallArgKind::Len, SyscallArgKind::None, SyscallArgKind::None, SyscallArgKind::None),
    },
    SyscallDescriptor {
        number: SYS_OPEN,
        name: "open",
        handler: sys_unimplemented,
        max_caller_ring: PrivilegeLevel::Ring3,
        required_capabilities: 0,
        args: arg_spec(SyscallArgKind::CStringPtr, SyscallArgKind::Flags, SyscallArgKind::Flags, SyscallArgKind::None, SyscallArgKind::None, SyscallArgKind::None),
    },
    SyscallDescriptor {
        number: SYS_CLOSE,
        name: "close",
        handler: sys_unimplemented,
        max_caller_ring: PrivilegeLevel::Ring3,
        required_capabilities: 0,
        args: arg_spec(SyscallArgKind::Fd, SyscallArgKind::None, SyscallArgKind::None, SyscallArgKind::None, SyscallArgKind::None, SyscallArgKind::None),
    },
    SyscallDescriptor {
        number: SYS_IOCTL,
        name: "ioctl",
        handler: sys_unimplemented,
        max_caller_ring: PrivilegeLevel::Ring3,
        required_capabilities: 0,
        args: arg_spec(SyscallArgKind::Fd, SyscallArgKind::Usize, SyscallArgKind::Ptr, SyscallArgKind::None, SyscallArgKind::None, SyscallArgKind::None),
    },
    SyscallDescriptor {
        number: SYS_YIELD,
        name: "yield",
        handler: sys_yield,
        max_caller_ring: PrivilegeLevel::Ring3,
        required_capabilities: CAP_NONE,
        args: arg_spec(SyscallArgKind::None, SyscallArgKind::None, SyscallArgKind::None, SyscallArgKind::None, SyscallArgKind::None, SyscallArgKind::None),
    },
    SyscallDescriptor {
        number: SYS_NANOSLEEP,
        name: "nanosleep",
        handler: sys_unimplemented,
        max_caller_ring: PrivilegeLevel::Ring3,
        required_capabilities: CAP_NONE,
        args: arg_spec(SyscallArgKind::Ptr, SyscallArgKind::Ptr, SyscallArgKind::None, SyscallArgKind::None, SyscallArgKind::None, SyscallArgKind::None),
    },
    SyscallDescriptor {
        number: SYS_IPC_SEND,
        name: "ipc_send",
        handler: sys_unimplemented,
        max_caller_ring: PrivilegeLevel::Ring3,
        required_capabilities: 0,
        args: arg_spec(SyscallArgKind::Usize, SyscallArgKind::Ptr, SyscallArgKind::Len, SyscallArgKind::Flags, SyscallArgKind::None, SyscallArgKind::None),
    },
    SyscallDescriptor {
        number: SYS_IPC_RECV,
        name: "ipc_recv",
        handler: sys_unimplemented,
        max_caller_ring: PrivilegeLevel::Ring3,
        required_capabilities: 0,
        args: arg_spec(SyscallArgKind::Usize, SyscallArgKind::Ptr, SyscallArgKind::Len, SyscallArgKind::Flags, SyscallArgKind::None, SyscallArgKind::None),
    },
    SyscallDescriptor {
        number: SYS_SHM_CREATE,
        name: "shm_create",
        handler: sys_unimplemented,
        max_caller_ring: PrivilegeLevel::Ring3,
        required_capabilities: 0,
        args: arg_spec(SyscallArgKind::Usize, SyscallArgKind::Len, SyscallArgKind::Flags, SyscallArgKind::None, SyscallArgKind::None, SyscallArgKind::None),
    },
    SyscallDescriptor {
        number: SYS_SHM_MAP,
        name: "shm_map",
        handler: sys_unimplemented,
        max_caller_ring: PrivilegeLevel::Ring3,
        required_capabilities: 0,
        args: arg_spec(SyscallArgKind::Usize, SyscallArgKind::Ptr, SyscallArgKind::Len, SyscallArgKind::Flags, SyscallArgKind::None, SyscallArgKind::None),
    },
    SyscallDescriptor {
        number: SYS_SYSFS_READ,
        name: "sysfs_read",
        handler: sys_unimplemented,
        max_caller_ring: PrivilegeLevel::Ring3,
        required_capabilities: 0,
        args: arg_spec(SyscallArgKind::CStringPtr, SyscallArgKind::Ptr, SyscallArgKind::Len, SyscallArgKind::None, SyscallArgKind::None, SyscallArgKind::None),
    },
    SyscallDescriptor {
        number: SYS_SYSFS_WRITE,
        name: "sysfs_write",
        handler: sys_unimplemented,
        max_caller_ring: PrivilegeLevel::Ring3,
        required_capabilities: 0,
        args: arg_spec(SyscallArgKind::CStringPtr, SyscallArgKind::Ptr, SyscallArgKind::Len, SyscallArgKind::None, SyscallArgKind::None, SyscallArgKind::None),
    },
    SyscallDescriptor {
        number: SYS_DEBUG_LOG,
        name: "debug_log",
        handler: sys_unimplemented,
        max_caller_ring: PrivilegeLevel::Ring0,
        required_capabilities: 0,
        args: arg_spec(SyscallArgKind::Ptr, SyscallArgKind::Len, SyscallArgKind::None, SyscallArgKind::None, SyscallArgKind::None, SyscallArgKind::None),
    },
    SyscallDescriptor {
        number: 23,
        name: "reserved23",
        handler: sys_unimplemented,
        max_caller_ring: PrivilegeLevel::Ring3,
        required_capabilities: 0,
        args: arg_spec(SyscallArgKind::None, SyscallArgKind::None, SyscallArgKind::None, SyscallArgKind::None, SyscallArgKind::None, SyscallArgKind::None),
    },
    SyscallDescriptor {
        number: 24,
        name: "reserved24",
        handler: sys_unimplemented,
        max_caller_ring: PrivilegeLevel::Ring3,
        required_capabilities: 0,
        args: arg_spec(SyscallArgKind::None, SyscallArgKind::None, SyscallArgKind::None, SyscallArgKind::None, SyscallArgKind::None, SyscallArgKind::None),
    },
    SyscallDescriptor {
        number: 25,
        name: "reserved25",
        handler: sys_unimplemented,
        max_caller_ring: PrivilegeLevel::Ring3,
        required_capabilities: 0,
        args: arg_spec(SyscallArgKind::None, SyscallArgKind::None, SyscallArgKind::None, SyscallArgKind::None, SyscallArgKind::None, SyscallArgKind::None),
    },
    SyscallDescriptor {
        number: 26,
        name: "reserved26",
        handler: sys_unimplemented,
        max_caller_ring: PrivilegeLevel::Ring3,
        required_capabilities: 0,
        args: arg_spec(SyscallArgKind::None, SyscallArgKind::None, SyscallArgKind::None, SyscallArgKind::None, SyscallArgKind::None, SyscallArgKind::None),
    },
    SyscallDescriptor {
        number: 27,
        name: "reserved27",
        handler: sys_unimplemented,
        max_caller_ring: PrivilegeLevel::Ring3,
        required_capabilities: 0,
        args: arg_spec(SyscallArgKind::None, SyscallArgKind::None, SyscallArgKind::None, SyscallArgKind::None, SyscallArgKind::None, SyscallArgKind::None),
    },
    SyscallDescriptor {
        number: 28,
        name: "reserved28",
        handler: sys_unimplemented,
        max_caller_ring: PrivilegeLevel::Ring3,
        required_capabilities: 0,
        args: arg_spec(SyscallArgKind::None, SyscallArgKind::None, SyscallArgKind::None, SyscallArgKind::None, SyscallArgKind::None, SyscallArgKind::None),
    },
    SyscallDescriptor {
        number: 29,
        name: "reserved29",
        handler: sys_unimplemented,
        max_caller_ring: PrivilegeLevel::Ring3,
        required_capabilities: 0,
        args: arg_spec(SyscallArgKind::None, SyscallArgKind::None, SyscallArgKind::None, SyscallArgKind::None, SyscallArgKind::None, SyscallArgKind::None),
    },
    SyscallDescriptor {
        number: 30,
        name: "reserved30",
        handler: sys_unimplemented,
        max_caller_ring: PrivilegeLevel::Ring3,
        required_capabilities: 0,
        args: arg_spec(SyscallArgKind::None, SyscallArgKind::None, SyscallArgKind::None, SyscallArgKind::None, SyscallArgKind::None, SyscallArgKind::None),
    },
    SyscallDescriptor {
        number: 31,
        name: "reserved31",
        handler: sys_unimplemented,
        max_caller_ring: PrivilegeLevel::Ring3,
        required_capabilities: 0,
        args: arg_spec(SyscallArgKind::None, SyscallArgKind::None, SyscallArgKind::None, SyscallArgKind::None, SyscallArgKind::None, SyscallArgKind::None),
    },
];

const CAP_NONE: CapMask = 0;

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

fn syscall_descriptor(syscall_number: usize) -> Option<&'static SyscallDescriptor> {
    SYSCALL_TABLE.get(syscall_number)
}

fn current_thread_snapshot() -> Result<(ThreadId, thread::Thread), Errno> {
    let tid = scheduler::current_thread().ok_or(Errno::ESRCH)?;
    let thread = thread::get_thread(tid).ok_or(Errno::ESRCH)?;
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

    #[cfg(not(any(target_arch = "x86_64", target_arch = "aarch64")))]
    {
        TargetArch::X86_64
    }
}

pub fn handle_syscall(syscall_number: usize, args: SyscallArgs) -> SyscallResult {
    let descriptor = syscall_descriptor(syscall_number).ok_or(Errno::ENOSYS)?;
    debug_assert_eq!(descriptor.number, syscall_number);

    let (_, thread) = current_thread_snapshot()?;
    let process = process::get_process(thread.process_id).ok_or(Errno::ESRCH)?;

    if !process.security.is_privileged_enough(descriptor.max_caller_ring) {
        return Err(Errno::EPERM);
    }

    if !process.security.has_capabilities(descriptor.required_capabilities) {
        return Err(Errno::EPERM);
    }

    (descriptor.handler)(args)
}

#[no_mangle]
pub extern "C" fn syscall_handler(
    syscall_number: usize,
    arg1: usize,
    arg2: usize,
    arg3: usize,
    arg4: usize,
    arg5: usize,
    arg6: usize,
) -> isize {
    match handle_syscall(syscall_number, SyscallArgs::new(arg1, arg2, arg3, arg4, arg5, arg6)) {
        Ok(value) => value as isize,
        Err(errno) => -(errno as isize),
    }
}

fn sys_exit(args: SyscallArgs) -> SyscallResult {
    let (tid, thread) = current_thread_snapshot()?;
    finalize_thread(tid, thread, args.a1);
    Ok(args.a1)
}

fn sys_fork(_: SyscallArgs) -> SyscallResult {
    let (_, thread) = current_thread_snapshot()?;
    let parent_process = process::get_process(thread.process_id).ok_or(Errno::ESRCH)?;

    let new_page_frame = allocate_frame().ok_or(Errno::ENOMEM)?;
    let child_security = parent_process.security.clone();
    let child_fds = parent_process.file_descriptors.inherit();

    let child_pid = create_process(
        parent_process.name.clone(),
        new_page_frame,
        child_security,
        child_fds,
    )
    .ok_or(Errno::ENOMEM)?;

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

    let child_stack = allocate_frame().ok_or(Errno::ENOMEM)?;
    let child_tid = create_thread(
        child_pid,
        thread.priority,
        VirtAddr::new(thread.context.rip),
        VirtAddr::new(child_stack.start_address.0),
        thread.user_stack,
    )
    .ok_or(Errno::ENOMEM)?;

    scheduler::add_thread(child_tid);
    Ok(child_pid.0)
}

fn sys_exec(args: SyscallArgs) -> SyscallResult {
    let (tid, thread) = current_thread_snapshot()?;
    let arch = current_arch();

    let path = if args.a1 == 0 {
        String::from("/sbin/init")
    } else {
        read_user_cstring(args.a1)?
    };

    let mut argv = read_user_string_array(args.a2)?;
    if argv.is_empty() {
        argv.push(path.clone());
    }
    let argv_refs = argv.iter().map(|arg| arg.as_str()).collect::<Vec<_>>();

    let image = userspace::lookup(&path, arch).ok_or(Errno::ESRCH)?;

    let loaded = {
        let mut table = process::PROCESS_TABLE.lock();
        let process = table.get_process_mut(thread.process_id).ok_or(Errno::ESRCH)?;
        let loaded = elf_loader::load_executable(image, arch, &process.address_space, &argv_refs, &[])
            .map_err(|_| Errno::EINVAL)?;
        let process = table
            .get_process_mut(thread.process_id)
            .ok_or(SyscallError::ProcessNotFound)?;
        let loaded = loader::exec_into_process(process, image, arch, &argv_refs, &[])
            .map_err(|_| SyscallError::InvalidArgument)?;
        process.name = path;
        loaded
    };

    let mut threads = THREAD_TABLE.lock();
    if let Some(current) = threads.get_thread_mut(tid) {
        current.context = Context::new_user(loaded.entry_point, loaded.stack.user_sp);
        current.user_stack = Some(loaded.stack.user_sp);
    }

    Ok(0)
}

fn sys_wait(args: SyscallArgs) -> SyscallResult {
    let (tid, thread) = current_thread_snapshot()?;
    if let Some(zombie) = take_zombie(thread.process_id, args.a1) {
        return Ok(zombie.child.0);
    }

    register_waiter(thread.process_id, tid, args.a1);
    scheduler::block_current_thread();
    Ok(0)
}

fn sys_getpid(_: SyscallArgs) -> SyscallResult {
    let (_, thread) = current_thread_snapshot()?;
    Ok(thread.process_id.0)
}

fn sys_mmap(args: SyscallArgs) -> SyscallResult {
    let addr = args.a1;
    let length = args.a2;

    if length == 0 {
        return Err(Errno::EINVAL);
    }

    let start_page = Page::containing_address(VirtAddr::new(addr as u64));
    let num_pages = (length + 4095) / 4096;

    for i in 0..num_pages {
        let _page = Page::containing_address(VirtAddr::new(
            start_page.start_address.0 + (i * 4096) as u64,
        ));
        let _frame = allocate_frame().ok_or(Errno::ENOMEM)?;
    }

    Ok(addr)
}

fn sys_munmap(_: SyscallArgs) -> SyscallResult {
    Ok(0)
}

fn sys_brk(args: SyscallArgs) -> SyscallResult {
    Ok(args.a1)
}

fn sys_clone(args: SyscallArgs) -> SyscallResult {
    let (_, thread) = current_thread_snapshot()?;

    let stack = args.a2;
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
    .ok_or(Errno::ENOMEM)?;

    scheduler::add_thread(new_tid);
    Ok(new_tid.0)
}

fn sys_write(args: SyscallArgs) -> SyscallResult {
    let fd = args.a1;
    let buf = args.a2;
    let len = args.a3;

    if len == 0 {
        return Ok(0);
    }

    let data = unsafe { slice::from_raw_parts(buf as *const u8, len) };
    match fd {
        1 | 2 => {
            USER_STDOUT.lock().extend_from_slice(data);
            Ok(len)
        }
        0 => Err(Errno::EPERM),
        _ => Err(Errno::EINVAL),
    }
}

fn sys_read(args: SyscallArgs) -> SyscallResult {
    let fd = args.a1;
    let buf = args.a2;
    let len = args.a3;

    if fd != 0 {
        return Err(Errno::EINVAL);
    }

    if len == 0 {
        return Ok(0);
    }

    let mut stdin = USER_STDIN.lock();
    let mut read = 0;
    while read < len {
        match stdin.pop_front() {
            Some(byte) => {
                unsafe {
                    ((buf as *mut u8).add(read)).write(byte);
                }
                read += 1;
            }
            None => break,
        }
    }
    Ok(read)
}

fn sys_yield(_: SyscallArgs) -> SyscallResult {
    scheduler::yield_cpu();
    Ok(0)
}

fn sys_open(_path_ptr: usize, _flags: usize, _mode: usize) -> SyscallResult {
    Err(SyscallError::InvalidSyscall)
}

fn sys_close(_fd: usize) -> SyscallResult {
    Err(SyscallError::InvalidSyscall)
}

fn sys_pipe(_pipefd_ptr: usize) -> SyscallResult {
    Err(SyscallError::InvalidSyscall)
}

fn sys_signal(_signum: usize, _handler: usize) -> SyscallResult {
    Err(SyscallError::InvalidSyscall)
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

fn read_user_cstring(ptr: usize) -> Result<String, Errno> {
    if ptr == 0 {
        return Err(Errno::EINVAL);
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
            return Err(Errno::EINVAL);
        }
    }

    str::from_utf8(&bytes)
        .map(|s| s.to_string())
        .map_err(|_| Errno::EINVAL)
}

fn read_user_string_array(ptr: usize) -> Result<Vec<String>, Errno> {
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
        if let Some(index) = zombies
            .iter()
            .position(|z| z.parent == parent && z.child == target)
        {
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

#[cfg(test)]
mod tests {
    use super::*;
    use crate::memory::{frame_allocator::Frame, PhysAddr, VirtAddr};
    use crate::process::{scheduler::SCHEDULER, thread::ThreadTable, ProcessTable};
    use crate::security::{SecurityContext, CAP_CONSOLE_IO};

    fn reset_state() {
        *crate::process::PROCESS_TABLE.lock() = ProcessTable::new();
        *crate::process::thread::THREAD_TABLE.lock() = ThreadTable::new();
        *SCHEDULER.lock() = crate::process::scheduler::Scheduler::new();

        USER_STDOUT.lock().clear();
        USER_STDIN.lock().clear();
        ZOMBIE_CHILDREN.lock().clear();
        WAITERS.lock().clear();
    }

    fn create_minimal_process_with_thread(security: SecurityContext) -> (ProcessId, ThreadId) {
        let frame = Frame {
            start_address: PhysAddr::new(0x1000),
        };
        let pid = create_process(
            "test".into(),
            frame,
            security,
            FileDescriptorTable::new(),
        )
        .unwrap();

        let tid = create_thread(
            pid,
            crate::process::Priority::Normal,
            VirtAddr::new(0x2000),
            VirtAddr::new(0x3000),
            Some(VirtAddr::new(0x4000)),
        )
        .unwrap();

        scheduler::add_thread(tid);
        scheduler::schedule();

        (pid, tid)
    }

    #[test]
    fn test_syscall_numbers() {
        assert_eq!(SYS_EXIT, 0);
        assert_eq!(SYS_FORK, 1);
        assert_eq!(SYS_GETPID, 4);
        assert_eq!(SYS_WRITE, 9);
        assert_eq!(SYS_READ, 10);
    }

    #[test]
    fn test_capability_enforcement() {
        reset_state();

        let security = SecurityContext::as_user(1000);
        let (pid, _tid) = create_minimal_process_with_thread(security);

        // getpid requires no caps
        let ok = syscall_handler(SYS_GETPID, 0, 0, 0, 0, 0, 0);
        assert_eq!(ok, pid.0 as isize);

        // write requires CAP_CONSOLE_IO
        let denied = syscall_handler(SYS_WRITE, 1, 0, 0, 0, 0, 0);
        assert_eq!(denied, -(Errno::EPERM as isize));

        crate::process::grant_process_capabilities(pid, CAP_CONSOLE_IO);

        let ok2 = syscall_handler(SYS_WRITE, 1, 0, 0, 0, 0, 0);
        assert_eq!(ok2, 0);
    }

    #[test]
    fn test_privilege_ring_enforcement() {
        reset_state();

        let security = SecurityContext::as_user(1000).with_capabilities(u64::MAX);
        create_minimal_process_with_thread(security);

        let denied = syscall_handler(SYS_DEBUG_LOG, 0, 0, 0, 0, 0, 0);
        assert_eq!(denied, -(Errno::EPERM as isize));

        reset_state();
        let security = SecurityContext::kernel();
        create_minimal_process_with_thread(security);

        let allowed = syscall_handler(SYS_DEBUG_LOG, 0, 0, 0, 0, 0, 0);
        assert_eq!(allowed, -(Errno::ENOSYS as isize));
    }
}
