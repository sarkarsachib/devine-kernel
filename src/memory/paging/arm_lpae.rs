use super::*;
use crate::memory::frame_allocator::{Frame, allocate_frame};

bitflags::bitflags! {
    #[derive(Debug, Clone, Copy)]
    pub struct ArmPageFlags: u64 {
        const VALID = 1 << 0;
        const TABLE = 1 << 1;
        const AF = 1 << 10;
        const NG = 1 << 11;
        const USER_ACCESSIBLE = 1 << 6;
        const READ_ONLY = 1 << 7;
        const OUTER_SHAREABLE = 2 << 8;
        const INNER_SHAREABLE = 3 << 8;
        const NO_EXECUTE = 1 << 54;
        const PRIVILEGED_NO_EXECUTE = 1 << 53;
    }
}

#[repr(C)]
#[derive(Debug, Clone, Copy)]
pub struct ArmPageTableEntry {
    entry: u64,
}

impl ArmPageTableEntry {
    pub const fn new() -> Self {
        ArmPageTableEntry { entry: 0 }
    }

    pub fn is_unused(&self) -> bool {
        self.entry & 1 == 0
    }

    pub fn set_unused(&mut self) {
        self.entry = 0;
    }

    pub fn is_table(&self) -> bool {
        (self.entry & 0x3) == 0x3
    }

    pub fn is_block(&self) -> bool {
        (self.entry & 0x3) == 0x1
    }

    pub fn flags(&self) -> ArmPageFlags {
        ArmPageFlags::from_bits_truncate(self.entry)
    }

    pub fn addr(&self) -> PhysAddr {
        PhysAddr(self.entry & 0x0000_ffff_ffff_f000)
    }

    pub fn set_table(&mut self, addr: PhysAddr) {
        self.entry = addr.0 | ArmPageFlags::VALID.bits() | ArmPageFlags::TABLE.bits();
    }

    pub fn set_block(&mut self, addr: PhysAddr, flags: ArmPageFlags) {
        self.entry = addr.0 | flags.bits() | ArmPageFlags::VALID.bits() | ArmPageFlags::AF.bits();
    }

    pub fn set_page(&mut self, addr: PhysAddr, flags: ArmPageFlags) {
        self.entry = addr.0 | flags.bits() | ArmPageFlags::VALID.bits() | ArmPageFlags::TABLE.bits() | ArmPageFlags::AF.bits();
    }
}

#[repr(C, align(4096))]
pub struct ArmPageTable {
    entries: [ArmPageTableEntry; ENTRY_COUNT],
}

impl ArmPageTable {
    pub const fn new() -> Self {
        ArmPageTable {
            entries: [ArmPageTableEntry::new(); ENTRY_COUNT],
        }
    }

    pub fn zero(&mut self) {
        for entry in self.entries.iter_mut() {
            entry.set_unused();
        }
    }
}

impl Index<usize> for ArmPageTable {
    type Output = ArmPageTableEntry;

    fn index(&self, index: usize) -> &ArmPageTableEntry {
        &self.entries[index]
    }
}

impl IndexMut<usize> for ArmPageTable {
    fn index_mut(&mut self, index: usize) -> &mut ArmPageTableEntry {
        &mut self.entries[index]
    }
}

pub struct ArmLpaePageTable {
    l0_frame: Frame,
}

impl ArmLpaePageTable {
    pub fn new(l0_frame: Frame) -> Self {
        ArmLpaePageTable { l0_frame }
    }

    fn l0_index(addr: VirtAddr) -> usize {
        ((addr.0 >> 39) & 0x1FF) as usize
    }

    fn l1_index(addr: VirtAddr) -> usize {
        ((addr.0 >> 30) & 0x1FF) as usize
    }

    fn l2_index(addr: VirtAddr) -> usize {
        ((addr.0 >> 21) & 0x1FF) as usize
    }

    fn l3_index(addr: VirtAddr) -> usize {
        ((addr.0 >> 12) & 0x1FF) as usize
    }

