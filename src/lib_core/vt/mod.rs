//! VT100/ANSI Emulator

pub mod parser;
pub mod screen;

#[cfg(test)]
mod tests;

use crate::lib_core::vt::parser::{Parser, Action};
use crate::lib_core::vt::screen::{ScreenBuffer, Color, Attr, Cell};
use alloc::vec::Vec;

pub trait TerminalDriver {
    fn draw_cell(&mut self, x: usize, y: usize, cell: Cell);
    fn move_cursor(&mut self, x: usize, y: usize);
    fn clear_screen(&mut self);
    fn set_title(&mut self, title: &str);
}

pub struct VtTerminal {
    parser: Parser,
    pub primary_buffer: ScreenBuffer,
    pub alternate_buffer: ScreenBuffer,
    pub is_alternate: bool,
    width: usize,
    height: usize,
    pub mouse_reporting: bool,
    pub bracketed_paste: bool,
    osc_buffer: Vec<u8>,
}

impl VtTerminal {
    pub fn new(width: usize, height: usize) -> Self {
        VtTerminal {
            parser: Parser::new(),
            primary_buffer: ScreenBuffer::new(width, height),
            alternate_buffer: ScreenBuffer::new(width, height),
            is_alternate: false,
            width,
            height,
            mouse_reporting: false,
            bracketed_paste: false,
            osc_buffer: Vec::new(),
        }
    }

    pub fn resize(&mut self, width: usize, height: usize) {
        self.width = width;
        self.height = height;
        self.primary_buffer.resize(width, height);
        self.alternate_buffer.resize(width, height);
    }

    pub fn process_byte(&mut self, byte: u8) {
        let mut actions = alloc::vec::Vec::new();
        self.parser.advance(byte, |action| actions.push(action));
        
        for action in actions {
            self.handle_action(action);
        }
    }

    pub fn render(&self, driver: &mut dyn TerminalDriver) {
        let buffer = if self.is_alternate {
            &self.alternate_buffer
        } else {
            &self.primary_buffer
        };
        
        for y in 0..self.height {
            for x in 0..self.width {
                let idx = y * self.width + x;
                if idx < buffer.buffer.len() {
                    driver.draw_cell(x, y, buffer.buffer[idx]);
                }
            }
        }
        driver.move_cursor(buffer.cursor_x, buffer.cursor_y);
    }

    fn handle_action(&mut self, action: Action) {
        // OSC handling
        match action {
            Action::OscStart => {
                self.osc_buffer.clear();
                return;
            }
            Action::OscPut(b) => {
                self.osc_buffer.push(b);
                return;
            }
            Action::OscEnd => {
                self.handle_osc();
                return;
            }
            _ => {}
        }

        let buffer = if self.is_alternate {
            &mut self.alternate_buffer
        } else {
            &mut self.primary_buffer
        };

        match action {
            Action::Print(c) => buffer.write_char(c),
            Action::Execute(b) => {
                 match b {
                     0x08 => { // BS
                         if buffer.cursor_x > 0 { buffer.cursor_x -= 1; }
                     }
                     0x0A | 0x0B | 0x0C => buffer.new_line(), // LF, VT, FF
                     0x0D => buffer.cursor_x = 0, // CR
                     _ => {}
                 }
            }
            Action::CsiDispatch(params, _intermediates, _ignore, char, private) => {
                self.handle_csi(params, private, char);
            }
            Action::EscDispatch(intermediates, _ignore, byte) => {
                 if intermediates.is_empty() {
                     match byte {
                         0x45 => buffer.new_line(), // NEL
                         0x4D => buffer.cursor_y = buffer.cursor_y.saturating_sub(1), // RI (Reverse Index)
                         _ => {}
                     }
                 }
            }
            _ => {}
        }
    }

