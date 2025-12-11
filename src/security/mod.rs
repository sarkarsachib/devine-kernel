#![cfg_attr(not(test), allow(dead_code))]

#[cfg(not(test))]
extern crate alloc;

use alloc::vec::Vec;
use core::fmt;
use spin::Mutex;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum PrivilegeLevel {
    Ring0 = 0,
    Ring1 = 1,
    Ring2 = 2,
    Ring3 = 3,
}

impl fmt::Display for PrivilegeLevel {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "Ring{}", *self as u8)
    }
}

pub type CapMask = u64;

pub const CAP_NONE: CapMask = 0;

pub const CAP_PROC_MANAGE: CapMask = 1 << 16;
pub const CAP_VM_MANAGE: CapMask = 1 << 17;
pub const CAP_CONSOLE_IO: CapMask = 1 << 18;
pub const CAP_FS_RW: CapMask = 1 << 19;
pub const CAP_IPC: CapMask = 1 << 20;
pub const CAP_SYS_ADMIN: CapMask = 1 << 63;

#[derive(Debug, Clone)]
pub struct SecurityContext {
    pub uid: u32,
    pub gid: u32,
    pub euid: u32,
    pub egid: u32,
    pub privilege: PrivilegeLevel,
    pub umask: u16,
    pub capabilities: u64,
}

impl SecurityContext {
    pub fn new(uid: u32, privilege: PrivilegeLevel) -> Self {
        SecurityContext {
            uid,
            gid: uid,
            euid: uid,
            egid: uid,
            privilege,
            umask: 0o022,
            capabilities: 0,
        }
    }

    pub fn kernel() -> Self {
        let mut ctx = Self::new(0, PrivilegeLevel::Ring0);
        ctx.capabilities = u64::MAX;
        ctx
    }

    pub fn as_user(uid: u32) -> Self {
        Self::new(uid, PrivilegeLevel::Ring3)
    }

    pub fn drop_privileges(&mut self, level: PrivilegeLevel) {
        if level as u8 >= self.privilege as u8 {
            self.privilege = level;
        }
    }

    pub fn set_umask(&mut self, mask: u16) {
        self.umask = mask & 0o777;
    }

    pub fn apply_umask(&self, mode: u32) -> u32 {
        mode & !(self.umask as u32)
    }

    pub fn apply_setuid(&mut self, uid: u32) {
        self.euid = uid;
        self.egid = uid;
    }

    pub fn is_privileged_enough(&self, max_allowed_ring: PrivilegeLevel) -> bool {
        (self.privilege as u8) <= (max_allowed_ring as u8)
    }

    pub fn has_capabilities(&self, required: CapMask) -> bool {
        (self.capabilities & required) == required
    }

    pub fn grant_capabilities(&mut self, mask: CapMask) {
        self.capabilities |= mask;
    }

    pub fn revoke_capabilities(&mut self, mask: CapMask) {
        self.capabilities &= !mask;
    }

    pub fn with_capabilities(mut self, caps: CapMask) -> Self {
        self.capabilities = caps;
        self
    }

    pub fn inherit_child(&self, caps_subset: CapMask) -> Self {
        let mut child = self.clone();
        child.capabilities = self.capabilities & caps_subset;
        child
    }
}

#[derive(Clone)]
struct SecurityEntry {
    pid: usize,
    context: SecurityContext,
}

impl SecurityEntry {
    fn new(pid: usize, context: SecurityContext) -> Self {
        Self { pid, context }
    }
}

static SECURITY_CONTEXTS: Mutex<Vec<SecurityEntry>> = Mutex::new(Vec::new());

pub fn register_process(pid: usize, context: SecurityContext) {
    let mut table = SECURITY_CONTEXTS.lock();
    if let Some(entry) = table.iter_mut().find(|entry| entry.pid == pid) {
        entry.context = context;
    } else {
        table.push(SecurityEntry::new(pid, context));
    }
}

pub fn clone_context(parent_pid: usize, child_pid: usize) -> Option<SecurityContext> {
    let mut table = SECURITY_CONTEXTS.lock();
    let parent = table.iter().find(|entry| entry.pid == parent_pid)?.context.clone();
    if let Some(existing) = table.iter_mut().find(|entry| entry.pid == child_pid) {
        existing.context = parent.clone();
    } else {
        table.push(SecurityEntry::new(child_pid, parent.clone()));
    }
    Some(parent)
}

pub fn get_context(pid: usize) -> Option<SecurityContext> {
    SECURITY_CONTEXTS
        .lock()
        .iter()
        .find(|entry| entry.pid == pid)
        .map(|entry| entry.context.clone())
}

pub fn set_capabilities(pid: usize, capabilities: CapMask) {
    if let Some(entry) = SECURITY_CONTEXTS
        .lock()
        .iter_mut()
        .find(|entry| entry.pid == pid)
    {
        entry.context.capabilities = capabilities;
    }
}

pub fn grant_capabilities(pid: usize, mask: CapMask) {
    if let Some(entry) = SECURITY_CONTEXTS
        .lock()
        .iter_mut()
        .find(|entry| entry.pid == pid)
    {
        entry.context.grant_capabilities(mask);
    }
}

pub fn clone_context_with_caps_subset(
    parent_pid: usize,
    child_pid: usize,
    caps_subset: CapMask,
) -> Option<SecurityContext> {
    let parent = get_context(parent_pid)?;
    let child = parent.inherit_child(caps_subset);
    register_process(child_pid, child.clone());
    Some(child)
}

pub fn set_umask(pid: usize, umask: u16) {
    if let Some(entry) = SECURITY_CONTEXTS
        .lock()
        .iter_mut()
        .find(|entry| entry.pid == pid)
    {
        entry.context.set_umask(umask);
    }
}

pub fn remove_context(pid: usize) {
    let mut table = SECURITY_CONTEXTS.lock();
    if let Some(index) = table.iter().position(|entry| entry.pid == pid) {
        table.swap_remove(index);
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_registration_and_retrieval() {
        register_process(1, SecurityContext::as_user(1000));
        let ctx = get_context(1).unwrap();
        assert_eq!(ctx.uid, 1000);
        assert_eq!(ctx.privilege, PrivilegeLevel::Ring3);
    }

    #[test]
    fn test_clone_context() {
        register_process(2, SecurityContext::as_user(42));
        let cloned = clone_context(2, 3).unwrap();
        assert_eq!(cloned.uid, 42);
        assert!(get_context(3).is_some());
    }

    #[test]
    fn test_capability_grants_and_subsets() {
        register_process(10, SecurityContext::as_user(1).with_capabilities(CAP_PROC_MANAGE | CAP_CONSOLE_IO));

        grant_capabilities(10, CAP_VM_MANAGE);
        let ctx = get_context(10).unwrap();
        assert!(ctx.has_capabilities(CAP_VM_MANAGE));

        let child = clone_context_with_caps_subset(10, 11, CAP_CONSOLE_IO).unwrap();
        assert_eq!(child.capabilities, CAP_CONSOLE_IO);
    }
}
