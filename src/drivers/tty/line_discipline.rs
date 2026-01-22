//! TTY/Line Discipline Subsystem
//!
//! This module implements a POSIX-like line discipline for terminal I/O,
//! including input buffering, canonical/raw modes, signal handling, and
//! terminal control.

use core::sync::atomic::AtomicUsize;

/// Special character indices in the termios structure
pub const VINTR: usize = 0;   // Interrupt character (Ctrl+C)
pub const VQUIT: usize = 1;   // Quit character (Ctrl+\)
pub const VERASE: usize = 2;  // Erase character (DEL/Backspace)
pub const VKILL: usize = 3;   // Kill line character (Ctrl+U)
pub const VEOF: usize = 4;    // End-of-file character (Ctrl+D)
pub const VSTART: usize = 5;  // Start output (Ctrl+Q)
pub const VSTOP: usize = 6;   // Stop output (Ctrl+S)
pub const VSUSP: usize = 7;   // Suspend process (Ctrl+Z)
pub const VEOL: usize = 8;    // Additional end-of-line
pub const VEOL2: usize = 9;   // Second EOL character
pub const VWERASE: usize = 10; // Word erase (Ctrl+W)
pub const VREPRINT: usize = 11; // Reprint line (Ctrl+R)
pub const VDISCARD: usize = 12; // Discard (Ctrl+O)
pub const VLNEXT: usize = 13; // Literal next (Ctrl+V)
pub const VSTATUS: usize = 14; // Status request (Ctrl+T)
pub const VMIN: usize = 16;
pub const VTIME: usize = 17;

/// Input modes
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct InputModes(u32);

impl InputModes {
    pub const IGNBRK:  Self = Self(1 << 0);  // Ignore break
    pub const BRKINT:  Self = Self(1 << 1);  // Signal on break
    pub const IGNCR:   Self = Self(1 << 2);  // Ignore carriage return
    pub const ICRNL:   Self = Self(1 << 3);  // Map CR to NL on input
    pub const INLCR:   Self = Self(1 << 4);  // Map NL to CR on input
    pub const INPCK:   Self = Self(1 << 5);  // Check parity
    pub const ISTRIP:  Self = Self(1 << 6);  // Strip 8th bit
    pub const INLCR:   Self = Self(1 << 7);  // Map NL to CR
    pub const IGNCR:   Self = Self(1 << 8);  // Ignore CR
    pub const ICRNL:   Self = Self(1 << 9);  // Map CR to NL
    pub const IUCLC:   Self = Self(1 << 10); // Map uppercase to lowercase
    pub const IXON:    Self = Self(1 << 11); // XON/XOFF flow control on output
    pub const IXANY:   Self = Self(1 << 12); // Any char restarts output
    pub const IXOFF:   Self = Self(1 << 13); // XON/XOFF flow control on input
    pub const IMAXBEL: Self = Self(1 << 14); // Ring bell on input buffer full
    pub const IUTF8:   Self = Self(1 << 15); // UTF-8 mode

    pub fn raw(&self) -> bool {
        self.0 & (Self::ICRNL.0 | Self::INLCR.0 | Self::ISTRIP.0 | 
                  Self::IGNBRK.0 | Self::BRKINT.0) == 0
    }

    pub fn set_raw(&mut self) {
        self.0 &= !(Self::ICRNL.0 | Self::INLCR.0 | Self::ISTRIP.0);
        self.0 |= Self::IGNBRK.0;
    }
}

impl Default for InputModes {
    fn default() -> Self {
        Self(0)
    }
}

/// Output modes
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct OutputModes(u32);

impl OutputModes {
    pub const OPOST:  Self = Self(1 << 0);  // Post-process output
    pub const OLCUC:  Self = Self(1 << 1);  // Map lowercase to uppercase
    pub const ONLCR:  Self = Self(1 << 2);  // Map NL to CR-NL
    pub const OCRNL:  Self = Self(1 << 3);  // Map CR to NL
    pub const ONOCR:  Self = Self(1 << 4);  // No CR at column 0
    pub const ONLRET: Self = Self(1 << 5);  // NL performs CR
    pub const OFILL:  Self = Self(1 << 6);  // Fill with DEL for delays
    pub const OFDEL:  Self = Self(1 << 7);  // Fill char is DEL
    pub const NLDLY:  Self = Self(1 << 8);  // NL delay mask
    pub const TABDLY: Self = Self(1 << 10); // Tab delay mask
    pub const CRDLY:  Self = Self(1 << 12); // CR delay mask
    pub const FFDLY:  Self = Self(1 << 14); // Form feed delay mask
    pub const BSDLY:  Self = Self(1 << 15); // Backspace delay mask

