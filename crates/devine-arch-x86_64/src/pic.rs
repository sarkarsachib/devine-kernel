use spin::Mutex;

pub static PIC: Mutex<Pic8259> = Mutex::new(Pic8259::new());

pub struct Pic8259 {
    master_command: u16,
    master_data: u16,
    slave_command: u16,
    slave_data: u16,
}

impl Pic8259 {
    pub const fn new() -> Self {
        Self {
            master_command: 0x20,
            master_data: 0x21,
            slave_command: 0xA0,
            slave_data: 0xA1,
        }
    }

    pub unsafe fn init(&mut self) {
        let master_mask = inb(self.master_data);
        let slave_mask = inb(self.slave_data);

        outb(self.master_command, 0x11);
        outb(self.slave_command, 0x11);
        outb(self.master_data, 0x20);
        outb(self.slave_data, 0x28);
        outb(self.master_data, 4);
        outb(self.slave_data, 2);
        outb(self.master_data, 0x01);
        outb(self.slave_data, 0x01);

        outb(self.master_data, master_mask);
        outb(self.slave_data, slave_mask);
    }
}

unsafe fn outb(port: u16, value: u8) {
    core::arch::asm!(
        "out dx, al",
        in("dx") port,
        in("al") value,
        options(nomem, nostack, preserves_flags)
    );
}

unsafe fn inb(port: u16) -> u8 {
    let value: u8;
    core::arch::asm!(
        "in al, dx",
        out("al") value,
        in("dx") port,
        options(nomem, nostack, preserves_flags)
    );
    value
}

pub fn init() {
    unsafe {
        PIC.lock().init();
    }
}
