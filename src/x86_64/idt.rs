use core::mem;
use core::arch::asm;

#[repr(C, packed)]
#[derive(Clone, Copy)]
pub struct IdtEntry {
    pub offset_low: u16,
    pub selector: u16,
    pub ist_reserved: u8,
    pub flags: u8,
    pub offset_mid: u16,
    pub offset_high: u32,
    pub reserved: u32,
}

#[repr(C, packed)]
pub struct IdtPointer {
    pub limit: u16,
    pub base: u64,
}

pub const IDT_SIZE: usize = 256;

#[repr(C, align(16))]
pub struct Idt {
    pub entries: [IdtEntry; IDT_SIZE],
}

impl Idt {
    pub fn new() -> Self {
        Idt {
            entries: [IdtEntry {
                offset_low: 0,
                selector: 0,
                ist_reserved: 0,
                flags: 0,
                offset_mid: 0,
                offset_high: 0,
                reserved: 0,
            }; IDT_SIZE],
        }
    }

    pub fn load(&self) {
        let ptr = IdtPointer {
            limit: (mem::size_of::<Idt>() - 1) as u16,
            base: self as *const _ as u64,
        };

        unsafe {
            asm!("lidt [{}]", in(reg) &ptr, options(nostack, preserves_flags));
        }
    }

    pub fn set_gate(
        &mut self,
        vector: usize,
        handler: u64,
        selector: u16,
        flags: u8,
    ) {
        if vector < IDT_SIZE {
            let entry = &mut self.entries[vector];
            entry.offset_low = (handler & 0xFFFF) as u16;
            entry.offset_mid = ((handler >> 16) & 0xFFFF) as u16;
            entry.offset_high = ((handler >> 32) & 0xFFFFFFFF) as u32;
            entry.selector = selector;
            entry.flags = flags;
            entry.ist_reserved = 0;
            entry.reserved = 0;
        }
    }
}