    pub fn set_raw(&mut self) {
        self.0 &= Self::OPOST.0;
    }
}

impl Default for OutputModes {
    fn default() -> Self {
        Self(Self::OPOST.0)
    }
}

/// Control modes
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct ControlModes(u32);

impl ControlModes {
    pub const CSIZE:  Self = Self(0b11 << 4); // Character size mask
    pub const CS5:    Self = Self(0b00 << 4); // 5 bits
    pub const CS6:    Self = Self(0b01 << 4); // 6 bits
    pub const CS7:    Self = Self(0b10 << 4); // 7 bits
    pub const CS8:    Self = Self(0b11 << 4); // 8 bits
    pub const CSTOPB: Self = Self(1 << 3);   // Two stop bits
    pub const CREAD:  Self = Self(1 << 2);   // Enable receiver
    pub const PARENB: Self = Self(1 << 1);   // Parity enable
    pub const PARODD: Self = Self(1 << 0);   // Odd parity
    pub const HUPCL:  Self = Self(1 << 6);   // Hang up on close
    pub const CLOCAL: Self = Self(1 << 7);   // Ignore modem control lines
    pub const CCTS_OFLOW: Self = Self(1 << 18); // CTS flow control
    pub const CCTS_OFLOW: Self = Self(1 << 19); // RTS flow control

    pub fn set_raw(&mut self) {
        self.0 &= Self::CREAD.0 | Self::CS8.0;
    }
}

impl Default for ControlModes {
    fn default() -> Self {
        Self(Self::CREAD.0 | Self::CS8.0)
    }
}

/// Local modes
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct LocalModes(u32);

impl LocalModes {
    pub const ISIG:   Self = Self(1 << 0);   // Enable signals
    pub const ICANON: Self = Self(1 << 1);   // Canonical input
    pub const XCASE:  Self = Self(1 << 2);   // Uppercase emulation
    pub const ECHO:   Self = Self(1 << 3);   // Echo input
    pub const ECHOE:  Self = Self(1 << 4);   // Echo erase as backspace-space-backspace
    pub const ECHOK:  Self = Self(1 << 5);   // Echo KILL
    pub const ECHONL: Self = Self(1 << 6);   // Echo NL
    pub const NOFLSH: Self = Self(1 << 7);   // Disable flush on signal
    pub const TOSTOP: Self = Self(1 << 8);   // SIGTTOU on background output
    pub const IEXTEN: Self = Self(1 << 15);  // Enable extended input processing
    pub const ECHOCTL: Self = Self(1 << 13); // Echo control chars as ^X
    pub const ECHOKE: Self = Self(1 << 14);  // Echo KILL with erase line

    pub fn set_raw(&mut self) {
        self.0 = Self::ISIG.0 | Self::IEXTEN.0;
    }
}

impl Default for LocalModes {
    fn default() -> Self {
        Self(Self::ICANON.0 | Self::ECHO.0 | Self::ECHOK.0 | 
             Self::ECHOE.0 | Self::ISIG.0 | Self::IEXTEN.0)
    }
}

/// Control characters (special keys)
pub const NCCS: usize = 32;

/// Terminal control characters structure
#[derive(Debug, Clone)]
#[repr(C)]
pub struct ControlChars {
    pub c_cc: [u8; NCCS],
}

impl Default for ControlChars {
    fn default() -> Self {
        let mut c_cc = [0u8; NCCS];
        c_cc[VINTR] = 3;      // Ctrl+C
        c_cc[VQUIT] = 28;     // Ctrl+\
        c_cc[VERASE] = 127;   // DEL/Backspace
        c_cc[VKILL] = 21;     // Ctrl+U
        c_cc[VEOF] = 4;       // Ctrl+D
        c_cc[VSTART] = 17;    // Ctrl+Q
        c_cc[VSTOP] = 19;     // Ctrl+S
        c_cc[VSUSP] = 26;     // Ctrl+Z
        c_cc[VEOL] = 0;       // Not set
        c_cc[VEOL2] = 0;      // Not set
        c_cc[VWERASE] = 23;   // Ctrl+W
        c_cc[VREPRINT] = 18;  // Ctrl+R
        c_cc[VDISCARD] = 15;  // Ctrl+O
        c_cc[VLNEXT] = 22;    // Ctrl+V
        c_cc[VMIN] = 1;       // Min characters for read
        c_cc[VTIME] = 0;      // Timeout (0 = blocking)
        Self { c_cc }
    }
}

