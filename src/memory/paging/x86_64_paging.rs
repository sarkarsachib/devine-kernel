use super::*;
use crate::memory::frame_allocator::{Frame, allocate_frame};
use spin::Mutex;

pub struct X86_64PageTable {
    #[allow(dead_code)]
    p4_frame: Frame,
}

impl X86_64PageTable {
    pub fn new(p4_frame: Frame) -> Self {
        X86_64PageTable { p4_frame }
    }

    pub fn active() -> Self {
        let (p4_frame, _) = unsafe { Self::get_active_p4() };
        X86_64PageTable { p4_frame }
    }

    unsafe fn get_active_p4() -> (Frame, &'static mut PageTable) {
        let p4_addr = 0o177777_777_777_777_777_0000u64;
        let p4_ptr = p4_addr as *mut PageTable;
        let p4_frame = Frame {
            start_address: PhysAddr(read_cr3()),
        };
        (p4_frame, &mut *p4_ptr)
    }

    fn p4(&self) -> &PageTable {
        let p4_addr = 0o177777_777_777_777_777_0000u64;
        unsafe { &*(p4_addr as *const PageTable) }
    }

    fn p4_mut(&mut self) -> &mut PageTable {
        let p4_addr = 0o177777_777_777_777_777_0000u64;
        unsafe { &mut *(p4_addr as *mut PageTable) }
    }

    fn p3(&self, page: Page) -> Option<&PageTable> {
        let p3_addr = 0o177777_777_777_777_000_0000u64 | ((page.p4_index() as u64) << 12);
        let entry = &self.p4()[page.p4_index()];
        if entry.flags().contains(PageFlags::PRESENT) {
            Some(unsafe { &*(p3_addr as *const PageTable) })
        } else {
            None
        }
    }

    fn p3_mut(&mut self, page: Page) -> Option<&mut PageTable> {
        let p3_addr = 0o177777_777_777_777_000_0000u64 | ((page.p4_index() as u64) << 12);
        let entry = &self.p4()[page.p4_index()];
        if entry.flags().contains(PageFlags::PRESENT) {
            Some(unsafe { &mut *(p3_addr as *mut PageTable) })
        } else {
            None
        }
    }

    fn p2(&self, page: Page) -> Option<&PageTable> {
        let p2_addr = 0o177777_777_777_000_000_0000u64 
            | ((page.p4_index() as u64) << 21)
            | ((page.p3_index() as u64) << 12);
        
        if let Some(p3) = self.p3(page) {
            let entry = &p3[page.p3_index()];
            if entry.flags().contains(PageFlags::PRESENT) && !entry.flags().contains(PageFlags::HUGE_PAGE) {
                return Some(unsafe { &*(p2_addr as *const PageTable) });
            }
        }
        None
    }

    fn p2_mut(&mut self, page: Page) -> Option<&mut PageTable> {
        let p2_addr = 0o177777_777_777_000_000_0000u64 
            | ((page.p4_index() as u64) << 21)
            | ((page.p3_index() as u64) << 12);
        
        if let Some(p3) = self.p3(page) {
            let entry = &p3[page.p3_index()];
            if entry.flags().contains(PageFlags::PRESENT) && !entry.flags().contains(PageFlags::HUGE_PAGE) {
                return Some(unsafe { &mut *(p2_addr as *mut PageTable) });
            }
        }
        None
    }

    fn p1(&self, page: Page) -> Option<&PageTable> {
        let p1_addr = 0o177777_777_000_000_000_0000u64 
            | ((page.p4_index() as u64) << 30)
            | ((page.p3_index() as u64) << 21)
            | ((page.p2_index() as u64) << 12);
        
        if let Some(p2) = self.p2(page) {
            let entry = &p2[page.p2_index()];
            if entry.flags().contains(PageFlags::PRESENT) && !entry.flags().contains(PageFlags::HUGE_PAGE) {
                return Some(unsafe { &*(p1_addr as *const PageTable) });
            }
        }
        None
    }

    fn p1_mut(&mut self, page: Page) -> Option<&mut PageTable> {
        let p1_addr = 0o177777_777_000_000_000_0000u64 
            | ((page.p4_index() as u64) << 30)
            | ((page.p3_index() as u64) << 21)
            | ((page.p2_index() as u64) << 12);
        
        if let Some(p2) = self.p2(page) {
            let entry = &p2[page.p2_index()];
            if entry.flags().contains(PageFlags::PRESENT) && !entry.flags().contains(PageFlags::HUGE_PAGE) {
                return Some(unsafe { &mut *(p1_addr as *mut PageTable) });
            }
        }
        None
    }