    unsafe fn get_table(&self, frame: Frame) -> &'static ArmPageTable {
        &*(frame.start_address.0 as *const ArmPageTable)
    }

    unsafe fn get_table_mut(&mut self, frame: Frame) -> &'static mut ArmPageTable {
        &mut *(frame.start_address.0 as *mut ArmPageTable)
    }

    fn convert_flags(flags: PageFlags) -> ArmPageFlags {
        let mut arm_flags = ArmPageFlags::VALID | ArmPageFlags::AF;
        
        if !flags.contains(PageFlags::WRITABLE) {
            arm_flags |= ArmPageFlags::READ_ONLY;
        }
        
        if flags.contains(PageFlags::USER_ACCESSIBLE) {
            arm_flags |= ArmPageFlags::USER_ACCESSIBLE;
        }
        
        if flags.contains(PageFlags::NO_EXECUTE) {
            arm_flags |= ArmPageFlags::NO_EXECUTE;
        }
        
        arm_flags
    }

    fn create_next_level_table(&mut self, entry: &mut ArmPageTableEntry) -> Result<Frame, MapError> {
        if entry.is_unused() {
            let frame = allocate_frame().ok_or(MapError::FrameAllocationFailed)?;
            entry.set_table(frame.start_address);
            unsafe {
                self.get_table_mut(frame).zero();
            }
            Ok(frame)
        } else if entry.is_table() {
            Ok(Frame { start_address: entry.addr() })
        } else {
            Err(MapError::PageAlreadyMapped)
        }
    }

    pub fn map_to_lpae(&mut self, page: Page, frame: Frame, flags: PageFlags) -> Result<(), MapError> {
        let arm_flags = Self::convert_flags(flags);
        
        unsafe {
            let l0_table = self.get_table_mut(self.l0_frame);
            let l0_idx = Self::l0_index(page.start_address);
            let l1_frame = self.create_next_level_table(&mut l0_table[l0_idx])?;

            let l1_table = self.get_table_mut(l1_frame);
            let l1_idx = Self::l1_index(page.start_address);
            let l2_frame = self.create_next_level_table(&mut l1_table[l1_idx])?;

            let l2_table = self.get_table_mut(l2_frame);
            let l2_idx = Self::l2_index(page.start_address);
            let l3_frame = self.create_next_level_table(&mut l2_table[l2_idx])?;

            let l3_table = self.get_table_mut(l3_frame);
            let l3_idx = Self::l3_index(page.start_address);
            
            if !l3_table[l3_idx].is_unused() {
                return Err(MapError::PageAlreadyMapped);
            }

            l3_table[l3_idx].set_page(frame.start_address, arm_flags);
        }

        Ok(())
    }

    pub fn unmap_lpae(&mut self, page: Page) -> Result<(), UnmapError> {
        unsafe {
            let l0_table = self.get_table(self.l0_frame);
            let l0_idx = Self::l0_index(page.start_address);
            if l0_table[l0_idx].is_unused() {
                return Err(UnmapError::PageNotMapped);
            }
            let l1_frame = Frame { start_address: l0_table[l0_idx].addr() };

            let l1_table = self.get_table(l1_frame);
            let l1_idx = Self::l1_index(page.start_address);
            if l1_table[l1_idx].is_unused() {
                return Err(UnmapError::PageNotMapped);
            }
            let l2_frame = Frame { start_address: l1_table[l1_idx].addr() };

            let l2_table = self.get_table(l2_frame);
            let l2_idx = Self::l2_index(page.start_address);
            if l2_table[l2_idx].is_unused() {
                return Err(UnmapError::PageNotMapped);
            }
            let l3_frame = Frame { start_address: l2_table[l2_idx].addr() };

            let l3_table = self.get_table_mut(l3_frame);
            let l3_idx = Self::l3_index(page.start_address);
            
            if l3_table[l3_idx].is_unused() {
                return Err(UnmapError::PageNotMapped);
            }

            l3_table[l3_idx].set_unused();
        }

        Ok(())
    }

    pub fn translate_lpae(&self, addr: VirtAddr) -> Option<PhysAddr> {
        let page = Page::containing_address(addr);
        let offset = addr.0 % PAGE_SIZE as u64;

        unsafe {
            let l0_table = self.get_table(self.l0_frame);
            let l0_entry = &l0_table[Self::l0_index(page.start_address)];
            if l0_entry.is_unused() { return None; }

            let l1_table = self.get_table(Frame { start_address: l0_entry.addr() });
            let l1_entry = &l1_table[Self::l1_index(page.start_address)];
            if l1_entry.is_unused() { return None; }
            if l1_entry.is_block() { return Some(PhysAddr(l1_entry.addr().0 + offset)); }

            let l2_table = self.get_table(Frame { start_address: l1_entry.addr() });
            let l2_entry = &l2_table[Self::l2_index(page.start_address)];
            if l2_entry.is_unused() { return None; }
            if l2_entry.is_block() { return Some(PhysAddr(l2_entry.addr().0 + offset)); }

            let l3_table = self.get_table(Frame { start_address: l2_entry.addr() });
            let l3_entry = &l3_table[Self::l3_index(page.start_address)];
            if l3_entry.is_unused() { return None; }

            Some(PhysAddr(l3_entry.addr().0 + offset))
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_arm_page_table_entry() {
        let mut entry = ArmPageTableEntry::new();
        assert!(entry.is_unused());

        entry.set_table(PhysAddr::new(0x1000));
        assert!(entry.is_table());
        assert_eq!(entry.addr().0, 0x1000);
    }

    #[test]
    fn test_arm_indices() {
        let addr = VirtAddr::new(0x1234_5678_9000);
        assert!(ArmLpaePageTable::l0_index(addr) < 512);
        assert!(ArmLpaePageTable::l1_index(addr) < 512);
        assert!(ArmLpaePageTable::l2_index(addr) < 512);
        assert!(ArmLpaePageTable::l3_index(addr) < 512);
    }
}