/// Complete termios structure
#[derive(Debug, Clone, Default)]
pub struct Termios {
    pub input_modes: InputModes,
    pub output_modes: OutputModes,
    pub control_modes: ControlModes,
    pub local_modes: LocalModes,
    pub control_chars: ControlChars,
    pub line_speed: u32,   // Input speed
    pub output_speed: u32, // Output speed
}

impl Termios {
    /// Make the terminal raw
    pub fn make_raw(&mut self) {
        self.input_modes.set_raw();
        self.output_modes.set_raw();
        self.control_modes.set_raw();
        self.local_modes.set_raw();
        self.control_chars.c_cc[VMIN] = 0;
        self.control_chars.c_cc[VTIME] = 0;
    }

    /// Make the terminal canonical
    pub fn make_canonical(&mut self) {
        self.local_modes.0 = LocalModes::default().0;
        self.control_chars.c_cc[VMIN] = 1;
        self.control_chars.c_cc[VTIME] = 0;
    }
}

/// Terminal line discipline state
pub struct LineDiscipline {
    termios: Termios,
    input_buffer: RingBuffer<u8, 4096>,
    line_buffer: RingBuffer<u8, 4096>,
    echo_buffer: RingBuffer<u8, 4096>,
    cursor_pos: usize,
    line_len: usize,
    signal_pending: Option<Signal>,
    pub read_count: AtomicUsize,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Signal {
    SigInt,   // SIGINT - Ctrl+C
    SigQuit,  // SIGQUIT - Ctrl+\
    SigTSTP,  // SIGTSTP - Ctrl+Z
    SigCont,  // SIGCONT
    SigWinch, // SIGWINCH - window size change
}

impl Default for LineDiscipline {
    fn default() -> Self {
        Self {
            termios: Termios::default(),
            input_buffer: RingBuffer::new(),
            line_buffer: RingBuffer::new(),
            echo_buffer: RingBuffer::new(),
            cursor_pos: 0,
            line_len: 0,
            signal_pending: None,
            read_count: AtomicUsize::new(0),
        }
    }
}

impl LineDiscipline {
    /// Get current termios settings
    pub fn termios(&self) -> &Termios {
        &self.termios
    }

    /// Get mutable termios settings
    pub fn termios_mut(&mut self) -> &mut Termios {
        &mut self.termios
    }

    /// Set raw mode
    pub fn set_raw(&mut self) {
        self.termios.make_raw();
    }

    /// Set canonical mode
    pub fn set_canonical(&mut self) {
        self.termios.make_canonical();
    }

    /// Check if in canonical mode
    pub fn is_canonical(&self) -> bool {
        self.termios.local_modes.0 & LocalModes::ICANON.0 != 0
    }

    /// Check if echo is enabled
    pub fn is_echo(&self) -> bool {
        self.termios.local_modes.0 & LocalModes::ECHO.0 != 0
    }

    /// Check if signals are enabled
    pub fn is_signals_enabled(&self) -> bool {
        self.termios.local_modes.0 & LocalModes::ISIG.0 != 0
    }

    /// Process an incoming character from the hardware
    pub fn receive_byte(&mut self, byte: u8) {
        let c_cc = &self.termios.control_chars.c_cc;

        // Check for special characters
        if byte == c_cc[VINTR] && self.is_signals_enabled() {
            self.signal_pending = Some(Signal::SigInt);
            return;
        }
        if byte == c_cc[VQUIT] && self.is_signals_enabled() {
            self.signal_pending = Some(Signal::SigQuit);
            return;
        }
        if byte == c_cc[VSUSP] && self.is_signals_enabled() {
            self.signal_pending = Some(Signal::SigTSTP);
            return;
        }

        // Check if we have a pending signal
        if self.signal_pending.is_some() {
            return;
        }

        // Process based on mode
        if self.is_canonical() {
            self.process_canonical(byte, c_cc);
        } else {
            self.process_raw(byte, c_cc);
        }
    }

