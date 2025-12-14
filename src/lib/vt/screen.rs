use alloc::vec;
use alloc::vec::Vec;

#[derive(Debug, Clone, Copy, PartialEq)]
pub enum Color {
    Indexed(u8),
    RGB(u8, u8, u8),
}

impl Default for Color {
    fn default() -> Self {
        Color::Indexed(7) // Light Grey
    }
}

#[derive(Debug, Clone, Copy, PartialEq)]
pub struct Attr {
    pub fg: Color,
    pub bg: Color,
    pub bold: bool,
    pub underline: bool,
    pub blink: bool,
    pub reverse: bool,
}

impl Default for Attr {
    fn default() -> Self {
        Attr {
            fg: Color::Indexed(7), // Light Grey
            bg: Color::Indexed(0), // Black
            bold: false,
            underline: false,
            blink: false,
            reverse: false,
        }
    }
}

#[derive(Debug, Clone, Copy, PartialEq)]
pub struct Cell {
    pub char: char,
    pub attr: Attr,
}

impl Default for Cell {
    fn default() -> Self {
        Cell {
            char: ' ',
            attr: Attr::default(),
        }
    }
}

pub struct ScreenBuffer {
    pub width: usize,
    pub height: usize,
    pub buffer: Vec<Cell>,
    pub cursor_x: usize,
    pub cursor_y: usize,
    pub current_attr: Attr,
    pub saved_cursor_x: usize,
    pub saved_cursor_y: usize,
    pub saved_attr: Attr,
}

impl ScreenBuffer {
    pub fn new(width: usize, height: usize) -> Self {
        let size = width * height;
        ScreenBuffer {
            width,
            height,
            buffer: vec![Cell::default(); size],
            cursor_x: 0,
            cursor_y: 0,
            current_attr: Attr::default(),
            saved_cursor_x: 0,
            saved_cursor_y: 0,
            saved_attr: Attr::default(),
        }
    }

    pub fn resize(&mut self, width: usize, height: usize) {
        self.width = width;
        self.height = height;
        let size = width * height;
        self.buffer.resize(size, Cell::default());
        if self.cursor_x >= width {
            self.cursor_x = width.saturating_sub(1);
        }
        if self.cursor_y >= height {
            self.cursor_y = height.saturating_sub(1);
        }
    }

    pub fn write_char(&mut self, c: char) {
        if c == '\n' {
            self.new_line();
            return;
        }
        if c == '\r' {
            self.cursor_x = 0;
            return;
        }
        // Handle backspace
        if c == '\x08' {
            if self.cursor_x > 0 {
                self.cursor_x -= 1;
            }
            return;
        }

        if self.cursor_x >= self.width {
            self.new_line();
        }
        
        let idx = self.cursor_y * self.width + self.cursor_x;
        if idx < self.buffer.len() {
            self.buffer[idx] = Cell {
                char: c,
                attr: self.current_attr,
            };
        }
        self.cursor_x += 1;
    }

    pub fn new_line(&mut self) {
        self.cursor_x = 0;
        self.cursor_y += 1;
        if self.cursor_y >= self.height {
            self.scroll_up();
            self.cursor_y = self.height - 1;
        }
    }

    fn scroll_up(&mut self) {
        let row_count = self.height;
        let col_count = self.width;
        
        for y in 0..row_count - 1 {
            for x in 0..col_count {
                self.buffer[y * col_count + x] = self.buffer[(y + 1) * col_count + x];
            }
        }
        
        let last_row_start = (row_count - 1) * col_count;
        for x in 0..col_count {
            self.buffer[last_row_start + x] = Cell {
                char: ' ',
                attr: Attr { bg: self.current_attr.bg, ..Attr::default() }, 
            };
        }
    }

    pub fn clear_screen(&mut self) {
        for cell in self.buffer.iter_mut() {
            *cell = Cell {
                char: ' ',
                attr: Attr { bg: self.current_attr.bg, ..Attr::default() },
            };
        }
        self.cursor_x = 0;
        self.cursor_y = 0;
    }

    pub fn clear_line(&mut self, mode: u8) {
        let row_start = self.cursor_y * self.width;
        match mode {
            0 => { // Clear from cursor to end
                for x in self.cursor_x..self.width {
                    self.buffer[row_start + x] = Cell { char: ' ', attr: self.current_attr };
                }
            }
            1 => { // Clear from start to cursor
                for x in 0..=self.cursor_x {
                    self.buffer[row_start + x] = Cell { char: ' ', attr: self.current_attr };
                }
            }
            2 => { // Clear whole line
                for x in 0..self.width {
                    self.buffer[row_start + x] = Cell { char: ' ', attr: self.current_attr };
                }
            }
            _ => {}
        }
    }

    pub fn set_cursor(&mut self, x: usize, y: usize) {
        self.cursor_x = x.min(self.width.saturating_sub(1));
        self.cursor_y = y.min(self.height.saturating_sub(1));
    }

    pub fn save_cursor(&mut self) {
        self.saved_cursor_x = self.cursor_x;
        self.saved_cursor_y = self.cursor_y;
        self.saved_attr = self.current_attr;
    }

    pub fn restore_cursor(&mut self) {
        self.cursor_x = self.saved_cursor_x;
        self.cursor_y = self.saved_cursor_y;
        self.current_attr = self.saved_attr;
    }
}
