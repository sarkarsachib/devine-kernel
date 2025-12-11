use core::mem;
use core::arch::asm;

#[repr(C, packed)]
pub struct GdtEntry {
    pub limit_low: u16,
    pub base_low: u16,
    pub base_mid: u8,
    pub access: u8,
    pub limit_high_flags: u8,
    pub base_high: u8,
}

#[repr(C, packed)]
pub struct GdtPointer {
    pub limit: u16,
    pub base: u32,
}

pub const GDT_SIZE: usize = 5;

#[repr(C, align(16))]
pub struct Gdt {
    pub entries: [GdtEntry; GDT_SIZE],
}

impl Gdt {
    pub fn new() -> Self {
        Gdt {
            entries: [
                GdtEntry {
                    limit_low: 0,
                    base_low: 0,
                    base_mid: 0,
                    access: 0,
                    limit_high_flags: 0,
                    base_high: 0,
                },
                GdtEntry {
                    limit_low: 0xFFFF,
                    base_low: 0,
                    base_mid: 0,
                    access: 0x9A,
                    limit_high_flags: 0xCF,
                    base_high: 0,
                },
                GdtEntry {
                    limit_low: 0xFFFF,
                    base_low: 0,
                    base_mid: 0,
                    access: 0x92,
                    limit_high_flags: 0xCF,
                    base_high: 0,
                },
                GdtEntry {
                    limit_low: 0xFFFF,
                    base_low: 0,
                    base_mid: 0,
                    access: 0xFA,
                    limit_high_flags: 0xCF,
                    base_high: 0,
                },
                GdtEntry {
                    limit_low: 0xFFFF,
                    base_low: 0,
                    base_mid: 0,
                    access: 0xF2,
                    limit_high_flags: 0xCF,
                    base_high: 0,
                },
            ],
        }
    }

    pub fn load(&self) {
        let ptr = GdtPointer {
            limit: (mem::size_of::<Gdt>() - 1) as u16,
            base: self as *const _ as u32,
        };

        unsafe {
            asm!("lgdt [{}]", in(reg) &ptr, options(nostack, preserves_flags));
            asm!("mov ax, 0x10");
            asm!("mov ds, ax");
            asm!("mov es, ax");
            asm!("mov fs, ax");
            asm!("mov gs, ax");
            asm!("mov ss, ax");
        }
    }
}
