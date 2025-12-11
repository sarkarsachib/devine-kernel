use volatile::Volatile;

const VGA_BUFFER: usize = 0xb8000;
const BUFFER_HEIGHT: usize = 25;
const BUFFER_WIDTH: usize = 80;

#[repr(transparent)]
struct Buffer {
    chars: [[Volatile<ScreenChar>; BUFFER_WIDTH]; BUFFER_HEIGHT],
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[repr(C)]
struct ScreenChar {
    ascii_character: u8,
    color_code: u8,
}

pub struct Writer {
    column_position: usize,
    row_position: usize,
    color_code: u8,
    buffer: &'static mut Buffer,
}

impl Writer {
    pub fn new() -> Self {
        Self {
            column_position: 0,
            row_position: 0,
            color_code: 0x0f,
            buffer: unsafe { &mut *(VGA_BUFFER as *mut Buffer) },
        }
    }

    pub fn write_byte(&mut self, byte: u8) {
        match byte {
            b'\n' => self.new_line(),
            byte => {
                if self.column_position >= BUFFER_WIDTH {
                    self.new_line();
                }

                let row = self.row_position;
                let col = self.column_position;

                self.buffer.chars[row][col].write(ScreenChar {
                    ascii_character: byte,
                    color_code: self.color_code,
                });
                self.column_position += 1;
            }
        }
    }

    fn new_line(&mut self) {
        self.row_position += 1;
        self.column_position = 0;
    }
}