    /// Process input in canonical (line-oriented) mode
    fn process_canonical(&mut self, byte: u8, c_cc: &[u8; NCCS]) {
        match byte {
            _ if byte == c_cc[VERASE] => {
                // Erase character
                if self.line_len > 0 && self.cursor_pos > 0 {
                    self.cursor_pos -= 1;
                    self.line_len -= 1;

                    // Shift remaining characters
                    for i in self.cursor_pos..self.line_len {
                        self.line_buffer.buffer[i] = self.line_buffer.buffer[i + 1];
                    }

                    if self.is_echo() {
                        self.echo_buffer.push(b'\x08'); // Move back
                        self.echo_buffer.push(b' ');   // Erase char
                        self.echo_buffer.push(b'\x08'); // Move back
                        if self.termios.local_modes.0 & LocalModes::ECHOE.0 != 0 {
                            self.echo_buffer.push(b' ');
                            self.echo_buffer.push(b'\x08');
                            self.echo_buffer.push(b'\x08');
                        }
                    }
                }
            }
            _ if byte == c_cc[VKILL] => {
                // Kill entire line
                if self.is_echo() {
                    for _ in 0..self.cursor_pos {
                        self.echo_buffer.push(b'\x08');
                        self.echo_buffer.push(b' ');
                        self.echo_buffer.push(b'\x08');
                    }
                    if self.termios.local_modes.0 & LocalModes::ECHOK.0 != 0 {
                        self.echo_buffer.push(b'\n');
                    }
                }
                self.cursor_pos = 0;
                self.line_len = 0;
            }
            _ if byte == c_cc[VEOF] => {
                // EOF - return whatever we have (even empty)
                self.finalize_line();
                return;
            }
            _ if byte == c_cc[VWERASE] && self.is_signals_enabled() => {
                // Word erase
                while self.cursor_pos > 0 && 
                      core::str::from_utf8(&[self.line_buffer.buffer[self.cursor_pos - 1]])
                      .map(|c| c.chars().next().unwrap_or(' ').is_whitespace()).unwrap_or(true) 
                {
                    self.cursor_pos -= 1;
                    self.line_len -= 1;
                    if self.is_echo() {
                        self.echo_buffer.push(b'\x08');
                        self.echo_buffer.push(b' ');
                        self.echo_buffer.push(b'\x08');
                    }
                }
                while self.cursor_pos > 0 {
                    let ch = self.line_buffer.buffer[self.cursor_pos - 1];
                    if core::str::from_utf8(&[ch])
                        .map(|c| c.chars().next().unwrap_or(' ').is_whitespace()).unwrap_or(true) 
                    {
                        break;
                    }
                    self.cursor_pos -= 1;
                    self.line_len -= 1;
                    if self.is_echo() {
                        self.echo_buffer.push(b'\x08');
                        self.echo_buffer.push(b' ');
                        self.echo_buffer.push(b'\x08');
                    }
                }
            }
            b'\r' | b'\n' => {
                // Newline
                if self.termios.input_modes.0 & InputModes::ICRNL.0 != 0 && byte == b'\r' {
                    // Map CR to NL
                } else if self.termios.input_modes.0 & InputModes::INLCR.0 != 0 && byte == b'\n' {
                    // Map NL to CR
                }
                if self.is_echo() {
                    self.echo_buffer.push(b'\n');
                }
                self.finalize_line();
                return;
            }
            b'\n' => {
                if self.is_echo() {
                    self.echo_buffer.push(b'\n');
                }
                self.finalize_line();
                return;
            }
            _ if byte < 32 => {
                // Control character
                if self.is_echo() {
                    self.echo_buffer.push(b'^');
                    self.echo_buffer.push(byte + 64);
                }
                if byte == c_cc[VSTART] && self.is_signals_enabled() {
                    // Resume output - handled at higher level
                }
                if byte == c_cc[VSTOP] && self.is_signals_enabled() {
                    // Stop output - handled at higher level
                }
            }
            _ => {
                // Regular character
                if self.line_len < self.line_buffer.capacity() - 1 {
                    // Insert at cursor position
                    for i in (self.cursor_pos..self.line_len).rev() {
                        self.line_buffer.buffer[i + 1] = self.line_buffer.buffer[i];
                    }
                    self.line_buffer.buffer[self.cursor_pos] = byte;
                    self.cursor_pos += 1;
                    self.line_len += 1;

                    if self.is_echo() {
                        if self.cursor_pos == self.line_len {
                            // Appending at end
                            self.echo_buffer.push(byte);
                        } else {
                            // Inserting in middle - need to redraw rest of line
                            for i in self.cursor_pos..self.line_len {
                                self.echo_buffer.push(self.line_buffer.buffer[i]);
                            }
                            for _ in 0..self.line_len - self.cursor_pos {
                                self.echo_buffer.push(b'\x08');
                            }
                        }
                    }
                }
            }
        }
    }

