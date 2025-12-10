use spin::Mutex;

pub static GDT: Mutex<GlobalDescriptorTable> = Mutex::new(GlobalDescriptorTable::new());

#[repr(C, packed)]
pub struct GlobalDescriptorTable {
    null: u64,
    code: u64,
    data: u64,
}

impl GlobalDescriptorTable {
    pub const fn new() -> Self {
        Self {
            null: 0,
            code: 0x00af9b000000ffff,
            data: 0x00cf93000000ffff,
        }
    }

    pub fn load(&'static self) {
        let ptr = DescriptorTablePointer {
            limit: (core::mem::size_of::<Self>() - 1) as u16,
            base: self as *const _ as u64,
        };

        unsafe {
            core::arch::asm!(
                "lgdt [{}]",
                in(reg) &ptr,
                options(nostack)
            );
        }
    }
}

#[repr(C, packed)]
struct DescriptorTablePointer {
    limit: u16,
    base: u64,
}

pub fn init() {
    let gdt = GDT.lock();
    gdt.load();
}
