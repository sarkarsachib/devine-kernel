pub const GICD_BASE: usize = 0x08000000;
pub const GICC_BASE: usize = 0x08010000;

pub struct Gic {
    gicd_base: usize,
    gicc_base: usize,
}

impl Gic {
    pub const fn new() -> Self {
        Self {
            gicd_base: GICD_BASE,
            gicc_base: GICC_BASE,
        }
    }

    pub unsafe fn init(&mut self) {
        let gicd_ctlr = self.gicd_base as *mut u32;
        gicd_ctlr.write_volatile(1);

        let gicc_ctlr = self.gicc_base as *mut u32;
        gicc_ctlr.write_volatile(1);

        let gicc_pmr = (self.gicc_base + 0x04) as *mut u32;
        gicc_pmr.write_volatile(0xF0);
    }

    pub unsafe fn enable_interrupt(&mut self, irq: u32) {
        let reg = (self.gicd_base + 0x100 + (irq / 32) as usize * 4) as *mut u32;
        reg.write_volatile(reg.read_volatile() | (1 << (irq % 32)));
    }
}

pub fn init() {
    unsafe {
        let mut gic = Gic::new();
        gic.init();
    }
}
