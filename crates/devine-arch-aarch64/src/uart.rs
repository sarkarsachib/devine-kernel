use spin::Mutex;
use volatile::Volatile;

const UART0_BASE: usize = 0x09000000;

pub static UART: Mutex<Uart> = Mutex::new(Uart::new(UART0_BASE));

#[repr(C)]
struct UartRegisters {
    dr: Volatile<u32>,
    _rsrecr: Volatile<u32>,
    _reserved1: [u32; 4],
    fr: Volatile<u32>,
}

pub struct Uart {
    registers: *mut UartRegisters,
}

unsafe impl Send for Uart {}

impl Uart {
    pub const fn new(base: usize) -> Self {
        Self {
            registers: base as *mut UartRegisters,
        }
    }

    pub fn init(&mut self) {
    }

    pub fn write_byte(&mut self, byte: u8) {
        unsafe {
            let regs = &mut *self.registers;
            while (regs.fr.read() & (1 << 5)) != 0 {}
            regs.dr.write(byte as u32);
        }
    }

    pub fn write_str(&mut self, s: &str) {
        for byte in s.bytes() {
            self.write_byte(byte);
        }
    }
}

pub fn init() {
    UART.lock().init();
}