    /// Process input in raw mode
    fn process_raw(&mut self, byte: u8, c_cc: &[u8; NCCS]) {
        // In raw mode, pass through directly
        self.line_buffer.push(byte).ok();
        if self.is_echo() {
            self.echo_buffer.push(byte).ok();
        }
    }

    /// Finalize the current line
    fn finalize_line(&mut self) {
        self.line_buffer.push(b'\0').ok(); // Null terminate
        self.read_count.fetch_add(self.line_len, Ordering::SeqCst);
        self.cursor_pos = 0;
        self.line_len = 0;
    }

    /// Check if a signal is pending
    pub fn signal_pending(&mut self) -> Option<Signal> {
        self.signal_pending.take()
    }

    /// Get bytes ready for reading
    pub fn read(&mut self, buffer: &mut [u8]) -> usize {
        let mut count = 0;
        while count < buffer.len() {
            if let Some(byte) = self.line_buffer.pop() {
                buffer[count] = byte;
                count += 1;
            } else {
                break;
            }
        }
        count
    }

    /// Get echo bytes ready
    pub fn read_echo(&mut self, buffer: &mut [u8]) -> usize {
        let mut count = 0;
        while count < buffer.len() {
            if let Some(byte) = self.echo_buffer.pop() {
                buffer[count] = byte;
                count += 1;
            } else {
                break;
            }
        }
        count
    }

    /// Write bytes to be echoed
    pub fn write_echo(&mut self, data: &[u8]) {
        for &byte in data {
            self.echo_buffer.push(byte).ok();
        }
    }

    /// Check if there's data available to read
    pub fn has_data(&self) -> bool {
        !self.line_buffer.is_empty()
    }

    /// Get available bytes count
    pub fn available(&self) -> usize {
        self.line_buffer.len()
    }

    /// Set line speed
    pub fn set_line_speed(&mut self, speed: u32) {
        self.termios.line_speed = speed;
    }

    /// Set output speed
    pub fn set_output_speed(&mut self, speed: u32) {
        self.termios.output_speed = speed;
    }

    /// Get line speed
    pub fn line_speed(&self) -> u32 {
        self.termios.line_speed
    }

    /// Get output speed
    pub fn output_speed(&self) -> u32 {
        self.termios.output_speed
    }
}

/// Ring buffer for line discipline
#[derive(Debug)]
pub struct RingBuffer<T, const N: usize> {
    buffer: [T; N],
    head: usize,
    tail: usize,
    count: usize,
}

impl<T: Copy + Default, const N: usize> RingBuffer<T, N> {
    pub const fn new() -> Self {
        Self {
            buffer: [T::default(); N],
            head: 0,
            tail: 0,
            count: 0,
        }
    }

    pub fn clear(&mut self) {
        self.head = 0;
        self.tail = 0;
        self.count = 0;
    }

    pub fn push(&mut self, item: T) -> Result<(), ()> {
        if self.count >= N {
            return Err(());
        }
        self.buffer[self.tail] = item;
        self.tail = (self.tail + 1) % N;
        self.count += 1;
        Ok(())
    }

    pub fn pop(&mut self) -> Option<T> {
        if self.count == 0 {
            return None;
        }
        let item = self.buffer[self.head];
        self.head = (self.head + 1) % N;
        self.count -= 1;
        Some(item)
    }

    pub fn len(&self) -> usize {
        self.count
    }

    pub fn is_empty(&self) -> bool {
        self.count == 0
    }

    pub fn is_full(&self) -> bool {
        self.count >= N
    }

    pub fn capacity(&self) -> usize {
        N
    }
}

use core::sync::atomic::Ordering;
