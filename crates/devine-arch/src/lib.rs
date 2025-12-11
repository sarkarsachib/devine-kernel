#![no_std]

pub trait ArchOps {
    fn name() -> &'static str;
    
    fn init();
    
    fn halt_loop() -> !;
    
    fn enable_interrupts();
    
    fn disable_interrupts();
    
    fn wait_for_interrupt();
}

pub mod interrupts {
    pub trait InterruptController {
        fn init(&mut self);
        fn enable(&mut self, irq: u8);
        fn disable(&mut self, irq: u8);
        fn acknowledge(&mut self, irq: u8);
    }
}

pub mod mmu {
    use crate::PhysAddr;
    use crate::VirtAddr;

    pub trait MemoryManagement {
        fn init(&mut self);
        fn map_page(&mut self, virt: VirtAddr, phys: PhysAddr, flags: PageFlags);
        fn unmap_page(&mut self, virt: VirtAddr);
    }

    bitflags::bitflags! {
        pub struct PageFlags: u64 {
            const PRESENT = 1 << 0;
            const WRITABLE = 1 << 1;
            const USER = 1 << 2;
            const EXECUTABLE = 1 << 3;
        }
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord)]
#[repr(transparent)]
pub struct PhysAddr(pub u64);

#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord)]
#[repr(transparent)]
pub struct VirtAddr(pub u64);

impl PhysAddr {
    pub const fn new(addr: u64) -> Self {
        Self(addr)
    }

    pub const fn as_u64(self) -> u64 {
        self.0
    }
}

impl VirtAddr {
    pub const fn new(addr: u64) -> Self {
        Self(addr)
    }

    pub const fn as_u64(self) -> u64 {
        self.0
    }
}
