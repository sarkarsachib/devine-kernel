# System Call ABI

This document specifies the **stable, cross‑architecture system call ABI** exposed to userspace.

The syscall interface is the single, stable boundary between userspace and the kernel. Syscall numbers and their metadata (privilege/capability requirements and argument layout) are defined by the kernel’s syscall descriptor table in `src/syscall/mod.rs`.

## Common semantics

- **Syscall number namespace**: one global numbering scheme shared by all architectures.
- **Maximum arguments**: 6 machine‑word arguments.
- **Return value**:
  - On success: a non‑negative integer result.
  - On failure: a **negative errno** value (Linux‑style), e.g. `-EPERM`.
- **Errors**: at minimum the kernel returns `-EPERM`, `-EINVAL`, `-ENOMEM`, `-ESRCH`, `-ENOSYS`.
- **Denied calls**: if a syscall fails the privilege ring check or the capability mask check, it returns **`-EPERM`**.

## x86_64 ABI (`syscall` / `sysretq`)

### Userspace → kernel entry

Userspace must invoke:

- `syscall`

Register convention on entry:

| Purpose | Register |
|---|---|
| Syscall number | `RAX` |
| Arg0 | `RDI` |
| Arg1 | `RSI` |
| Arg2 | `RDX` |
| Arg3 | `R10` |
| Arg4 | `R8` |
| Arg5 | `R9` |

Clobbers (as with Linux):
- `RCX` and `R11` are clobbered by the CPU during `syscall`.

### Kernel → userspace return

- Return instruction: `sysretq`
- Return value is placed in `RAX`.
- `RCX` contains the return RIP and `R11` contains the return RFLAGS when executing `sysretq`.

### Required MSR configuration

The kernel must configure the syscall MSRs during early boot:

- `IA32_EFER.SCE` (enable `syscall`/`sysret`)
- `IA32_STAR` (kernel/user CS/SS selectors)
- `IA32_LSTAR` (64‑bit entry point RIP)
- `IA32_FMASK` (RFLAGS mask applied on entry)

In this tree, the intended configuration is performed during early boot in `src/x86_64/boot.s` and the entry stub targets the symbol `syscall_entry`.

## AArch64 ABI (`svc #0`)

### Userspace → kernel entry

Userspace must invoke:

- `svc #0`

Register convention on entry:

| Purpose | Register |
|---|---|
| Syscall number | `X8` |
| Arg0..Arg5 | `X0..X5` |

### Kernel → userspace return

- Return instruction: `eret`
- Return value is placed in `X0`.

### Exception vector

The kernel must install an exception vector base (`VBAR_EL1`) containing a handler for **synchronous exceptions from EL0** (the `svc` path). In this tree, the boot code in `src/arm64/boot.s` installs the vector base and routes the EL0 SVC case to the symbol `svc_entry`.

## Syscall numbering and metadata

Syscall numbers are stable. Unimplemented/reserved numbers are retained as placeholders so that the ABI remains stable as new subsystems (VFS, IPC, sysfs, …) are brought up.

The authoritative table is in `src/syscall/mod.rs` (search for `SYSCALL_TABLE`). A snapshot of the currently reserved ABI:

| Number | Name | Notes |
|---:|---|---|
| 0 | `exit` | process/thread termination |
| 1 | `fork` | process creation |
| 2 | `exec` | image replace |
| 3 | `wait` | wait on child |
| 4 | `getpid` | query process id |
| 5 | `mmap` | virtual memory mapping |
| 6 | `munmap` | unmap |
| 7 | `brk` | heap break |
| 8 | `clone` | thread creation |
| 9 | `write` | console write (temporary) |
| 10 | `read` | console read (temporary) |
| 11+ | reserved | VFS/IPC/sysfs placeholders |
