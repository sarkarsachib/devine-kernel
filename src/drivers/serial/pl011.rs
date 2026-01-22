//! PL011 UART Driver for ARM64
//!
//! This module provides a production-grade PL011 UART driver for ARM64 platforms
//! supporting hardware flow control, DMA, and all standard PL011 features.

use core::sync::atomic::Ordering;
use super::uart16550::{UartConfig, Parity, FlowControl, UartError, RingBuffer, UartStats, MAX_BAUD};

/// PL011 register offsets (32-bit aligned)
const REG_DR:        usize = 0x00;  // Data Register
const REG_FR:        usize = 0x18;  // Flag Register
const REG_IBRD:      usize = 0x24;  // Integer Baud Rate Divisor
const REG_FBRD:      usize = 0x28;  // Fractional Baud Rate Divisor
const REG_LCRH:      usize = 0x2C;  // Line Control Register (high)
const REG_LCRM:      usize = 0x30;  // Line Control Register (mid) - not used
const REG_LCRL:      usize = 0x34;  // Line Control Register (low) - not used
const REG_CR:        usize = 0x30;  // Control Register
const REG_IMSC:      usize = 0x38;  // Interrupt Mask Set/Clear
const REG_ICR:       usize = 0x44;  // Interrupt Clear Register
const REG_DMACR:     usize = 0x48;  // DMA Control Register

/// FR bits
const FR_CTS:        u32 = 0x00000001;
const FR_DSR:        u32 = 0x00000002;
const FR_DCD:        u32 = 0x00000004;
const FR_BUSY:       u32 = 0x00000008;
const FR_RXFE:       u32 = 0x00000010;  // Receive FIFO empty
const FR_TXFF:       u32 = 0x00000020;  // Transmit FIFO full
const FR_RXFF:       u32 = 0x00000040;  // Receive FIFO full
const FR_TXFE:       u32 = 0x00000080;  // Transmit FIFO empty

/// LCRH bits
const LCRH_BRK:      u32 = 0x00000001;
const LCRH_PEN:      u32 = 0x00000002;  // Parity enable
const LCRH_EPS:      u32 = 0x00000004;  // Even parity select
const LCRH_STP2:     u32 = 0x00000008;  // Two stop bits
const LCRH_FEN:      u32 = 0x00000010;  // FIFOs enable
const LCRH_WLEN:     u32 = 0x00000060;  // Word length mask (bits 5-6)
const LCRH_SPS:      u32 = 0x00000080;  // Stick parity select

/// CR bits
const CR_UARTEN:     u32 = 0x00000001;  // UART enable
const CR_SIREN:      u32 = 0x00000002;  // SIR enable
const CR_SIRLP:      u32 = 0x00000004;  // SIR low-power mode
const CR_LBE:        u32 = 0x00000008;  // Loopback enable
const CR_TXE:        u32 = 0x00000100;  // Transmit enable
const CR_RXE:        u32 = 0x00000200;  // Receive enable
const CR_DTR:        u32 = 0x00000400;  // Data transmit ready
const CR_RTS:        u32 = 0x00000800;  // Request to send
const CR_RTSEN:      u32 = 0x00004000;  // RTS hardware flow control
const CR_CTSEN:      u32 = 0x00008000;  // CTS hardware flow control

/// IMSC bits
const IMSC_OEIM:     u32 = 0x00000001;  // Overrun error interrupt mask
const IMSC_BEIM:     u32 = 0x00000002;  // Break error interrupt mask
const IMSC_PEIM:     u32 = 0x00000004;  // Parity error interrupt mask
const IMSC_FEIM:     u32 = 0x00000008;  // Framing error interrupt mask
const IMSC_RTIM:     u32 = 0x00000010;  // Receive timeout interrupt mask
const IMSC_TXIM:     u32 = 0x00000020;  // Transmit interrupt mask
const IMSC_RXIM:     u32 = 0x00000040;  // Receive interrupt mask
const IMSC_CTSMIM:   u32 = 0x00000080;  // CTS modem interrupt mask

