//! Architecture entry stubs for the syscall ABI.
//!
//! These stubs are responsible for marshalling the architecture-specific syscall
//! register ABI into the architecture-agnostic `syscall_handler(number, a1..a6)`
//! calling convention.

#[cfg(any(target_arch = "x86_64", target_arch = "aarch64"))]
use core::arch::global_asm;

#[cfg(target_arch = "x86_64")]
global_asm!(r#"
    .section .text
    .att_syntax
    .global syscall_entry
    .type syscall_entry, @function

// x86_64 syscall entry
//
// Userspace ABI:
//   rax = syscall number
//   rdi, rsi, rdx, r10, r8, r9 = args 1..6
//
// Kernel ABI (SysV C):
//   rdi, rsi, rdx, rcx, r8, r9 = args 1..6
//   7th arg passed on stack
syscall_entry:
    // Preserve return state for sysretq
    pushq %rcx
    pushq %r11

    // Place arg6 on stack as the 7th C argument.
    pushq %r9

    // Shuffle arguments into SysV ABI registers.
    movq %r8, %r9      // a5
    movq %r10, %r8     // a4
    movq %rdx, %rcx    // a3
    movq %rsi, %rdx    // a2
    movq %rdi, %rsi    // a1
    movq %rax, %rdi    // syscall_number

    call syscall_handler

    addq $8, %rsp      // drop arg6

    // Restore return state
    popq %r11
    popq %rcx

    sysretq
"#);

#[cfg(target_arch = "aarch64")]
global_asm!(r#"
    .section .text
    .global svc_entry
    .type svc_entry, %function

// AArch64 SVC entry from EL0.
//
// Userspace ABI:
//   x8 = syscall number
//   x0..x5 = args 1..6
//
// Kernel ABI (AAPCS64):
//   x0..x6 = syscall_handler(number, a1..a6)
svc_entry:
    stp x29, x30, [sp, #-16]!
    mov x29, sp

    // Marshal arguments.
    mov x6, x5
    mov x5, x4
    mov x4, x3
    mov x3, x2
    mov x2, x1
    mov x1, x0
    mov x0, x8

    bl syscall_handler

    ldp x29, x30, [sp], #16
    eret
"#);
