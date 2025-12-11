use devine_arch::{PhysAddr, VirtAddr};
use devine_arch::mmu::{MemoryManagement, PageFlags};

pub struct Mmu {
    page_table: usize,
}

impl Mmu {
    pub const fn new() -> Self {
        Self { page_table: 0 }
    }
}

impl MemoryManagement for Mmu {
    fn init(&mut self) {
    }

    fn map_page(&mut self, _virt: VirtAddr, _phys: PhysAddr, _flags: PageFlags) {
    }

    fn unmap_page(&mut self, _virt: VirtAddr) {
    }
}

pub unsafe fn enable_mmu() {
    core::arch::asm!(
        "msr sctlr_el1, {0}",
        in(reg) 0x1 as u64,
        options(nomem, nostack)
    );
}