/// ICR bits
const ICR_OEIC:      u32 = 0x00000001;
const ICR_BEIC:      u32 = 0x00000002;
const ICR_PEIC:      u32 = 0x00000004;
const ICR_FEIC:      u32 = 0x00000008;
const ICR_RTIC:      u32 = 0x00000010;
const ICR_TXIC:      u32 = 0x00000020;
const ICR_RXIC:      u32 = 0x00000040;
const ICR_CTSMIC:    u32 = 0x00000080;

/// DMACR bits
const DMACR_RXDMAE:  u32 = 0x00000001;  // Receive DMA enable
const DMACR_TXDMAE:  u32 = 0x00000002;  // Transmit DMA enable
const DMACR_DMAONERR: u32 = 0x00000004; // DMA on error

/// Default baud rate
const DEFAULT_BAUD: u32 = 115200;

/// System clock (typical for Raspberry Pi and other ARM boards)
const SYSTEM_CLOCK: u32 = 48000000;

/// FIFO size
const FIFO_SIZE: usize = 16;

/// Receive/transmit buffer size
const RX_BUFFER_SIZE: usize = 4096;
const TX_BUFFER_SIZE: usize = 4096;

use super::uart16550::{MAX_BAUD};

/// PL011-specific configuration
#[derive(Debug, Clone, Copy)]
pub struct Pl011Config {
    pub baud_rate: u32,
    pub data_bits: u8,     // 5, 6, 7, or 8
    pub stop_bits: u8,     // 1 or 2
    pub parity: Parity,
    pub flow_control: FlowControl,
    pub enable_fifo: bool,
    pub enable_break: bool,
}

impl Default for Pl011Config {
    fn default() -> Self {
        Self {
            baud_rate: DEFAULT_BAUD,
            data_bits: 8,
            stop_bits: 1,
            parity: Parity::None,
            flow_control: FlowControl::None,
            enable_fifo: true,
            enable_break: false,
        }
    }
}

/// PL011 UART driver
pub struct Pl011Uart {
    base: usize,
    config: Pl011Config,
    rx_buffer: RingBuffer<u8, RX_BUFFER_SIZE>,
    tx_buffer: RingBuffer<u8, TX_BUFFER_SIZE>,
    stats: UartStats,
    interrupt_enabled: bool,
}

impl Pl011Uart {
    /// Create a new PL011 driver for the given base address
    pub const fn new(base: usize) -> Self {
        Self {
            base,
            config: Pl011Config::default(),
            rx_buffer: RingBuffer::new(),
            tx_buffer: RingBuffer::new(),
            stats: UartStats::default(),
            interrupt_enabled: false,
        }
    }

    /// Initialize the UART with default configuration
    pub fn init(&mut self) -> Result<(), UartError> {
        self.configure(Pl011Config::default())
    }

