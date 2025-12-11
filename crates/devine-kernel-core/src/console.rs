use core::fmt;
use spin::Mutex;

pub static CONSOLE: Mutex<Console> = Mutex::new(Console::new());

pub struct Console {
    initialized: bool,
}

impl Console {
    pub const fn new() -> Self {
        Self {
            initialized: false,
        }
    }

    pub fn init(&mut self) {
        self.initialized = true;
    }

    pub fn write_str(&mut self, s: &str) {
        if self.initialized {
            for byte in s.bytes() {
                self.write_byte(byte);
            }
        }
    }

    fn write_byte(&mut self, _byte: u8) {
    }
}

impl fmt::Write for Console {
    fn write_str(&mut self, s: &str) -> fmt::Result {
        self.write_str(s);
        Ok(())
    }
}

#[macro_export]
macro_rules! print {
    ($($arg:tt)*) => {
        {
            use core::fmt::Write;
            let mut console = $crate::console::CONSOLE.lock();
            let _ = write!(console, $($arg)*);
        }
    };
}

#[macro_export]
macro_rules! println {
    () => ($crate::print!("\n"));
    ($($arg:tt)*) => {
        $crate::print!("{}\n", format_args!($($arg)*))
    };
}
