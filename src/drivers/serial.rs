
use crate::lib::spinlock::Spinlock;

const COM1: u16 = 0x3F8;

pub struct SerialPort {
    port: u16,
}

impl SerialPort {
    pub const fn new(port: u16) -> Self {
        Self { port }
    }

    pub fn init(&mut self) {
        #[cfg(target_arch = "x86_64")]
        unsafe {
            outb(self.port + 1, 0x00); // Disable all interrupts
            outb(self.port + 3, 0x80); // Enable DLAB (set baud rate divisor)
            outb(self.port + 0, 0x03); // Set divisor to 3 (lo byte) 38400 baud
            outb(self.port + 1, 0x00); //                  (hi byte)
            outb(self.port + 3, 0x03); // 8 bits, no parity, one stop bit
            outb(self.port + 2, 0xC7); // Enable FIFO, clear them, with 14-byte threshold
            outb(self.port + 4, 0x0B); // IRQs enabled, RTS/DSR set
            outb(self.port + 1, 0x01); // Enable interrupts
        }
    }

    pub fn send(&mut self, data: u8) {
        #[cfg(target_arch = "x86_64")]
        unsafe {
            while (inb(self.port + 5) & 0x20) == 0 {}
            outb(self.port, data);
        }
    }

    pub fn receive(&mut self) -> u8 {
        #[cfg(target_arch = "x86_64")]
        unsafe {
            while (inb(self.port + 5) & 1) == 0 {}
            inb(self.port)
        }
        #[cfg(not(target_arch = "x86_64"))]
        0
    }
    
    pub fn try_receive(&mut self) -> Option<u8> {
        #[cfg(target_arch = "x86_64")]
        unsafe {
             if (inb(self.port + 5) & 1) == 0 {
                 None
             } else {
                 Some(inb(self.port))
             }
        }
        #[cfg(not(target_arch = "x86_64"))]
        None
    }
}

pub static SERIAL1: Spinlock<SerialPort> = Spinlock::new(SerialPort::new(COM1));

#[cfg(target_arch = "x86_64")]
unsafe fn outb(port: u16, val: u8) {
    core::arch::asm!("out dx, al", in("dx") port, in("al") val, options(nomem, nostack));
}

#[cfg(target_arch = "x86_64")]
unsafe fn inb(port: u16) -> u8 {
    let ret: u8;
    core::arch::asm!("in al, dx", out("al") ret, in("dx") port, options(nomem, nostack));
    ret
}