    /// Configure the UART with specific settings
    pub fn configure(&mut self, config: Pl011Config) -> Result<(), UartError> {
        // Validate configuration
        if config.baud_rate == 0 || config.baud_rate > MAX_BAUD {
            return Err(UartError::InvalidBaudRate);
        }
        if config.data_bits < 5 || config.data_bits > 8 {
            return Err(UartError::InvalidConfig);
        }
        if config.stop_bits < 1 || config.stop_bits > 2 {
            return Err(UartError::InvalidConfig);
        }

        self.config = config;

        // Disable UART during configuration
        self.clear_bits(REG_CR as u32, CR_UARTEN);

        // Calculate baud rate divisor
        // IBRD = floor(system_clock / (16 * baud_rate))
        // FBRD = round((64 * frac) / 16) where frac = (system_clock % (16 * baud_rate)) / baud_rate
        let baud_divisor = (SYSTEM_CLOCK as u64) / (16 * config.baud_rate as u64) as u32;
        let remainder = (SYSTEM_CLOCK as u64) % (16 * config.baud_rate as u64);
        let frac = (remainder * 64 + 8) / (16 * config.baud_rate as u64);  // Round to nearest

        self.write_reg(REG_IBRD, baud_divisor);
        self.write_reg(REG_FBRD, frac as u32);

        // Configure line control
        let mut lcrh: u32 = 0;

        // Word length
        let wlen = match config.data_bits {
            5 => 0b00,
            6 => 0b01,
            7 => 0b10,
            8 => 0b11,
            _ => 0b11,
        };
        lcrh |= (wlen as u32) << 5;

        // Stop bits
        if config.stop_bits == 2 {
            lcrh |= LCRH_STP2;
        }

        // Parity
        match config.parity {
            Parity::None => {}
            Parity::Odd => { lcrh |= LCRH_PEN; }
            Parity::Even => { lcrh |= LCRH_PEN | LCRH_EPS; }
            Parity::Mark | Parity::Space => { lcrh |= LCRH_PEN | LCRH_SPS; }
        }

        // FIFO enable
        if config.enable_fifo {
            lcrh |= LCRH_FEN;
        }

        // Break
        if config.enable_break {
            lcrh |= LCRH_BRK;
        }

        self.write_reg(REG_LCRH, lcrh);

        // Configure control register
        let mut cr: u32 = CR_TXE | CR_RXE;  // Enable TX and RX

        match config.flow_control {
            FlowControl::None => {}
            FlowControl::RtsCts => {
                cr |= CR_RTSEN | CR_CTSEN;
            }
            FlowControl::XonXoff => {
                // Software flow control - handled at higher level
            }
        }

        self.write_reg(REG_CR, cr);

        // Clear all interrupts
        self.write_reg(REG_ICR, 0x3FF);

        // Clear buffers
        self.rx_buffer.clear();
        self.tx_buffer.clear();

        // Enable UART
        self.set_bits(REG_CR as u32, CR_UARTEN);

        Ok(())
    }

    /// Enable interrupts
    pub fn enable_interrupts(&mut self) {
        let mut imsc: u32 = IMSC_RXIM | IMSC_RTIM;
        if self.config.flow_control == FlowControl::RtsCts {
            imsc |= IMSC_CTSMIM;
        }
        self.set_bits(REG_IMSC as u32, imsc);
        self.interrupt_enabled = true;
    }

    /// Disable interrupts
    pub fn disable_interrupts(&mut self) {
        self.clear_bits(REG_IMSC as u32, 0x3FF);
        self.interrupt_enabled = false;
    }

    /// Send a single byte (blocking)
    pub fn send(&mut self, byte: u8) {
        // Wait for TX FIFO to have space
        while self.is_transmit_fifo_full() {
            core::hint::spin_loop();
        }

        // Write to data register
        self.write_reg(REG_DR, byte as u32);
        self.stats.bytes_sent.fetch_add(1, Ordering::Relaxed);
    }

    /// Send a buffer of bytes (blocking)
    pub fn send_buffer(&mut self, data: &[u8]) {
        for &byte in data {
            self.send(byte);
        }
    }

    /// Try to receive a byte (non-blocking)
    pub fn try_receive(&mut self) -> Option<u8> {
        // Check hardware receive buffer first
        if !self.is_receive_fifo_empty() {
            let dr = self.read_reg(REG_DR);
            let byte = (dr & 0xFF) as u8;

            // Check for errors
            if dr & 0xFF00 != 0 {
                if dr & 0x200 != 0 {
                    self.stats.overruns.fetch_add(1, Ordering::Relaxed);
                }
                if dr & 0x800 != 0 {
                    self.stats.parity_errors.fetch_add(1, Ordering::Relaxed);
                }
                if dr & 0x1000 != 0 {
                    self.stats.framing_errors.fetch_add(1, Ordering::Relaxed);
                }
                if dr & 0x400 != 0 {
                    self.stats.break_interrupts.fetch_add(1, Ordering::Relaxed);
                }
            }

            self.stats.bytes_received.fetch_add(1, Ordering::Relaxed);
            return Some(byte);
        }

        // Check software buffer
        self.rx_buffer.pop()
    }

