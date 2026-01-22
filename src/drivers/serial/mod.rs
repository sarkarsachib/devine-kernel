//! Serial driver module exports
//!
//! UART driver implementations for different platforms.

pub mod uart16550;
pub mod pl011;

pub use self::uart16550::{
    Uart16550, UartConfig, Parity, FlowControl, FifoTrigger, UartError, UartStats, 
    RingBuffer, MAX_BAUD, DEFAULT_BAUD
};
pub use self::pl011::{Pl011Uart, Pl011Config};

pub use self::uart16550::SERIAL1;