    fn handle_csi(&mut self, params: Vec<i64>, private: Option<char>, char: char) {
        if char == 'h' || char == 'l' {
             self.handle_mode(params, private, char == 'h');
             return;
        }

        let buffer = if self.is_alternate {
            &mut self.alternate_buffer
        } else {
            &mut self.primary_buffer
        };

        match char {
            'A' => { // CUU
                 let n = params.get(0).cloned().unwrap_or(1).max(1) as usize;
                 buffer.cursor_y = buffer.cursor_y.saturating_sub(n);
            }
            'B' => { // CUD
                 let n = params.get(0).cloned().unwrap_or(1).max(1) as usize;
                 buffer.cursor_y = (buffer.cursor_y + n).min(buffer.height - 1);
            }
            'C' => { // CUF
                 let n = params.get(0).cloned().unwrap_or(1).max(1) as usize;
                 buffer.cursor_x = (buffer.cursor_x + n).min(buffer.width - 1);
            }
            'D' => { // CUB
                 let n = params.get(0).cloned().unwrap_or(1).max(1) as usize;
                 buffer.cursor_x = buffer.cursor_x.saturating_sub(n);
            }
            'H' | 'f' => { // CUP
                 let y = params.get(0).cloned().unwrap_or(1).max(1) as usize;
                 let x = params.get(1).cloned().unwrap_or(1).max(1) as usize;
                 buffer.set_cursor(x - 1, y - 1);
            }
            'J' => { // ED
                 let mode = params.get(0).cloned().unwrap_or(0) as u8;
                 match mode {
                     2 => buffer.clear_screen(),
                     _ => {} 
                 }
            }
            'K' => { // EL
                 let mode = params.get(0).cloned().unwrap_or(0) as u8;
                 buffer.clear_line(mode);
            }
            'm' => { // SGR
                 let mut i = 0;
                 while i < params.len() {
                     let param = params[i];
                     match param {
                         0 => buffer.current_attr = Attr::default(),
                         1 => buffer.current_attr.bold = true,
                         4 => buffer.current_attr.underline = true,
                         5 => buffer.current_attr.blink = true,
                         7 => buffer.current_attr.reverse = true,
                         30..=37 => buffer.current_attr.fg = Color::Indexed((param - 30) as u8),
                         38 => { // Extended FG
                             if i + 2 < params.len() && params[i+1] == 5 {
                                 buffer.current_attr.fg = Color::Indexed(params[i+2] as u8);
                                 i += 2;
                             } else if i + 4 < params.len() && params[i+1] == 2 {
                                 buffer.current_attr.fg = Color::RGB(params[i+2] as u8, params[i+3] as u8, params[i+4] as u8);
                                 i += 4;
                             }
                         }
                         40..=47 => buffer.current_attr.bg = Color::Indexed((param - 40) as u8),
                         48 => { // Extended BG
                             if i + 2 < params.len() && params[i+1] == 5 {
                                 buffer.current_attr.bg = Color::Indexed(params[i+2] as u8);
                                 i += 2;
                             } else if i + 4 < params.len() && params[i+1] == 2 {
                                 buffer.current_attr.bg = Color::RGB(params[i+2] as u8, params[i+3] as u8, params[i+4] as u8);
                                 i += 4;
                             }
                         }
                         90..=97 => buffer.current_attr.fg = Color::Indexed((param - 90 + 8) as u8),
                         100..=107 => buffer.current_attr.bg = Color::Indexed((param - 100 + 8) as u8),
                         _ => {}
                     }
                     i += 1;
                 }
            }
            's' => buffer.save_cursor(),
            'u' => buffer.restore_cursor(),
            _ => {}
        }
    }

    fn handle_mode(&mut self, params: Vec<i64>, private: Option<char>, set: bool) {
        if let Some('?') = private {
            for param in params {
                 match param {
                     47 | 1047 | 1049 => {
                         if set {
                             self.is_alternate = true;
                             if param == 1049 {
                                 self.primary_buffer.save_cursor();
                                 self.alternate_buffer.clear_screen();
                             }
                         } else {
                             self.is_alternate = false;
                             if param == 1049 {
                                 self.primary_buffer.restore_cursor();
                             }
                         }
                     }
                     25 => { /* Show/Hide Cursor */ }
                     1000 | 1002 | 1006 | 1015 => {
                         self.mouse_reporting = set;
                     }
                     2004 => self.bracketed_paste = set,
                     _ => {}
                 }
            }
        }
    }

    fn handle_osc(&mut self) {
        // Parse OSC buffer. E.g. "0;Title"
        // Split by ';'
        // This is a bit manual in no_std
        let mut parts = self.osc_buffer.split(|&b| b == b';');
        if let Some(p0) = parts.next() {
            // Parse p0 as number
            let mut cmd = 0;
            for &d in p0 {
                if d >= b'0' && d <= b'9' {
                    cmd = cmd * 10 + (d - b'0') as u64;
                }
            }
            
            if cmd == 0 || cmd == 2 {
                // Set window title
                // Remaining parts form the title
                // Just treating it as byte string for now, but should handle properly
                // For now, we can ignore as we don't have a window system, but
                // could pass to driver if it wants it.
            }
        }
    }
}