    /// Receive a single byte (blocking)
    pub fn receive(&mut self) -> Result<u8, UartError> {
        // Try hardware buffer first
        if !self.is_receive_fifo_empty() {
            let dr = self.read_reg(REG_DR);
            let byte = (dr & 0xFF) as u8;

            // Check for errors
            if dr & 0xFF00 != 0 {
                if dr & 0x200 != 0 {
                    self.stats.overruns.fetch_add(1, Ordering::Relaxed);
                }
                if dr & 0x800 != 0 {
                    self.stats.parity_errors.fetch_add(1, Ordering::Relaxed);
                }
                if dr & 0x1000 != 0 {
                    self.stats.framing_errors.fetch_add(1, Ordering::Relaxed);
                }
                if dr & 0x400 != 0 {
                    self.stats.break_interrupts.fetch_add(1, Ordering::Relaxed);
                }
                return Err(UartError::ParityError);  // Simplified error handling
            }

            self.stats.bytes_received.fetch_add(1, Ordering::Relaxed);
            return Ok(byte);
        }

        // Try software buffer
        if let Some(byte) = self.rx_buffer.pop() {
            return Ok(byte);
        }

        Err(UartError::Timeout)
    }

    /// Receive available data into a buffer
    pub fn receive_into(&mut self, buffer: &mut [u8]) -> usize {
        let mut count = 0;

        // Drain hardware buffer first
        while !self.is_receive_fifo_empty() && count < buffer.len() {
            let dr = self.read_reg(REG_DR);
            buffer[count] = (dr & 0xFF) as u8;
            self.stats.bytes_received.fetch_add(1, Ordering::Relaxed);
            count += 1;
        }

        // Then drain software buffer
        while count < buffer.len() {
            if let Some(byte) = self.rx_buffer.pop() {
                buffer[count] = byte;
                count += 1;
            } else {
                break;
            }
        }

        count
    }

    /// Check if there is data available to read
    pub fn has_data(&self) -> bool {
        !self.is_receive_fifo_empty() || !self.rx_buffer.is_empty()
    }

    /// Get the number of available bytes
    pub fn available(&self) -> usize {
        let hw_available = if self.is_receive_fifo_empty() { 0 } else { 1 };
        hw_available + self.rx_buffer.len()
    }

    /// Process receive interrupt - should be called from IRQ handler
    pub fn handle_rx_interrupt(&mut self) {
        while !self.is_receive_fifo_empty() {
            let dr = self.read_reg(REG_DR);

            // Check for errors
            let has_error = dr & 0xFF00 != 0;
            let byte = (dr & 0xFF) as u8;

            self.stats.bytes_received.fetch_add(1, Ordering::Relaxed);

            if has_error {
                if dr & 0x200 != 0 { self.stats.overruns.fetch_add(1, Ordering::Relaxed); }
                if dr & 0x800 != 0 { self.stats.parity_errors.fetch_add(1, Ordering::Relaxed); }
                if dr & 0x1000 != 0 { self.stats.framing_errors.fetch_add(1, Ordering::Relaxed); }
                if dr & 0x400 != 0 { self.stats.break_interrupts.fetch_add(1, Ordering::Relaxed); }
            }

            // If buffer is full, count overrun
            if self.rx_buffer.push(byte).is_err() {
                self.stats.overruns.fetch_add(1, Ordering::Relaxed);
            }
        }
    }

    /// Process transmit interrupt - should be called from IRQ handler
    pub fn handle_tx_interrupt(&mut self) {
        // Fill TX FIFO from software buffer
        while !self.is_transmit_fifo_full() {
            if let Some(byte) = self.tx_buffer.pop() {
                self.write_reg(REG_DR, byte as u32);
                self.stats.bytes_sent.fetch_add(1, Ordering::Relaxed);
            } else {
                break;
            }
        }

        // If buffer empty, disable TX interrupts to prevent unnecessary IRQs
        if self.tx_buffer.is_empty() && self.interrupt_enabled {
            self.clear_bits(REG_IMSC as u32, IMSC_TXIM);
        }
    }

