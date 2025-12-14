use super::*;
use crate::lib::vt::screen::{Color, Cell};

struct MockDriver;
impl TerminalDriver for MockDriver {
    fn draw_cell(&mut self, _x: usize, _y: usize, _cell: Cell) {}
    fn move_cursor(&mut self, _x: usize, _y: usize) {}
    fn clear_screen(&mut self) {}
    fn set_title(&mut self, _title: &str) {}
}

#[test]
fn test_simple_chars() {
    let mut term = VtTerminal::new(80, 25);
    term.process_byte(b'A');
    assert_eq!(term.primary_buffer.buffer[0].char, 'A');
    assert_eq!(term.primary_buffer.cursor_x, 1);
}

#[test]
fn test_color() {
    let mut term = VtTerminal::new(80, 25);
    // CSI 31 m (Red FG)
    let bytes = b"\x1b[31m";
    for &b in bytes {
        term.process_byte(b);
    }
    term.process_byte(b'A');
    assert_eq!(term.primary_buffer.buffer[0].attr.fg, Color::Indexed(1));
}

#[test]
fn test_cursor_move() {
    let mut term = VtTerminal::new(80, 25);
    // CSI 5;5 H
    let bytes = b"\x1b[5;5H";
    for &b in bytes {
        term.process_byte(b);
    }
    assert_eq!(term.primary_buffer.cursor_x, 4);
    assert_eq!(term.primary_buffer.cursor_y, 4);
}

#[test]
fn test_alternate_buffer() {
    let mut term = VtTerminal::new(80, 25);
    term.process_byte(b'A');
    
    // Switch to alt: CSI ? 1049 h
    let bytes = b"\x1b[?1049h";
    for &b in bytes {
        term.process_byte(b);
    }
    
    assert!(term.is_alternate);
    term.process_byte(b'B');
    assert_eq!(term.alternate_buffer.buffer[0].char, 'B');
    assert_eq!(term.primary_buffer.buffer[0].char, 'A'); // Primary untouched
    
    // Switch back
    let bytes = b"\x1b[?1049l";
    for &b in bytes {
        term.process_byte(b);
    }
    
    assert!(!term.is_alternate);
}
