pub mod x86_64_paging;
pub mod arm_lpae;

use super::{PhysAddr, VirtAddr, PAGE_SIZE};
use core::ops::{Index, IndexMut};

pub use x86_64_paging::X86_64PageTable;

bitflags::bitflags! {
    #[derive(Debug, Clone, Copy)]
    pub struct PageFlags: u64 {
        const PRESENT = 1 << 0;
        const WRITABLE = 1 << 1;
        const USER_ACCESSIBLE = 1 << 2;
        const WRITE_THROUGH = 1 << 3;
        const NO_CACHE = 1 << 4;
        const ACCESSED = 1 << 5;
        const DIRTY = 1 << 6;
        const HUGE_PAGE = 1 << 7;
        const GLOBAL = 1 << 8;
        const NO_EXECUTE = 1 << 63;
    }
}

#[derive(Debug, Clone, Copy)]
pub struct Page {
    pub start_address: VirtAddr,
}

impl Page {
    pub fn containing_address(address: VirtAddr) -> Page {
        Page {
            start_address: VirtAddr(address.0 & !((PAGE_SIZE as u64) - 1)),
        }
    }

    pub fn p4_index(&self) -> usize {
        ((self.start_address.0 >> 39) & 0x1FF) as usize
    }

    pub fn p3_index(&self) -> usize {
        ((self.start_address.0 >> 30) & 0x1FF) as usize
    }

    pub fn p2_index(&self) -> usize {
        ((self.start_address.0 >> 21) & 0x1FF) as usize
    }

    pub fn p1_index(&self) -> usize {
        ((self.start_address.0 >> 12) & 0x1FF) as usize
    }
}

#[repr(C)]
#[derive(Debug, Clone, Copy)]
pub struct PageTableEntry {
    entry: u64,
}

impl PageTableEntry {
    pub const fn new() -> Self {
        PageTableEntry { entry: 0 }
    }

    pub fn is_unused(&self) -> bool {
        self.entry == 0
    }

    pub fn set_unused(&mut self) {
        self.entry = 0;
    }

    pub fn flags(&self) -> PageFlags {
        PageFlags::from_bits_truncate(self.entry)
    }

    pub fn addr(&self) -> PhysAddr {
        PhysAddr(self.entry & 0x000f_ffff_ffff_f000)
    }

    pub fn set(&mut self, addr: PhysAddr, flags: PageFlags) {
        self.entry = addr.0 | flags.bits();
    }
}

pub const ENTRY_COUNT: usize = 512;

#[repr(C, align(4096))]
pub struct PageTable {
    entries: [PageTableEntry; ENTRY_COUNT],
}

impl PageTable {
    pub const fn new() -> Self {
        PageTable {
            entries: [PageTableEntry::new(); ENTRY_COUNT],
        }
    }

    pub fn zero(&mut self) {
        for entry in self.entries.iter_mut() {
            entry.set_unused();
        }
    }
}

impl Index<usize> for PageTable {
    type Output = PageTableEntry;

    fn index(&self, index: usize) -> &PageTableEntry {
        &self.entries[index]
    }
}

impl IndexMut<usize> for PageTable {
    fn index_mut(&mut self, index: usize) -> &mut PageTableEntry {
        &mut self.entries[index]
    }
}

pub trait PageTableMapper {
    fn map_to(&mut self, page: Page, frame: super::frame_allocator::Frame, flags: PageFlags) -> Result<(), MapError>;
    fn unmap(&mut self, page: Page) -> Result<(), UnmapError>;
    fn translate(&self, addr: VirtAddr) -> Option<PhysAddr>;
}

#[derive(Debug)]
pub enum MapError {
    FrameAllocationFailed,
    PageAlreadyMapped,
}

#[derive(Debug)]
pub enum UnmapError {
    PageNotMapped,
    InvalidPageTable,
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_page_indices() {
        let page = Page::containing_address(VirtAddr::new(0xdeadbeef000));
        assert!(page.p4_index() < 512);
        assert!(page.p3_index() < 512);
        assert!(page.p2_index() < 512);
        assert!(page.p1_index() < 512);
    }

    #[test]
    fn test_page_table_entry() {
        let mut entry = PageTableEntry::new();
        assert!(entry.is_unused());

        entry.set(PhysAddr::new(0x1000), PageFlags::PRESENT | PageFlags::WRITABLE);
        assert!(!entry.is_unused());
        assert_eq!(entry.addr().0, 0x1000);
        assert!(entry.flags().contains(PageFlags::PRESENT));
        assert!(entry.flags().contains(PageFlags::WRITABLE));
    }
}