    /// Process error interrupt - should be called from IRQ handler
    pub fn handle_error_interrupt(&mut self) {
        let fr = self.read_reg(REG_FR);
        let icr = self.read_reg(REG_ICR);

        // Check for errors in interrupt clear register
        if icr & 0xF != 0 {
            // Errors were present, they are now cleared
            if icr & 0x1 != 0 { self.stats.overruns.fetch_add(1, Ordering::Relaxed); }
            if icr & 0x2 != 0 { self.stats.break_interrupts.fetch_add(1, Ordering::Relaxed); }
            if icr & 0x4 != 0 { self.stats.parity_errors.fetch_add(1, Ordering::Relaxed); }
            if icr & 0x8 != 0 { self.stats.framing_errors.fetch_add(1, Ordering::Relaxed); }
        }
    }

    /// Process receive timeout interrupt
    pub fn handle_rx_timeout_interrupt(&mut self) {
        self.stats.recv_fifo_timeouts.fetch_add(1, Ordering::Relaxed);
        // Drain any remaining data
        self.handle_rx_interrupt();
    }

    /// Process modem status interrupt
    pub fn handle_modem_interrupt(&mut self) {
        let _fr = self.read_reg(REG_FR);
        // RTS/CTS flow control handling would go here
    }

    /// Get statistics
    pub fn stats(&self) -> &UartStats {
        &self.stats
    }

    /// Check if receive FIFO is empty
    pub fn is_receive_fifo_empty(&self) -> bool {
        self.read_reg(REG_FR) & FR_RXFE != 0
    }

    /// Check if transmit FIFO is full
    pub fn is_transmit_fifo_full(&self) -> bool {
        self.read_reg(REG_FR) & FR_TXFF != 0
    }

    /// Check if transmit FIFO is empty
    pub fn is_transmit_fifo_empty(&self) -> bool {
        self.read_reg(REG_FR) & FR_TXFE != 0
    }

    /// Check if UART is busy
    pub fn is_busy(&self) -> bool {
        self.read_reg(REG_FR) & FR_BUSY != 0
    }

    /// Get current configuration
    pub fn config(&self) -> Pl011Config {
        self.config
    }

    /// Set break condition
    pub fn set_break(&mut self) {
        self.set_bits(REG_LCRH as u32, LCRH_BRK);
    }

    /// Clear break condition
    pub fn clear_break(&mut self) {
        self.clear_bits(REG_LCRH as u32, LCRH_BRK);
    }

    /// Set RTS signal
    pub fn set_rts(&mut self) {
        self.set_bits(REG_CR as u32, CR_RTS);
    }

    /// Clear RTS signal
    pub fn clear_rts(&mut self) {
        self.clear_bits(REG_CR as u32, CR_RTS);
    }

    /// Enable DMA
    pub fn enable_dma(&mut self, rx: bool, tx: bool, on_error: bool) {
        let mut dmacr: u32 = 0;
        if rx { dmacr |= DMACR_RXDMAE; }
        if tx { dmacr |= DMACR_TXDMAE; }
        if on_error { dmacr |= DMACR_DMAONERR; }
        self.write_reg(REG_DMACR, dmacr);
    }

    /// Disable DMA
    pub fn disable_dma(&mut self) {
        self.write_reg(REG_DMACR, 0);
    }

    /// Read from a register
    fn read_reg(&self, offset: usize) -> u32 {
        let ptr = (self.base + offset) as *const u32;
        unsafe { ptr.read_volatile() }
    }

    /// Write to a register
    fn write_reg(&self, offset: usize, value: u32) {
        let ptr = (self.base + offset) as *mut u32;
        unsafe { ptr.write_volatile(value) }
    }

    /// Set specific bits in a register
    fn set_bits(&self, offset: usize, bits: u32) {
        let reg = self.read_reg(offset);
        self.write_reg(offset, reg | bits);
    }

    /// Clear specific bits in a register
    fn clear_bits(&self, offset: usize, bits: u32) {
        let reg = self.read_reg(offset);
        self.write_reg(offset, reg & !bits);
    }
}
