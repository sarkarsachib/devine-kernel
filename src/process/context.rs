use crate::memory::VirtAddr;

#[repr(C)]
#[derive(Debug, Clone, Copy)]
pub struct Context {
    pub r15: u64,
    pub r14: u64,
    pub r13: u64,
    pub r12: u64,
    pub r11: u64,
    pub r10: u64,
    pub r9: u64,
    pub r8: u64,
    pub rdi: u64,
    pub rsi: u64,
    pub rbp: u64,
    pub rbx: u64,
    pub rdx: u64,
    pub rcx: u64,
    pub rax: u64,
    pub rip: u64,
    pub cs: u64,
    pub rflags: u64,
    pub rsp: u64,
    pub ss: u64,
}

impl Context {
    pub const fn empty() -> Self {
        Context {
            r15: 0,
            r14: 0,
            r13: 0,
            r12: 0,
            r11: 0,
            r10: 0,
            r9: 0,
            r8: 0,
            rdi: 0,
            rsi: 0,
            rbp: 0,
            rbx: 0,
            rdx: 0,
            rcx: 0,
            rax: 0,
            rip: 0,
            cs: 0,
            rflags: 0,
            rsp: 0,
            ss: 0,
        }
    }

    pub fn new_user(entry_point: VirtAddr, stack_pointer: VirtAddr) -> Self {
        Context {
            r15: 0,
            r14: 0,
            r13: 0,
            r12: 0,
            r11: 0,
            r10: 0,
            r9: 0,
            r8: 0,
            rdi: 0,
            rsi: 0,
            rbp: 0,
            rbx: 0,
            rdx: 0,
            rcx: 0,
            rax: 0,
            rip: entry_point.0,
            cs: 0x1B,
            rflags: 0x202,
            rsp: stack_pointer.0,
            ss: 0x23,
        }
    }

    pub fn new_kernel(entry_point: VirtAddr, stack_pointer: VirtAddr) -> Self {
        Context {
            r15: 0,
            r14: 0,
            r13: 0,
            r12: 0,
            r11: 0,
            r10: 0,
            r9: 0,
            r8: 0,
            rdi: 0,
            rsi: 0,
            rbp: 0,
            rbx: 0,
            rdx: 0,
            rcx: 0,
            rax: 0,
            rip: entry_point.0,
            cs: 0x08,
            rflags: 0x202,
            rsp: stack_pointer.0,
            ss: 0x10,
        }
    }
}

pub unsafe fn switch_context(_old: *mut Context, _new: *const Context) {
    
}

#[repr(C)]
#[derive(Debug, Clone, Copy)]
pub struct ArmContext {
    pub r0: u64,
    pub r1: u64,
    pub r2: u64,
    pub r3: u64,
    pub r4: u64,
    pub r5: u64,
    pub r6: u64,
    pub r7: u64,
    pub r8: u64,
    pub r9: u64,
    pub r10: u64,
    pub r11: u64,
    pub r12: u64,
    pub sp: u64,
    pub lr: u64,
    pub pc: u64,
    pub cpsr: u64,
}

impl ArmContext {
    pub const fn empty() -> Self {
        ArmContext {
            r0: 0,
            r1: 0,
            r2: 0,
            r3: 0,
            r4: 0,
            r5: 0,
            r6: 0,
            r7: 0,
            r8: 0,
            r9: 0,
            r10: 0,
            r11: 0,
            r12: 0,
            sp: 0,
            lr: 0,
            pc: 0,
            cpsr: 0,
        }
    }

    pub fn new_user(entry_point: VirtAddr, stack_pointer: VirtAddr) -> Self {
        ArmContext {
            r0: 0,
            r1: 0,
            r2: 0,
            r3: 0,
            r4: 0,
            r5: 0,
            r6: 0,
            r7: 0,
            r8: 0,
            r9: 0,
            r10: 0,
            r11: 0,
            r12: 0,
            sp: stack_pointer.0,
            lr: 0,
            pc: entry_point.0,
            cpsr: 0x10,
        }
    }

    pub fn new_kernel(entry_point: VirtAddr, stack_pointer: VirtAddr) -> Self {
        ArmContext {
            r0: 0,
            r1: 0,
            r2: 0,
            r3: 0,
            r4: 0,
            r5: 0,
            r6: 0,
            r7: 0,
            r8: 0,
            r9: 0,
            r10: 0,
            r11: 0,
            r12: 0,
            sp: stack_pointer.0,
            lr: 0,
            pc: entry_point.0,
            cpsr: 0x1F,
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_context_creation() {
        let ctx = Context::empty();
        assert_eq!(ctx.rax, 0);
        assert_eq!(ctx.rip, 0);

        let user_ctx = Context::new_user(VirtAddr::new(0x1000), VirtAddr::new(0x2000));
        assert_eq!(user_ctx.rip, 0x1000);
        assert_eq!(user_ctx.rsp, 0x2000);
    }

    #[test]
    fn test_arm_context() {
        let ctx = ArmContext::empty();
        assert_eq!(ctx.pc, 0);
        assert_eq!(ctx.sp, 0);
    }
}
