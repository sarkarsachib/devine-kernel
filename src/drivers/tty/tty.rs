//! TTY Device Driver
//!
//! This module provides the main TTY device driver that combines UART hardware
//! access with line discipline and terminal emulation.

use super::{line_discipline::{LineDiscipline, Signal, Termios}, ansi::{AnsiParser, AnsiCommand, TerminalAttributes}};
use crate::drivers::serial::{SerialPort, SERIAL1};

/// TTY device structure
pub struct TtyDevice {
    name: &'static str,
    minor: u32,
    line_discipline: LineDiscipline,
    ansi_parser: AnsiParser,
    terminal_attrs: TerminalAttributes,
    serial: SerialPort,
    input_buffer: [u8; 1024],
    output_buffer: [u8; 4096],
    input_pos: usize,
    output_pos: usize,
    is_open: bool,
}

impl TtyDevice {
    /// Create a new TTY device
    pub fn new(name: &'static str, minor: u32, serial: SerialPort) -> Self {
        Self {
            name,
            minor,
            line_discipline: LineDiscipline::default(),
            ansi_parser: AnsiParser::new(),
            terminal_attrs: TerminalAttributes::default(),
            serial,
            input_buffer: [0u8; 1024],
            output_buffer: [0u8; 4096],
            input_pos: 0,
            output_pos: 0,
            is_open: false,
        }
    }

    /// Initialize the TTY device
    pub fn init(&mut self) {
        self.serial.init();
        self.line_discipline.set_canonical();
        self.is_open = true;
    }

    /// Open the TTY device
    pub fn open(&mut self) {
        self.is_open = true;
    }

    /// Close the TTY device
    pub fn close(&mut self) {
        self.is_open = false;
    }

    /// Check if TTY is open
    pub fn is_open(&self) -> bool {
        self.is_open
    }

    /// Get the minor device number
    pub fn minor(&self) -> u32 {
        self.minor
    }

    /// Get the device name
    pub fn name(&self) -> &'static str {
        self.name
    }

    /// Read from the TTY
    pub fn read(&mut self, buffer: &mut [u8]) -> usize {
        if !self.is_open {
            return 0;
        }

        let mut count = 0;

        // Read from line discipline
        while count < buffer.len() {
            if let Some(byte) = self.line_discipline.read_byte() {
                buffer[count] = byte;
                count += 1;
            } else {
                break;
            }
        }

        count
    }

    /// Write to the TTY
    pub fn write(&mut self, data: &[u8]) -> usize {
        if !self.is_open {
            return 0;
        }

        let mut count = 0;
        for &byte in data {
            if self.output_pos >= self.output_buffer.len() {
                self.flush_output();
            }

            // Check for ANSI escape sequences
            if byte == 0x1B {
                self.output_buffer[self.output_pos] = byte;
                self.output_pos += 1;
            } else if self.output_pos > 0 && self.output_buffer[self.output_pos - 1] == 0x1B {
                self.output_buffer[self.output_pos] = byte;
                self.output_pos += 1;

                // Process complete escape sequence
                if self.output_pos >= 2 {
                    if let Some(cmd) = self.ansi_parser.process_byte(byte) {
                        self.execute_ansi_command(cmd);
                        self.output_pos = 0;  // Clear escape sequence buffer
                    }
                }
            } else {
                self.output_buffer[self.output_pos] = byte;
                self.output_pos += 1;
            }
            count += 1;
        }

        count
    }

    /// Flush output buffer to serial port
    pub fn flush_output(&mut self) {
        if self.output_pos > 0 {
            self.serial.send_buffer(&self.output_buffer[..self.output_pos]);
            self.output_pos = 0;
        }
    }

    /// Handle received data from serial port
    pub fn handle_rx_data(&mut self, data: &[u8]) {
        for &byte in data {
            self.line_discipline.receive_byte(byte);
        }
    }

    /// Process pending signals
    pub fn check_signal(&mut self) -> Option<Signal> {
        self.line_discipline.signal_pending()
    }

    /// Execute ANSI command
    fn execute_ansi_command(&mut self, cmd: AnsiCommand) {
        match cmd {
            AnsiCommand::SetTitle(title) => {
                // Terminal title is stored but would be displayed by the host terminal
            }
            AnsiCommand::SetCursorMode(visible) => {
                self.terminal_attrs.cursor_visible = visible;
            }
            _ => {
                // Other commands are handled by the terminal emulator
            }
        }
    }

    /// Get terminal attributes
    pub fn termios(&self) -> &Termios {
        self.line_discipline.termios()
    }

    /// Get mutable terminal attributes
    pub fn termios_mut(&mut self) -> &mut Termios {
        self.line_discipline.termios_mut()
    }

    /// Set raw mode
    pub fn set_raw_mode(&mut self) {
        self.line_discipline.set_raw();
    }

    /// Set canonical mode
    pub fn set_canonical_mode(&mut self) {
        self.line_discipline.set_canonical();
    }

    /// Check if data is available
    pub fn has_data(&self) -> bool {
        self.line_discipline.has_data()
    }

    /// Get available bytes count
    pub fn available(&self) -> usize {
        self.line_discipline.available()
    }
}

/// TTY console device (COM1)
pub static mut TTY_CONSOLE: TtyDevice = TtyDevice::new("ttyS0", 0, SERIAL1);

/// Get the console TTY
pub fn console() -> &'static mut TtyDevice {
    unsafe { &mut TTY_CONSOLE }
}
