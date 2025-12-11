#![cfg_attr(not(test), no_std)]

#[cfg(not(test))]
extern crate alloc;

pub mod memory;
pub mod process;
pub mod arch;
pub mod syscall;
pub mod security;
pub mod userspace;
pub mod hwinfo;
pub mod x86_64;
pub mod arm64;

#[cfg(not(test))]
use core::panic::PanicInfo;

#[cfg(not(test))]
#[panic_handler]
fn panic(_info: &PanicInfo) -> ! {
    loop {}
}

#[cfg(test)]
mod tests {
    #[test]
    fn test_basic() {
        assert_eq!(2 + 2, 4);
    }
}
