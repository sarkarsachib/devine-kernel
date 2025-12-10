pub mod frame_allocator;
pub mod paging;
pub mod heap;
pub mod numa;

use core::fmt;

#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord)]
#[repr(transparent)]
pub struct PhysAddr(pub u64);

impl PhysAddr {
    pub const fn new(addr: u64) -> Self {
        PhysAddr(addr)
    }

    pub const fn as_u64(self) -> u64 {
        self.0
    }
}

impl fmt::Display for PhysAddr {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "PhysAddr(0x{:x})", self.0)
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord)]
#[repr(transparent)]
pub struct VirtAddr(pub u64);

impl VirtAddr {
    pub const fn new(addr: u64) -> Self {
        VirtAddr(addr)
    }

    pub const fn as_u64(self) -> u64 {
        self.0
    }

    pub fn align_up(&self, align: u64) -> VirtAddr {
        VirtAddr((self.0 + align - 1) & !(align - 1))
    }

    pub fn align_down(&self, align: u64) -> VirtAddr {
        VirtAddr(self.0 & !(align - 1))
    }

    pub fn is_aligned(&self, align: u64) -> bool {
        self.0 % align == 0
    }
}

impl fmt::Display for VirtAddr {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "VirtAddr(0x{:x})", self.0)
    }
}

pub const PAGE_SIZE: usize = 4096;
pub const PAGE_SIZE_2M: usize = 2 * 1024 * 1024;
pub const PAGE_SIZE_1G: usize = 1024 * 1024 * 1024;

#[derive(Debug, Clone, Copy)]
pub struct MemoryRegion {
    pub start: PhysAddr,
    pub size: usize,
}

impl MemoryRegion {
    pub const fn new(start: PhysAddr, size: usize) -> Self {
        Self { start, size }
    }

    pub fn end(&self) -> PhysAddr {
        PhysAddr(self.start.0 + self.size as u64)
    }
}