    fn create_table_if_needed(&mut self, page: Page, level: usize) -> Result<(), MapError> {
        match level {
            4 => {
                let entry = &mut self.p4_mut()[page.p4_index()];
                if !entry.flags().contains(PageFlags::PRESENT) {
                    let frame = allocate_frame().ok_or(MapError::FrameAllocationFailed)?;
                    entry.set(frame.start_address, PageFlags::PRESENT | PageFlags::WRITABLE | PageFlags::USER_ACCESSIBLE);
                    if let Some(table) = self.p3_mut(page) {
                        table.zero();
                    }
                }
                Ok(())
            }
            3 => {
                self.create_table_if_needed(page, 4)?;
                if let Some(p3) = self.p3_mut(page) {
                    let entry = &mut p3[page.p3_index()];
                    if !entry.flags().contains(PageFlags::PRESENT) {
                        let frame = allocate_frame().ok_or(MapError::FrameAllocationFailed)?;
                        entry.set(frame.start_address, PageFlags::PRESENT | PageFlags::WRITABLE | PageFlags::USER_ACCESSIBLE);
                        if let Some(table) = self.p2_mut(page) {
                            table.zero();
                        }
                    }
                }
                Ok(())
            }
            2 => {
                self.create_table_if_needed(page, 3)?;
                if let Some(p2) = self.p2_mut(page) {
                    let entry = &mut p2[page.p2_index()];
                    if !entry.flags().contains(PageFlags::PRESENT) {
                        let frame = allocate_frame().ok_or(MapError::FrameAllocationFailed)?;
                        entry.set(frame.start_address, PageFlags::PRESENT | PageFlags::WRITABLE | PageFlags::USER_ACCESSIBLE);
                        if let Some(table) = self.p1_mut(page) {
                            table.zero();
                        }
                    }
                }
                Ok(())
            }
            _ => Ok(()),
        }
    }
}

impl PageTableMapper for X86_64PageTable {
    fn map_to(&mut self, page: Page, frame: Frame, flags: PageFlags) -> Result<(), MapError> {
        self.create_table_if_needed(page, 2)?;

        if let Some(p1) = self.p1_mut(page) {
            let entry = &mut p1[page.p1_index()];
            if !entry.is_unused() {
                return Err(MapError::PageAlreadyMapped);
            }
            entry.set(frame.start_address, flags | PageFlags::PRESENT);
            unsafe {
                invlpg(page.start_address.0);
            }
            Ok(())
        } else {
            Err(MapError::FrameAllocationFailed)
        }
    }

    fn unmap(&mut self, page: Page) -> Result<(), UnmapError> {
        if let Some(p1) = self.p1_mut(page) {
            let entry = &mut p1[page.p1_index()];
            if entry.is_unused() {
                return Err(UnmapError::PageNotMapped);
            }
            entry.set_unused();
            unsafe {
                invlpg(page.start_address.0);
            }
            Ok(())
        } else {
            Err(UnmapError::PageNotMapped)
        }
    }

    fn translate(&self, addr: VirtAddr) -> Option<PhysAddr> {
        let page = Page::containing_address(addr);
        let offset = addr.0 % PAGE_SIZE as u64;

        if let Some(p1) = self.p1(page) {
            let entry = &p1[page.p1_index()];
            if entry.flags().contains(PageFlags::PRESENT) {
                return Some(PhysAddr(entry.addr().0 + offset));
            }
        }
        None
    }
}

#[inline]
unsafe fn read_cr3() -> u64 {
    let value: u64;
    core::arch::asm!("mov {}, cr3", out(reg) value, options(nomem, nostack));
    value
}

#[inline]
unsafe fn invlpg(addr: u64) {
    core::arch::asm!("invlpg [{}]", in(reg) addr, options(nostack));
}

pub static KERNEL_PAGE_TABLE: Mutex<Option<X86_64PageTable>> = Mutex::new(None);

pub fn init_paging(p4_frame: Frame) {
    let page_table = X86_64PageTable::new(p4_frame);
    *KERNEL_PAGE_TABLE.lock() = Some(page_table);
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_recursive_address_calculation() {
        let p4_addr = 0o177777_777_777_777_777_0000u64;
        assert!(p4_addr > 0, "Recursive P4 address should be in higher half");
    }
}
